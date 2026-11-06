#include <hgl/graph/asset/ModelLoadPlan.h>

#include <hgl/graph/asset/AssetSourceMount.h>
#include <hgl/graph/asset/ModelDependencyExtractor.h>
#include <hgl/graph/mesh/ModelPackFormat.h>
#include <hgl/asset/AssetSource.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/io/InputStream.h>
#include <hgl/io/MiniPack.h>
#include <hgl/utf.h>
#include <hgl/log/Log.h>

#include <cstring>

using namespace hgl::asset;
using namespace hgl::graph::modelpack;

DEFINE_LOGGER_MODULE(ModelPlan)
USE_MODULE_LOGGER(ModelPlan)

namespace hgl::graph
{
    namespace
    {
        constexpr const char *kRecipeSuffix    = ".recipe.toml";
        constexpr int         kRecipeSuffixLen = 12;

        /// `u8char` 在开 `HGL_SUPPORT_CHAR8_T` 时是 `char8_t`，与 `char` 没有隐式
        /// 转换；这里交换的本来就是同一段 UTF-8 字节。
        inline U8String ToU8(const AnsiString &str)
        {
            return U8String(reinterpret_cast<const u8char *>(str.c_str()), str.Length());
        }

        inline AnsiString ToAnsi(const U8String &str)
        {
            return AnsiString(reinterpret_cast<const char *>(str.c_str()), str.Length());
        }

        AnsiString AnsiFromOS(const os_char *str)
        {
            if(str == nullptr)
                return AnsiString();

            return ToAnsi(ToU8String(OSString(str)));
        }

        inline char LowerAscii(char c)
        {
            return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
        }

        bool EndsWithNoCase(const AnsiString &str, const char *suffix, int suffix_len)
        {
            const int len = str.Length();

            if(suffix_len <= 0 || len < suffix_len)
                return false;

            for(int i = 0; i < suffix_len; i++)
                if(LowerAscii(str[len - suffix_len + i]) != LowerAscii(suffix[i]))
                    return false;

            return true;
        }

        bool EqualNoCase(const AnsiString &a, const AnsiString &b)
        {
            const int len = a.Length();

            if(len != b.Length())
                return false;

            for(int i = 0; i < len; i++)
                if(LowerAscii(a[i]) != LowerAscii(b[i]))
                    return false;

            return true;
        }

        int LastSeparator(const AnsiString &str)
        {
            const int len = str.Length();
            int       res = -1;

            for(int i = 0; i < len; i++)
                if(str[i] == '/' || str[i] == '\\')
                    res = i;

            return res;
        }

        /// 取目录部分（不含末尾分隔符）。没有分隔符时返回空串。
        AnsiString DirNameOf(const AnsiString &path)
        {
            const int sep = LastSeparator(path);

            if(sep <= 0)
                return AnsiString();

            return path.SubString(0, sep);
        }

        AnsiString FileNameOf(const AnsiString &path)
        {
            const int sep = LastSeparator(path);

            if(sep < 0)
                return path;

            return path.SubString(sep + 1, path.Length() - sep - 1);
        }

        /**
         * `LoadScene.Chess.Chessboard.recipe.toml` → `Chessboard`
         *
         * 与 `LoadStaticMesh.cpp` 的 `RecipeMatchesMaterialName` 同口径：只看配方名
         * 的最后一段。配方前缀（`LoadScene.Chess.`）是场景/分类标签，不参与匹配。
         */
        AnsiString RecipeLastName(const AnsiString &path)
        {
            AnsiString name = FileNameOf(path);

            if(EndsWithNoCase(name, kRecipeSuffix, kRecipeSuffixLen))
                name = name.SubString(0, name.Length() - kRecipeSuffixLen);

            const int len = name.Length();
            int       dot = -1;

            for(int i = 0; i < len; i++)
                if(name[i] == '.')
                    dot = i;

            return (dot < 0) ? name : name.SubString(dot + 1, len - dot - 1);
        }

        /// 一个候选配方文件：`path` 是包内相对路径（依赖身份用），`open_ref` 是取流地址。
        struct RecipeFileRef
        {
            AnsiString path;
            AnsiString open_ref;
        };

        /**
         * 收集场景目录下的 `*.recipe.toml`。
         *
         * 为什么必须枚举而不是拼路径：模型包只给材质名（`Chessboard`），配方文件名
         * 却是 `LoadScene.Chess.Chessboard.recipe.toml` —— 前缀来自导出器，猜不出来。
         *
         * @return 收集到的数量；枚举能力不可用返回 -1（调用方降级为警告）
         */
        int CollectRecipeFiles(const asset::AssetURI &scene_uri,
                               const AnsiString &base_dir,
                               ManagedArray<RecipeFileRef> &out)
        {
            int count = 0;

            if(scene_uri.IsPack())
            {
                ManagedArray<asset::SourceEntry> entries;

                if(ListAssetDirectory(base_dir, entries, scene_uri.pack_name.c_str()) < 0)
                    return -1;

                for(int i = 0; i < entries.GetCount(); i++)
                {
                    const asset::SourceEntry *entry = entries[i];

                    if(entry == nullptr || entry->is_directory)
                        continue;

                    const AnsiString path = ToAnsi(entry->path);

                    if(!EndsWithNoCase(path, kRecipeSuffix, kRecipeSuffixLen))
                        continue;

                    RecipeFileRef *ref = out.Create();

                    ref->path     = path;
                    ref->open_ref = MakePackSegment(scene_uri.pack_name) + ':' + path;
                    count++;
                }

                return count;
            }

            // 裸物理路径：老示例仍这样传，退回直接扫磁盘（不走包裹索引）。
            ValueArray<filesystem::FileInfo> list;

            if(filesystem::GetFileInfoList(list, ToOSString(base_dir.c_str()), false, true, false) < 0)
                return -1;

            for(int i = 0; i < list.GetCount(); i++)
            {
                const filesystem::FileInfo &file = list[i];

                if(!file.is_file)
                    continue;

                const AnsiString name = AnsiFromOS(file.name);

                if(!EndsWithNoCase(name, kRecipeSuffix, kRecipeSuffixLen))
                    continue;

                RecipeFileRef *ref = out.Create();

                ref->path     = JoinAssetRef(base_dir, name);
                ref->open_ref = ref->path;
                count++;
            }

            return count;
        }

        AssetDepKind KindFromType(asset::AssetType type)
        {
            switch(type)
            {
                case AssetType::Geometry:        return AssetDepKind::Geometry;
                case AssetType::Texture:         return AssetDepKind::Texture;
                case AssetType::MaterialRecipe:  return AssetDepKind::MaterialRecipe;
                case AssetType::MaterialData:    return AssetDepKind::MaterialData;
                case AssetType::Model:           return AssetDepKind::Model;
                case AssetType::Primitive:       return AssetDepKind::Primitive;
                case AssetType::Environment:     return AssetDepKind::Environment;
                default:                         return AssetDepKind::Material;
            }
        }

        /// 读一个资产的全部字节（调用方负责 delete[]）。
        void *ReadAllBytes(const AnsiString &ref, int64 &out_size, AnsiString &out_error)
        {
            out_size = 0;

            io::InputStream *stream = OpenAssetInputStream(ToOSString(ref.c_str()),
                                                          AssetType::Unknown, &out_error);

            if(stream == nullptr)
                return nullptr;

            void *bytes = io::LoadFromInputStream(&out_size, stream);

            delete stream;

            if(bytes && out_size <= 0)
            {
                delete[] static_cast<char *>(bytes);
                bytes = nullptr;
            }

            return bytes;
        }
    }//anonymous namespace

    int ModelLoadPlan::CountOf(AssetDepKind kind) const
    {
        int count = 0;

        for(int i = 0; i < tasks.GetCount(); i++)
            if(tasks[i].kind == kind)
                count++;

        return count;
    }

    namespace
    {
        /**
         * 计划单构建的共用主体。
         *
         * 字节由调用方提供：本函数**不读盘、不释放字节**。异步流送里同一份
         * 字节要同时喂给「依赖提取」与「场景解析」，重复读盘是纯浪费。
         */
        bool BuildPlanFromBytes(const AnsiString &scene_ref,
                                const void *pack_bytes,
                                const uint32 pack_size,
                                ModelLoadPlan &out,
                                AssetLoadGraph *out_graph,
                                AnsiString *out_error)
        {
            const OSString        model_os = ToOSString(scene_ref.c_str());
            const asset::AssetURI uri      = asset::AssetURI::Parse(model_os, AssetType::Model);

            io::minipack::MiniPackMemory *mpm =
                io::minipack::GetMiniPackMemoryFromBytes(model_os, pack_bytes, pack_size);

            if (mpm == nullptr)
            {
                if(out_error)
                    *out_error = "cannot parse model pack";

                FLogError(u8"构建装载计划失败：模型包无法解析 %s", scene_ref.c_str());
                return false;
            }

        // 依赖提取与配方匹配共用的图。调用方不需要时也必须有实体。
        AssetLoadGraph local_graph;
        AssetLoadGraph &graph = (out_graph ? *out_graph : local_graph);

        graph.Clear();

        AnsiString base_dir;

        if(uri.IsPack())
            base_dir = DirNameOf(AnsiFromOS(uri.target.c_str()));
        else
            base_dir = DirNameOf(scene_ref);

        bool ok = false;

        const int32 header_idx  = mpm->FindFile(AnsiStringView("ModelPackHeader"));
        const int32 payload_idx = mpm->FindFile(AnsiStringView("ModelPayload"));

        if(header_idx >= 0 && payload_idx >= 0)
        {
            const void  *header_raw  = mpm->Map(header_idx);
            const uint32 header_len  = mpm->GetFileLength(header_idx);
            const void  *payload_raw = mpm->Map(payload_idx);
            const uint32 payload_len = mpm->GetFileLength(payload_idx);

            if(header_raw && payload_raw && header_len > 0 && payload_len > 0)
            {
                ManagedArray<ModelMaterialEntry> materials;

                int edge_count = 0;

                ok = ExtractModelDependencies(header_raw, header_len,
                                              payload_raw, payload_len,
                                              base_dir, scene_ref,
                                              graph, &edge_count, &materials,
                                              &out.embedded_geometry_count);

                if(ok)
                {
                    // ── 材质 → 配方 → 纹理 ─────────────────────────────────────
                    //
                    // 模型包只连到材质节点，纹理挂在配方节点上。不补这一条边，
                    // 纹理与配方就脱离根节点，闭包会漏掉它们 —— 而它们恰恰是最
                    // 贵的两类装载。
                    ManagedArray<RecipeFileRef> recipes;

                    const int recipe_count = CollectRecipeFiles(uri, base_dir, recipes);

                    if(recipe_count < 0)
                        FLogWarning(u8"场景目录不可枚举，配方依赖将缺失：%s", base_dir.c_str());

                    for(int i = 0; i < materials.GetCount(); i++)
                    {
                        const ModelMaterialEntry *mat = materials[i];

                        if(mat == nullptr)
                            continue;

                        ValueArray<AssetDepEdge> mat_edges;
                        int                      matched = 0;

                        for(int r = 0; r < recipes.GetCount(); r++)
                        {
                            const RecipeFileRef *recipe = recipes[r];

                            if(recipe == nullptr)
                                continue;

                            if(!EqualNoCase(RecipeLastName(recipe->path), mat->name))
                                continue;

                            AssetDepKey recipe_key;

                            if(!NormalizeAssetRef(recipe->path, AssetType::MaterialRecipe, recipe_key))
                                continue;

                            AssetDepEdge edge;
                            edge.target = recipe_key;
                            edge.kind   = AssetDepKind::MaterialRecipe;

                            mat_edges.Add(edge);
                            matched++;

                            // 配方只读一次：同目录里多个材质可能指向同一份配方
                            int      recipe_edges = 0;
                            int64    content_size = 0;
                            AnsiString recipe_error;
                            void    *content = ReadAllBytes(recipe->open_ref, content_size, recipe_error);

                            if(content)
                            {
                                ExtractRecipeDependencies(static_cast<const char *>(content),
                                                          (int)content_size,
                                                          base_dir, recipe->path,
                                                          graph, &recipe_edges);
                                delete[] static_cast<char *>(content);
                            }
                            else
                            {
                                FLogWarning(u8"配方读取失败，纹理依赖缺失：%s (%s)",
                                            recipe->open_ref.c_str(), recipe_error.c_str());
                            }
                        }

                        if(matched == 0)
                            FLogWarning(u8"材质缺少同名配方，纹理依赖缺失：%s",
                                        mat->name.c_str());

                        graph.SetDependencies(mat->key, mat_edges.GetData(), mat_edges.GetCount());
                    }
                }
            }
            else
            {
                FLogError(u8"构建装载计划失败：ModelPackHeader/ModelPayload 为空 %s",
                          scene_ref.c_str());
            }
        }
        else
        {
            FLogError(u8"构建装载计划失败：缺少 ModelPackHeader/ModelPayload 条目 %s",
                      scene_ref.c_str());
        }

        delete mpm;

        if(!ok)
        {
            if(out_error && out_error->IsEmpty())
                *out_error = "scene dependency extraction failed";
            return false;
        }

        // ── 闭包 → 任务单 ────────────────────────────────────────────────────
        AssetDepKey root;

        if(!NormalizeAssetRef(scene_ref, AssetType::Model, root))
        {
            if(out_error)
                *out_error = "scene ref cannot be normalized";
            return false;
        }

        ValueArray<AssetDepKey> closure;

        out.acyclic = graph.SolveClosure(root, closure);

        // 节点的派发种类只存在于「边」上（`AssetDepKey` 不冗余存 kind）。
        // 先把全图边扫一遍建索引，闭包里的每个节点都能查到自己的种类。
        UnorderedMap<AnsiString, int> kind_index;

        for(int i = 0; i < graph.GetNodeCount(); i++)
        {
            const AssetDepKey *node = graph.GetNodeAt(i);

            if(node == nullptr)
                continue;

            int                 edge_count = 0;
            const AssetDepEdge *edges      = graph.GetDependencies(*node, &edge_count);

            for(int e = 0; e < edge_count; e++)
            {
                const AnsiString id = edges[e].target.MakeId();

                if(!kind_index.ContainsKey(id))
                    kind_index.Add(id, (int)edges[e].kind);
            }
        }

        for(int i = 0; i < closure.GetCount(); i++)
        {
            ModelLoadTask task;

            task.key       = closure[i];
            task.ref_count = graph.GetRefCount(task.key);

            int kind = 0;

            if(kind_index.Get(task.key.MakeId(), kind))
                task.kind = (AssetDepKind)kind;
            else
                task.kind = KindFromType(task.key.type);

            out.tasks.Add(task);
        }

        if(!out.acyclic)
            FLogWarning(u8"场景依赖图有环，装载顺序不再是严格拓扑序：%s", scene_ref.c_str());

        return true;
    }
    }//anonymous namespace

    namespace
    {
        /// 两个入口共用的前置校验与字段复位。
        bool PreparePlan(const AnsiString &scene_ref, ModelLoadPlan &out, AnsiString *out_error)
        {
            out.tasks.Clear();
            out.acyclic = true;
            out.embedded_geometry_count = 0;

            if(out_error)
                out_error->Clear();

            if(scene_ref.IsEmpty())
            {
                if(out_error)
                    *out_error = "scene ref is empty";
                return false;
            }

            return true;
        }
    }//anonymous namespace

    bool BuildModelLoadPlanFromBytes(const AnsiString &scene_ref,
                                     const void *pack_bytes,
                                     const uint32 pack_size,
                                     ModelLoadPlan &out,
                                     AssetLoadGraph *out_graph,
                                     AnsiString *out_error)
    {
        if(!PreparePlan(scene_ref, out, out_error))
            return false;

        if(pack_bytes == nullptr || pack_size == 0)
        {
            if(out_error)
                *out_error = "model pack bytes is empty";

            FLogError(u8"构建装载计划失败：模型包字节为空 %s", scene_ref.c_str());
            return false;
        }

        return BuildPlanFromBytes(scene_ref, pack_bytes, pack_size, out, out_graph, out_error);
    }

    bool BuildModelLoadPlan(const AnsiString &scene_ref,
                            ModelLoadPlan &out,
                            AssetLoadGraph *out_graph,
                            AnsiString *out_error)
    {
        if(!PreparePlan(scene_ref, out, out_error))
            return false;

        int64      pack_size  = 0;
        AnsiString open_error;
        void      *pack_bytes = ReadAllBytes(scene_ref, pack_size, open_error);

        if(pack_bytes == nullptr)
        {
            if(out_error)
                *out_error = open_error.IsEmpty() ? AnsiString("cannot read scene") : open_error;

            FLogError(u8"构建装载计划失败：读不到场景 %s", scene_ref.c_str());
            return false;
        }

        const bool ok = BuildPlanFromBytes(scene_ref, pack_bytes, (uint32)pack_size,
                                           out, out_graph, out_error);

        delete[] static_cast<char *>(pack_bytes);

        return ok;
    }

    void DumpModelLoadPlan(const AnsiString &scene_ref, const ModelLoadPlan &plan)
    {
        FLogInfo(u8"装载计划：%s 任务=%d 几何=%d 内嵌几何=%d 材质=%d 配方=%d 纹理=%d 无环=%d",
                 scene_ref.c_str(),
                 plan.TotalCount(),
                 plan.CountOf(AssetDepKind::Geometry),
                 plan.embedded_geometry_count,
                 plan.CountOf(AssetDepKind::Material),
                 plan.CountOf(AssetDepKind::MaterialRecipe),
                 plan.CountOf(AssetDepKind::Texture),
                 plan.acyclic ? 1 : 0);

        for(int i = 0; i < plan.tasks.GetCount(); i++)
        {
            const ModelLoadTask &task = plan.tasks[i];

            FLogInfo(u8"  [%d] %s %s ref=%d",
                     i,
                     GetAssetDepKindName(task.kind),
                     task.key.MakeId().c_str(),
                     task.ref_count);
        }
    }
}//namespace hgl::graph
