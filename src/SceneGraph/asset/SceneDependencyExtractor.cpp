#include <hgl/graph/asset/SceneDependencyExtractor.h>

#include <hgl/graph/mesh/ScenePackFormat.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/MaterialRecipeFile.h>
#include <hgl/log/Log.h>

using namespace hgl::asset;
using namespace hgl::graph::scenepack;

DEFINE_LOGGER_MODULE(SceneDep)
USE_MODULE_LOGGER(SceneDep)

namespace hgl::graph
{
    AnsiString JoinAssetRef(const AnsiString &base_dir, const AnsiString &name)
    {
        if(base_dir.IsEmpty())
            return name;

        int len = base_dir.Length();

        // 基准目录末尾的分隔符统一收成一个，避免 `dir//file`
        while(len > 0 && (base_dir[len - 1] == '/' || base_dir[len - 1] == '\\'))
            len--;

        if(len <= 0)
            return name;

        return base_dir.SubString(0, len) + AnsiString("/") + name;
    }

    namespace
    {
        /// `.material` 是 SCN2 的每材质二进制容器，语义上是「材质」而不是「配方」；
        /// 身份层与 `.recipe.toml` 合并（见 AssetLoadGraph.cpp 的后缀表注释），
        /// 但派发层要区分开 —— 一个交给材质装配，一个交给配方注册表。
        inline AssetDepKind KindFromRef(const AnsiString &ref, AssetType type)
        {
            const int len = ref.Length();

            if(len > 9 && ref[len - 9] == '.' && ref.SubString(len - 9, 9) == ".material")
                return AssetDepKind::Material;

            switch(type)
            {
                case AssetType::Geometry:        return AssetDepKind::Geometry;
                case AssetType::Texture:         return AssetDepKind::Texture;
                case AssetType::MaterialRecipe:  return AssetDepKind::MaterialRecipe;
                case AssetType::MaterialData:    return AssetDepKind::MaterialData;
                case AssetType::Scene:           return AssetDepKind::Scene;
                case AssetType::Primitive:       return AssetDepKind::Primitive;
                case AssetType::Environment:     return AssetDepKind::Environment;
                default:                         return AssetDepKind::Material;
            }
        }

        /// 把一条引用加成 `edges` 的一条边。引用无法规范化（类型不明）时返回 false。
        bool PushEdge(ValueArray<AssetDepEdge> &edges,
                      const AnsiString &base_dir, const AnsiString &name,
                      AssetType declared_type, AssetDepKind kind)
        {
            if(name.IsEmpty())
                return false;

            const AnsiString ref = JoinAssetRef(base_dir, name);

            AssetDepKey key;

            if(!NormalizeAssetRef(ref, declared_type, key))
            {
                FLogWarning(u8"依赖引用无法规范化，已忽略：%s", ref.c_str());
                return false;
            }

            AssetDepEdge edge;
            edge.target = key;
            edge.kind   = kind;

            edges.Add(edge);
            return true;
        }

        /**
         * `ABeautifulGame.0.Chessboard.material` → `Chessboard`
         *
         * 规则与 `LoadStaticMesh.cpp` 的 `MaterialNameFromPackFile` 完全一致：
         * 去 `.material` 后缀；`<Scene>.<数字>.` 前缀剥掉。材质名本身可以含点，
         * 所以不能简单取最后一段。
         */
        AnsiString MaterialNameFromPackFile(const AnsiString &file)
        {
            AnsiString s = file;

            const int len = s.Length();

            if(len > 9 && s[len - 9] == '.' && s.SubString(len - 9, 9) == ".material")
                s = s.SubString(0, len - 9);

            const int d1 = s.FindString(AnsiString("."), 0);

            if(d1 < 0)
                return s;

            const int d2 = s.FindString(AnsiString("."), d1 + 1);

            if(d2 < 0)
                return s.SubString(d1 + 1);

            bool numeric = d2 > d1 + 1;

            for(int i = d1 + 1; i < d2 && numeric; i++)
                numeric = (s[i] >= '0' && s[i] <= '9');

            return numeric ? s.SubString(d2 + 1) : s;
        }
    }//anonymous namespace

    bool ExtractSceneDependencies(const void *pack_header_raw, uint32_t header_len,
                                  const void *payload_raw,    uint32_t payload_len,
                                  const AnsiString &base_dir,
                                  const AnsiString &scene_ref,
                                  AssetLoadGraph &graph,
                                  int *out_edge_count,
                                  ManagedArray<SceneMaterialEntry> *out_materials,
                                  int *out_embedded_geometry)
    {
        if(out_edge_count)
            *out_edge_count = 0;

        if(out_materials)
            out_materials->Clear();

        if(out_embedded_geometry)
            *out_embedded_geometry = 0;

        ScenePackHeader h{};
        const SceneTableDesc *dir = nullptr;

        if(!ReadPayloadDirectory(pack_header_raw, header_len, payload_raw, payload_len, h, dir))
        {
            FLogError(u8"提取场景依赖失败：包头/表目录不合法");
            return false;
        }

        const uint8_t *payload = reinterpret_cast<const uint8_t *>(payload_raw);

        const auto table = [payload, &h, dir](SceneTableType type, const uint8_t *&ptr, uint32_t &size)
        {
            return TryGetTableRange(payload, h.payload_size, FindTableDesc(dir, h.dir_count, type), ptr, size);
        };

        ValueArray<AssetDepEdge> edges;

        // ── 1. 材质：MaterialTable 每行指向 StringPool 里的 `<Scene>.<idx>.<Name>.material`
        {
            const uint8_t *mat = nullptr;  uint32_t mat_size = 0;
            const uint8_t *pool = nullptr; uint32_t pool_size = 0;

            const bool has_mat  = table(SceneTableType::MaterialTable, mat, mat_size);
            const bool has_pool = table(SceneTableType::StringPool, pool, pool_size);

            if(has_mat != has_pool)
            {
                FLogError(u8"提取场景依赖失败：MaterialTable 与 StringPool 必须成对存在");
                return false;
            }

            if(has_mat)
            {
                if(mat_size % sizeof(PackedMaterial) != 0)
                {
                    FLogError(u8"提取场景依赖失败：MaterialTable 字节数不是整行");
                    return false;
                }

                const PackedMaterial *pm = reinterpret_cast<const PackedMaterial *>(mat);
                const uint32_t count = mat_size / static_cast<uint32_t>(sizeof(PackedMaterial));

                for(uint32_t i = 0; i < count; i++)
                {
                    if(pm[i].file_offset > pool_size
                       || pm[i].file_length > pool_size
                       || pm[i].file_offset + pm[i].file_length > pool_size)
                    {
                        FLogError(u8"提取场景依赖失败：材质文件名越出 StringPool");
                        return false;
                    }

                    const AnsiString file(reinterpret_cast<const char *>(pool + pm[i].file_offset),
                                          static_cast<int>(pm[i].file_length));

                    const AssetDepKind kind = KindFromRef(file, AssetType::MaterialRecipe);

                    if(PushEdge(edges, base_dir, file, AssetType::MaterialRecipe, kind)
                       && out_materials)
                    {
                        // `ManagedArray` 持有并拥有 `T*`，因此必须走 `Create()` 而不是
                        // 栈上构造 —— `Add` 只接受指针。
                        SceneMaterialEntry *entry = out_materials->Create();

                        entry->name = MaterialNameFromPackFile(file);

                        // 与 PushEdge 走同一套规范化，保证写进 out_materials 的身份
                        // 与边上的目标身份**逐字节一致**（否则配方边连不上材质节点）
                        if(!NormalizeAssetRef(JoinAssetRef(base_dir, file), AssetType::MaterialRecipe, entry->key))
                            out_materials->DeleteAt(out_materials->GetCount() - 1);
                    }
                }
            }
        }

        // ── 2. 几何：只有**没有** GeometryBlob/GeometryView 的早期 V2 包才引用外部文件
        {
            const uint8_t *prim = nullptr;     uint32_t prim_size = 0;
            const uint8_t *geo_view = nullptr; uint32_t geo_view_size = 0;
            const uint8_t *geo_blob = nullptr; uint32_t geo_blob_size = 0;

            const bool has_prim     = table(SceneTableType::PrimitiveTable, prim, prim_size);
            const bool has_geo_view = table(SceneTableType::GeometryViewTable, geo_view, geo_view_size);
            const bool has_geo_blob = table(SceneTableType::GeometryBlob, geo_blob, geo_blob_size);

            // 与 LoadStaticMesh 的判据保持一致：视图表与 blob 都在，几何就是内嵌的
            const bool geometry_embedded = has_geo_view && has_geo_blob
                                        && geo_view_size >= sizeof(PackedGeometryView)
                                        && geo_blob_size > 0;

            if(has_prim && !geometry_embedded)
            {
                const uint8_t *pool = nullptr; uint32_t pool_size = 0;

                if(!table(SceneTableType::StringPool, pool, pool_size))
                {
                    FLogError(u8"提取场景依赖失败：外部几何回退路径缺少 StringPool");
                    return false;
                }

                if(prim_size % sizeof(PackedPrimitive) != 0)
                {
                    FLogError(u8"提取场景依赖失败：PrimitiveTable 字节数不是整行");
                    return false;
                }

                const PackedPrimitive *pp = reinterpret_cast<const PackedPrimitive *>(prim);
                const uint32_t count = prim_size / static_cast<uint32_t>(sizeof(PackedPrimitive));

                for(uint32_t i = 0; i < count; i++)
                {
                    if(pp[i].geometry_index < 0)
                        continue;

                    if(pp[i].geometry_file_offset > pool_size
                       || pp[i].geometry_file_length > pool_size
                       || pp[i].geometry_file_offset + pp[i].geometry_file_length > pool_size)
                    {
                        FLogError(u8"提取场景依赖失败：几何文件名越出 StringPool");
                        return false;
                    }

                    // 同一个几何被多个图元引用是常态，交给 SetDependencies 去重
                    const AnsiString file(reinterpret_cast<const char *>(pool + pp[i].geometry_file_offset),
                                          static_cast<int>(pp[i].geometry_file_length));

                    PushEdge(edges, base_dir, file, AssetType::Geometry, AssetDepKind::Geometry);
                }
            }
            else if(geometry_embedded && out_embedded_geometry)
            {
                // 内嵌几何不出边，但要报份数：上层据此知道「几何=0」不是「没几何」
                *out_embedded_geometry =
                    static_cast<int>(geo_view_size / static_cast<uint32_t>(sizeof(PackedGeometryView)));
            }
        }

        AssetDepKey root;

        if(!NormalizeAssetRef(scene_ref, AssetType::Scene, root))
        {
            FLogError(u8"提取场景依赖失败：场景引用无法规范化 %s", scene_ref.c_str());
            return false;
        }

        const int written = graph.SetDependencies(root, edges.GetData(), edges.GetCount());

        if(out_edge_count)
            *out_edge_count = written;

        FLogInfo(u8"场景依赖已提取：%s 边=%d", root.MakeId().c_str(), written);

        return true;
    }

    bool ExtractRecipeDependencies(const char *content, int content_size,
                                   const AnsiString &base_dir,
                                   const AnsiString &recipe_ref,
                                   AssetLoadGraph &graph,
                                   int *out_edge_count)
    {
        if(out_edge_count)
            *out_edge_count = 0;

        if(!content || content_size <= 0)
        {
            FLogError(u8"提取配方依赖失败：内容为空 %s", recipe_ref.c_str());
            return false;
        }

        mtl::MaterialRecipeFileData data;
        mtl::MaterialRecipeSpec       spec;

        const mtl::MaterialRecipeFileParseResult result =
            mtl::ParseMaterialRecipeFile(content, content_size, data, &spec);

        if(result != mtl::MaterialRecipeFileParseResult::OK
           && result != mtl::MaterialRecipeFileParseResult::Skipped)
        {
            const char *detail = data.parse_error.detail.IsEmpty()
                               ? "-" : data.parse_error.detail.c_str();

            FLogError(u8"提取配方依赖失败：%s（%s，第 %d 行 %s）",
                      recipe_ref.c_str(),
                      mtl::GetMaterialRecipeFileParseResultName(result),
                      data.parse_error.line, detail);
            return false;
        }

        ValueArray<AssetDepEdge> edges;

        // `spec` 是字段契约的真源（差量配方也一定被填），`data.recipe` 只在无父配方时成型
        for(int i = 0; i < spec.textures.GetCount(); i++)
        {
            const mtl::RecipeTextureSpec *tex_ptr = spec.textures[i];

            if(!tex_ptr)
                continue;

            const mtl::RecipeTextureSpec &tex = *tex_ptr;

            if(!tex.resource_id.present || tex.resource_id.value.IsEmpty())
                continue;

            const AnsiString ref = tex.resource_id.value;

            // 形如 `texfile:` / `rtex:` / `texarray:` 的资源 ID 不是包内路径，不参与依赖图
            if(ref.Length() > 1 && ref[0] != '/' && ref[0] != '.')
            {
                bool has_colon = false;

                for(int c = 0; c < ref.Length(); c++)
                    if(ref[c] == ':')
                    {
                        has_colon = true;
                        break;
                    }

                if(has_colon)
                    continue;
            }

            PushEdge(edges, base_dir, ref, AssetType::Texture, AssetDepKind::Texture);
        }

        AssetDepKey root;

        if(!NormalizeAssetRef(recipe_ref, AssetType::MaterialRecipe, root))
        {
            FLogError(u8"提取配方依赖失败：配方引用无法规范化 %s", recipe_ref.c_str());
            return false;
        }

        const int written = graph.SetDependencies(root, edges.GetData(), edges.GetCount());

        if(out_edge_count)
            *out_edge_count = written;

        FLogInfo(u8"配方依赖已提取：%s 边=%d", root.MakeId().c_str(), written);

        return true;
    }
}//namespace hgl::graph
