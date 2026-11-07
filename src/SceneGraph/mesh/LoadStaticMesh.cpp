// LoadStaticMesh.cpp — 从 .model pack 加载场景树到 StaticMesh
// Core scene loader for StaticMesh asset packs.

#include <hgl/graph/mesh/LoadStaticMesh.h>
#include <hgl/graph/mesh/ModelPackFormat.h>
#include <hgl/graph/geo/LoadGeometry.h>
#include <hgl/graph/mesh/StaticMesh.h>
#include <hgl/math/Matrix.h>      // TranslateMatrix / ToMatrix / ScaleMatrix（TRS→矩阵组合）
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/asset/MaterialDataBlockRegistry.h>
#include <hgl/graph/asset/MaterialDataBlockAsset.h>
#include <hgl/graph/asset/MaterialAsyncLoadLane.h>
#include <hgl/graph/asset/AssetSourceMount.h>
#include <hgl/asset/LoadedFileManager.h>
#include <hgl/time/Time.h>
#include <hgl/mtl/MaterialRecipeRegistry.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/io/MiniPack.h>
#include <hgl/io/InputStream.h>
#include <hgl/type/StdString.h>
#include <hgl/log/Log.h>

#include <vector>
#include <string>
#include <cstring>

// StaticMesh.h → BoundingVolumes.h → Matrix.h / VectorTypes.h / Quaternion.h
// 所以 Matrix4f / Vector3f / Quatf / BoundingVolumes / BoundingVolumesData 均可用

DEFINE_LOGGER_MODULE(LoadStaticMesh)

namespace hgl::graph
{

namespace
{
// ---- packed structures (mirror the exporter layout) ------------------------

/// 从局部 TRS 沿层级组合出 local/world 矩阵（两份装载路径共用，定义见文件后半）
void ComposeNodeMatrices(std::vector<StaticMeshNode> &nodes);

// MDL2 表词汇表（ModelTableType / ModelPackHeader / PackedXXX / FindTableDesc /
// TryGetTableRange）统一来自 <hgl/graph/mesh/ModelPackFormat.h>，与依赖图提取、
// 异步场景流送共用同一份声明，避免多处副本漂移。
using namespace hgl::graph::modelpack;

std::vector<std::string> ParseNameTable(const void *raw, uint32_t bytes);

static bool TryLoadModel(
    hgl::io::minipack::MiniPackMemory *mpm,
    VulkanDevice             *device,
    GeometryManager          *geo_mgr,
    const GeometryVertexFormat &geometry_vertex_format,
    const OSString           &pack_path,
    const OSString           &base_dir,
    std::vector<Geometry *>  &geometry_list,
    std::vector<int32_t>     &material_indices,
    std::vector<StaticMeshNode> &scene_nodes,
    std::vector<int32_t> &root_nodes)
{
    const int32 header_idx = mpm->FindFile(AnsiStringView("ModelPackHeader"));
    const int32 payload_idx = mpm->FindFile(AnsiStringView("ModelPayload"));

    if (header_idx < 0 || payload_idx < 0)
        return false;

    const void *header_raw = mpm->Map(header_idx);
    const uint32 header_len = mpm->GetFileLength(header_idx);
    const uint8_t *payload = reinterpret_cast<const uint8_t *>(mpm->Map(payload_idx));
    const uint32 payload_len = mpm->GetFileLength(payload_idx);

    if (!header_raw || header_len < sizeof(ModelPackHeader) || !payload)
        return false;

    ModelPackHeader h{};
    std::memcpy(&h, header_raw, sizeof(ModelPackHeader));

    if (h.magic != kModelPackV2Magic)
        return false;
    if (h.version_major != 2)
        return false;
    if (h.payload_size > payload_len)
        return false;
    if (h.dir_offset > h.payload_size || h.dir_count * sizeof(ModelTableDesc) > h.payload_size || h.dir_offset + h.dir_count * sizeof(ModelTableDesc) > h.payload_size)
        return false;

    const auto *dir = reinterpret_cast<const ModelTableDesc *>(payload + h.dir_offset);

    const ModelTableDesc *name_td = FindTableDesc(dir, h.dir_count, ModelTableType::NameTable);
    const ModelTableDesc *node_td = FindTableDesc(dir, h.dir_count, ModelTableType::NodeTable);
    const ModelTableDesc *node_prim_td = FindTableDesc(dir, h.dir_count, ModelTableType::NodePrimIndex);
    const ModelTableDesc *node_child_td = FindTableDesc(dir, h.dir_count, ModelTableType::NodeChildIndex);
    const ModelTableDesc *root_td = FindTableDesc(dir, h.dir_count, ModelTableType::RootIndex);
    const ModelTableDesc *trs_td = FindTableDesc(dir, h.dir_count, ModelTableType::TRSTable);
    const ModelTableDesc *bounds_td = FindTableDesc(dir, h.dir_count, ModelTableType::BoundsTable);
    const ModelTableDesc *primitive_td = FindTableDesc(dir, h.dir_count, ModelTableType::PrimitiveTable);
    const ModelTableDesc *string_pool_td = FindTableDesc(dir, h.dir_count, ModelTableType::StringPool);
    const ModelTableDesc *geo_view_td = FindTableDesc(dir, h.dir_count, ModelTableType::GeometryViewTable);
    const ModelTableDesc *geo_blob_td = FindTableDesc(dir, h.dir_count, ModelTableType::GeometryBlob);

    if (!node_td || !primitive_td || !string_pool_td)
        return false;

    MLogInfo(LoadStaticMesh, OS_TEXT("[V2] ") + pack_path
        + OS_TEXT(" v") + OSString::numberOf(h.version_major)
        + OS_TEXT(".") + OSString::numberOf(h.version_minor)
        + OS_TEXT(" nodes=") + OSString::numberOf(h.node_count)
        + OS_TEXT(" prims=") + OSString::numberOf(h.primitive_count)
        + OS_TEXT(" geos=") + OSString::numberOf(h.geometry_count)
        + OS_TEXT(" dirs=") + OSString::numberOf(h.dir_count));
    for (uint32_t _di = 0; _di < h.dir_count; ++_di)
        MLogInfo(LoadStaticMesh,
            OS_TEXT("[V2]  dir[") + OSString::numberOf(_di)
            + OS_TEXT("] type=") + OSString::numberOf(dir[_di].type)
            + OS_TEXT(" off=") + OSString::numberOf(dir[_di].offset)
            + OS_TEXT(" sz=") + OSString::numberOf(dir[_di].size));

    std::vector<std::string> names;
    {
        const uint8_t *ptr = nullptr;
        uint32_t size = 0;
        if (TryGetTableRange(payload, h.payload_size, name_td, ptr, size))
            names = ParseNameTable(ptr, size);
    }

    const PackedTRS *trs_arr = nullptr;
    uint32_t trs_count = 0;
    {
        const uint8_t *ptr = nullptr;
        uint32_t size = 0;
        if (TryGetTableRange(payload, h.payload_size, trs_td, ptr, size))
        {
            trs_arr = reinterpret_cast<const PackedTRS *>(ptr);
            trs_count = size / static_cast<uint32_t>(sizeof(PackedTRS));
        }
    }

    const math::BoundingVolumesData *bounds_arr = nullptr;
    uint32_t bounds_count = 0;
    {
        const uint8_t *ptr = nullptr;
        uint32_t size = 0;
        if (TryGetTableRange(payload, h.payload_size, bounds_td, ptr, size))
        {
            bounds_arr = reinterpret_cast<const math::BoundingVolumesData *>(ptr);
            bounds_count = size / static_cast<uint32_t>(sizeof(math::BoundingVolumesData));
        }
    }

    root_nodes.clear();
    {
        const uint8_t *ptr = nullptr;
        uint32_t size = 0;
        if (TryGetTableRange(payload, h.payload_size, root_td, ptr, size))
        {
            const int32_t *ri = reinterpret_cast<const int32_t *>(ptr);
            const uint32_t cnt = size / static_cast<uint32_t>(sizeof(int32_t));
            root_nodes.assign(ri, ri + cnt);
        }
    }

    const uint8_t *string_pool = nullptr;
    uint32_t string_pool_size = 0;
    if (string_pool_td)
    {
        if (!TryGetTableRange(payload, h.payload_size, string_pool_td, string_pool, string_pool_size))
            return false;
    }

    auto read_pool_string = [&](uint32_t off, uint32_t len, std::string &out) -> bool
    {
        if (!string_pool)
            return false;
        if (off > string_pool_size || len > string_pool_size || off + len > string_pool_size)
            return false;
        out.assign(reinterpret_cast<const char *>(string_pool + off), len);
        return true;
    };

    std::vector<Geometry *> geometry_by_index;
    if (geo_view_td && geo_blob_td)
    {
        const uint8_t *gv_ptr = nullptr;
        uint32_t gv_size = 0;
        const uint8_t *gb_ptr = nullptr;
        uint32_t gb_size = 0;

        if (!TryGetTableRange(payload, h.payload_size, geo_view_td, gv_ptr, gv_size))
            return false;
        if (!TryGetTableRange(payload, h.payload_size, geo_blob_td, gb_ptr, gb_size))
            return false;
        if (gv_size % sizeof(PackedGeometryView) != 0)
            return false;

        const auto *gv = reinterpret_cast<const PackedGeometryView *>(gv_ptr);
        const uint32_t gv_count = gv_size / static_cast<uint32_t>(sizeof(PackedGeometryView));

        geometry_by_index.resize(gv_count, nullptr);
        MLogInfo(LoadStaticMesh, OS_TEXT("[V2] GeometryViewTable: gv_count=") + OSString::numberOf(gv_count)
            + OS_TEXT(" blob_size=") + OSString::numberOf(gb_size));

        // Batch path: payload read is once, then each geometry is sliced from the same memory block.
        for (uint32_t i = 0; i < gv_count; ++i)
        {
            if (gv[i].blob_offset > gb_size || gv[i].blob_size > gb_size || gv[i].blob_offset + gv[i].blob_size > gb_size)
            {
                MLogError(LoadStaticMesh, OS_TEXT("LoadModel: invalid GeometryBlob range in ") + pack_path);
                continue;
            }

            const void *geo_blob = gb_ptr + gv[i].blob_offset;

            const OSString geo_debug_name =
                pack_path + OS_TEXT("#GeometryBlob[") + OSString::numberOf(i) + OS_TEXT("]");

            Geometry *geo = LoadGeometryFromMiniPackBytes(device, geometry_vertex_format, geo_blob, gv[i].blob_size, geo_debug_name);
            if (!geo)
            {
                MLogError(LoadStaticMesh, OS_TEXT("LoadModel: failed to load inlined geometry #") + OSString::numberOf(i) + OS_TEXT(" from ") + pack_path);
                continue;
            }

            geo_mgr->Add(geo);
            geometry_by_index[i] = geo;
        }
        {
            uint32_t _geo_ok = 0;
            for (Geometry *_g : geometry_by_index) if (_g) ++_geo_ok;
            MLogInfo(LoadStaticMesh, OS_TEXT("[V2] Geometries loaded from blob: ")
                + OSString::numberOf(_geo_ok) + OS_TEXT("/") + OSString::numberOf((uint32_t)geometry_by_index.size()));
        }
    }

    geometry_list.clear();
    material_indices.clear();
    {
        const uint8_t *ptr = nullptr;
        uint32_t size = 0;
        if (!TryGetTableRange(payload, h.payload_size, primitive_td, ptr, size))
            return false;
        if (size % sizeof(PackedPrimitive) != 0)
            return false;

        const auto *pp = reinterpret_cast<const PackedPrimitive *>(ptr);
        const uint32_t pcount = size / static_cast<uint32_t>(sizeof(PackedPrimitive));
        geometry_list.reserve(pcount);
        MLogInfo(LoadStaticMesh, OS_TEXT("[V2] PrimitiveTable: count=") + OSString::numberOf(pcount)
            + OS_TEXT(" raw_size=") + OSString::numberOf(size)
            + OS_TEXT(" entry_size=") + OSString::numberOf((uint32_t)sizeof(PackedPrimitive)));
        for (uint32_t _pi = 0, _plim = pcount < 8u ? pcount : 8u; _pi < _plim; ++_pi)
            MLogInfo(LoadStaticMesh,
                OS_TEXT("[V2]  pp[") + OSString::numberOf(_pi)
                + OS_TEXT("] orig=") + OSString::numberOf(pp[_pi].original_index)
                + OS_TEXT(" geo=") + OSString::numberOf(pp[_pi].geometry_index)
                + OS_TEXT(" mat=") + OSString::numberOf(pp[_pi].material_index)
                + OS_TEXT(" foff=") + OSString::numberOf(pp[_pi].geometry_file_offset)
                + OS_TEXT(" flen=") + OSString::numberOf(pp[_pi].geometry_file_length));

        std::vector<Geometry *> fallback_geometry_cache;

        for (uint32_t i = 0; i < pcount; ++i)
        {
            Geometry *geo = nullptr;
            material_indices.push_back(pp[i].material_index);

            if (!geometry_by_index.empty()
             && pp[i].geometry_index >= 0
             && static_cast<uint32_t>(pp[i].geometry_index) < geometry_by_index.size())
            {
                geo = geometry_by_index[pp[i].geometry_index];
            }
            else
            {
                // Fallback for early V2 packs without GeometryBlob/GeometryView.
                if (pp[i].geometry_index >= 0)
                {
                    const uint32_t gi = static_cast<uint32_t>(pp[i].geometry_index);
                    if (fallback_geometry_cache.size() <= gi)
                        fallback_geometry_cache.resize(gi + 1, nullptr);

                    geo = fallback_geometry_cache[gi];
                    if (!geo)
                    {
                        std::string geo_file;
                        if (!read_pool_string(pp[i].geometry_file_offset, pp[i].geometry_file_length, geo_file))
                        {
                            MLogError(LoadStaticMesh, OS_TEXT("LoadModel: invalid V2 geometry filename range in ") + pack_path);
                            geometry_list.push_back(nullptr);
                            continue;
                        }

                        const OSString geo_path = base_dir + OS_TEXT("/") + hgl::ToOSString(geo_file);

                        geo = LoadGeometry(device, geometry_vertex_format, geo_path);
                        if (!geo)
                        {
                            MLogError(LoadStaticMesh, OS_TEXT("LoadModel: failed to load geometry: ") + geo_path);
                            geometry_list.push_back(nullptr);
                            continue;
                        }

                        geo_mgr->Add(geo);
                        fallback_geometry_cache[gi] = geo;
                    }
                }
            }

            if (!geo)
            {
                MLogError(LoadStaticMesh, OS_TEXT("LoadModel: missing geometry for primitive #") + OSString::numberOf(i) + OS_TEXT(" in ") + pack_path);
                geometry_list.push_back(nullptr);
                continue;
            }

            geometry_list.push_back(geo);
        }
    }

    const int32_t *node_prim = nullptr;
    uint32_t node_prim_count = 0;
    {
        const uint8_t *ptr = nullptr;
        uint32_t size = 0;
        if (TryGetTableRange(payload, h.payload_size, node_prim_td, ptr, size))
        {
            node_prim = reinterpret_cast<const int32_t *>(ptr);
            node_prim_count = size / static_cast<uint32_t>(sizeof(int32_t));
        }
    }

    const int32_t *node_child = nullptr;
    uint32_t node_child_count = 0;
    {
        const uint8_t *ptr = nullptr;
        uint32_t size = 0;
        if (TryGetTableRange(payload, h.payload_size, node_child_td, ptr, size))
        {
            node_child = reinterpret_cast<const int32_t *>(ptr);
            node_child_count = size / static_cast<uint32_t>(sizeof(int32_t));
        }
    }

    scene_nodes.clear();
    {
        const uint8_t *ptr = nullptr;
        uint32_t size = 0;
        if (!TryGetTableRange(payload, h.payload_size, node_td, ptr, size))
            return false;
        if (size % sizeof(PackedNode) != 0)
            return false;

        const auto *pn = reinterpret_cast<const PackedNode *>(ptr);
        const uint32_t ncount = size / static_cast<uint32_t>(sizeof(PackedNode));
        scene_nodes.reserve(ncount);

        for (uint32_t i = 0; i < ncount; ++i)
        {
            StaticMeshNode node;

            if (pn[i].name_index >= 0 && pn[i].name_index < static_cast<int32_t>(names.size()))
                node.name = names[pn[i].name_index];

            if (pn[i].trs_index >= 0 && pn[i].trs_index < static_cast<int32_t>(trs_count))
            {
                const PackedTRS &t = trs_arr[pn[i].trs_index];
                node.hasTRS = true;
                node.translation = math::Vector3f(t.translation[0], t.translation[1], t.translation[2]);
                node.rotation    = math::Quatf(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]);
                node.scale       = math::Vector3f(t.scale[0], t.scale[1], t.scale[2]);
            }

            if (pn[i].bounds_index >= 0 && pn[i].bounds_index < static_cast<int32_t>(bounds_count))
            {
                math::BoundingVolumesData bvd = bounds_arr[pn[i].bounds_index];
                bvd.To(&node.nodeBounds);
                node.boundsValid = true;
            }

            if (pn[i].first_primitive >= 0 && pn[i].primitive_count >= 0)
            {
                const uint32_t first = static_cast<uint32_t>(pn[i].first_primitive);
                const uint32_t count = static_cast<uint32_t>(pn[i].primitive_count);
                if (first <= node_prim_count && count <= node_prim_count - first)
                    node.primitiveIndices.assign(node_prim + first, node_prim + first + count);
            }

            if (pn[i].first_child >= 0 && pn[i].child_count >= 0)
            {
                const uint32_t first = static_cast<uint32_t>(pn[i].first_child);
                const uint32_t count = static_cast<uint32_t>(pn[i].child_count);
                if (first <= node_child_count && count <= node_child_count - first)
                    node.children.assign(node_child + first, node_child + first + count);
            }

            scene_nodes.push_back(std::move(node));
        }
    }

    for (int32_t i = 0; i < static_cast<int32_t>(scene_nodes.size()); ++i)
    {
        for (int32_t c : scene_nodes[i].children)
        {
            if (c >= 0 && c < static_cast<int32_t>(scene_nodes.size()))
                scene_nodes[c].parentIndex = i;
        }
    }

    // 产物只存局部 TRS（T5）⇒ local/world 矩阵由 ComposeNodeMatrices 组合得出
    ComposeNodeMatrices(scene_nodes);

    return true;
}
// ---- NameTable parser -------------------------------------------------------
// Format written by write_string_list():
//   uint32_t count | uint8_t lengths[count] | per-string: char[len] + '\0'

std::vector<std::string> ParseNameTable(const void *raw, uint32_t bytes)
{
    std::vector<std::string> names;
    if (!raw || bytes < 4)
        return names;

    const uint8_t *p   = reinterpret_cast<const uint8_t *>(raw);
    const uint8_t *end = p + bytes;

    uint32_t count;
    std::memcpy(&count, p, sizeof(uint32_t));
    p += 4;

    if (count == 0 || p + count > end)
        return names;

    const uint8_t *lengths       = p;
    const uint8_t *strings_begin = p + count;
    p = strings_begin;

    names.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        uint8_t len = lengths[i];
        if (p + len + 1 > end)
            break;
        names.emplace_back(reinterpret_cast<const char *>(p), len);
        p += len + 1;   // skip content + NUL
    }
    return names;
}

/// 从局部 TRS 沿层级组合出 local/world 矩阵。
/// 约定：没有 TRS 行的节点 ⇒ 局部变换是单位矩阵。
void ComposeNodeMatrices(std::vector<StaticMeshNode> &nodes)
{
    std::vector<bool>    done(nodes.size(), false);
    std::vector<int32_t> stack;

    for (int32_t i = 0; i < static_cast<int32_t>(nodes.size()); ++i)
        if (nodes[i].parentIndex < 0)
            stack.push_back(i);                           // 根节点（或未挂父的孤立节点）

    // DFS 前序：父节点的 world 先算好，再下传给子（先压根，处理完再压子）
    while (!stack.empty())
    {
        const int32_t idx = stack.back();
        stack.pop_back();

        if (idx < 0 || idx >= static_cast<int32_t>(nodes.size()) || done[idx])
            continue;

        StaticMeshNode &n = nodes[idx];

        // 与 hgl::math::Transform::GetMatrix() 同一个组合式（复用引擎已有写法）
        const Matrix4f local = n.hasTRS
                             ? (math::TranslateMatrix(n.translation)
                                * math::ToMatrix(n.rotation)
                                * math::ScaleMatrix(n.scale))
                             : Matrix4f(1.0f);

        const int32_t p = n.parentIndex;
        n.localMatrix = local;
        n.worldMatrix = (p >= 0 && p < static_cast<int32_t>(nodes.size()))
                      ? nodes[p].worldMatrix * local
                      : local;
        done[idx] = true;

        for (int32_t c : n.children)
            stack.push_back(c);
    }
}

}  // anonymous namespace


// ---- Public API -------------------------------------------------------------

/// 解析一个**已打开**的 minipack（内存映射或字节视图）。
/// @note 本函数在返回前 `delete mpm`（接管所有权），调用方不得再使用该指针。
static bool BuildModelFromMiniPack(
    VulkanDevice                     *device,
    GeometryManager                  *geo_mgr,
    const GeometryVertexFormat       &geometry_vertex_format,
    const mtl::MaterialRecipe        *recipe,
    hgl::io::minipack::MiniPackMemory *mpm,
    const OSString                   &pack_path,
    const OSString                   &base_dir,
    std::vector<PrimitiveAsset>      &out_assets,
    std::vector<StaticMeshNode>      &out_nodes,
    std::vector<int32_t>             &out_root_nodes,
    const mtl::MaterialRecipe *const *material_recipes,
    const uint32_t                    material_recipe_count)
{
    using namespace hgl::math;

    out_assets.clear();
    out_nodes.clear();
    out_root_nodes.clear();

    if (!device || !geo_mgr || geometry_vertex_format.GetCount() == 0 || !recipe || !mpm)
    {
        MLogError(LoadStaticMesh, OS_TEXT("LoadModelAsPrimitiveAssets: invalid argument"));
        return false;
    }

    // 按源材质下标选取配方；缺失/越界/空条目回退到默认 recipe（既有单材质行为）。
    auto pick_recipe = [&](int32_t material_index) -> const mtl::MaterialRecipe *
    {
        if (material_recipes
            && material_index >= 0
            && static_cast<uint32_t>(material_index) < material_recipe_count
            && material_recipes[material_index])
            return material_recipes[material_index];
        return recipe;
    };

    std::vector<Geometry *> geometry_list;
    std::vector<int32_t> material_indices;
    std::vector<StaticMeshNode> scene_nodes;
    std::vector<int32_t> root_nodes;

    if (!TryLoadModel(
            mpm,
            device,
            geo_mgr,
            geometry_vertex_format,
            pack_path,
            base_dir,
            geometry_list,
            material_indices,
            scene_nodes,
            root_nodes))
    {
        MLogError(LoadStaticMesh,
            OS_TEXT("LoadModelAsPrimitiveAssets: MDL2 parse failed: ") + pack_path);
        delete mpm;
        return false;
    }

    delete mpm;

    out_assets.reserve(geometry_list.size());

    for (size_t i = 0; i < geometry_list.size(); ++i)
    {
        Geometry *geo = geometry_list[i];
        const int32_t mat_index = (i < material_indices.size()) ? material_indices[i] : -1;
        const mtl::MaterialRecipe *prim_recipe = pick_recipe(mat_index);

        if (!geo)
        {
            out_assets.emplace_back();
            continue;
        }

        out_assets.emplace_back(geo, prim_recipe, PrimitiveType::Triangles, mat_index);
    }

    out_nodes = std::move(scene_nodes);
    out_root_nodes = std::move(root_nodes);

    return true;
}

bool LoadModelAsPrimitiveAssets(
    VulkanDevice                     *device,
    GeometryManager                  *geo_mgr,
    const GeometryVertexFormat       &geometry_vertex_format,
    const mtl::MaterialRecipe        *recipe,
    const OSString                   &pack_path,
    const OSString                   &base_dir,
    std::vector<PrimitiveAsset>      &out_assets,
    std::vector<StaticMeshNode>      &out_nodes,
    std::vector<int32_t>             &out_root_nodes,
    const mtl::MaterialRecipe *const *material_recipes,
    const uint32_t                    material_recipe_count)
{
    using namespace hgl::io::minipack;

    // 统一取流：`pack_path` 既可以是历史裸路径，也可以是
    // `pack(res):scene/xxx`（无扩展名逻辑路径）或 `pack(res):id:<32 hex>`。
    AnsiString open_error;

    hgl::io::InputStream *stream=OpenAssetInputStream(pack_path,asset::AssetType::Model,&open_error);

    if (!stream)
    {
        MLogError(LoadStaticMesh, OS_TEXT("LoadModelAsPrimitiveAssets: cannot open asset: ") + pack_path
                                + OS_TEXT(" : ") + ToOSString(open_error.c_str()));
        return false;
    }

    const bool result=LoadModelFromStream(device,geo_mgr,geometry_vertex_format,recipe,
                                                   stream,pack_path,base_dir,
                                                   out_assets,out_nodes,out_root_nodes,
                                                   material_recipes,material_recipe_count);

    delete stream;
    return result;
}

bool LoadModelFromMemory(
    VulkanDevice                     *device,
    GeometryManager                  *geo_mgr,
    const GeometryVertexFormat       &geometry_vertex_format,
    const mtl::MaterialRecipe        *recipe,
    const void                       *pack_bytes,
    const uint32                      pack_size,
    const OSString                   &pack_name,
    const OSString                   &base_dir,
    std::vector<PrimitiveAsset>      &out_assets,
    std::vector<StaticMeshNode>      &out_nodes,
    std::vector<int32_t>             &out_root_nodes,
    const mtl::MaterialRecipe *const *material_recipes,
    const uint32_t                    material_recipe_count)
{
    using namespace hgl::io::minipack;

    MiniPackMemory *mpm = GetMiniPackMemoryFromBytes(pack_name,pack_bytes,pack_size);
    if (!mpm)
    {
        MLogError(LoadStaticMesh, OS_TEXT("LoadModelFromMemory: cannot parse pack: ") + pack_name);
        return false;
    }

    return BuildModelFromMiniPack(device,geo_mgr,geometry_vertex_format,recipe,
                                  mpm,pack_name,base_dir,
                                  out_assets,out_nodes,out_root_nodes,
                                  material_recipes,material_recipe_count);
}

bool LoadModelFromStream(
    VulkanDevice                     *device,
    GeometryManager                  *geo_mgr,
    const GeometryVertexFormat       &geometry_vertex_format,
    const mtl::MaterialRecipe        *recipe,
    io::InputStream                  *stream,
    const OSString                   &pack_name,
    const OSString                   &base_dir,
    std::vector<PrimitiveAsset>      &out_assets,
    std::vector<StaticMeshNode>      &out_nodes,
    std::vector<int32_t>             &out_root_nodes,
    const mtl::MaterialRecipe *const *material_recipes,
    const uint32_t                    material_recipe_count)
{
    if (!stream)
    {
        MLogError(LoadStaticMesh, OS_TEXT("LoadModelFromStream: null stream: ") + pack_name);
        return false;
    }

    // 模型包解析全程需要随机访问（FindFile/Map），因此先整块读入。
    // 这块内存的生命周期严格覆盖下面这一次同步解析，不跨线程保留。
    int64 pack_size = 0;
    void *pack_bytes = hgl::io::LoadFromInputStream(&pack_size, stream);

    if (!pack_bytes || pack_size <= 0)
    {
        delete[] static_cast<char *>(pack_bytes);

        MLogError(LoadStaticMesh, OS_TEXT("LoadModelFromStream: cannot read stream: ") + pack_name);
        return false;
    }

    const bool result = LoadModelFromMemory(
        device, geo_mgr, geometry_vertex_format, recipe,
        pack_bytes, static_cast<uint32>(pack_size),
        pack_name, base_dir,
        out_assets, out_nodes, out_root_nodes,
        material_recipes, material_recipe_count);

    delete[] static_cast<char *>(pack_bytes);
    return result;
}

// ---- 自包含模型目录装配（严禁跨目录查找） ------------------------------------

namespace
{
    std::string LowerAsciiString(const std::string &s)
    {
        std::string out = s;
        for (char &c : out)
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        return out;
    }

    bool HasOsCharPrefixI(const os_char *file, const os_char *dir)
    {
        const os_char *f = file;
        const os_char *d = dir;
        for (; *d != 0; ++f, ++d)
        {
            const os_char a = (*f >= L'A' && *f <= L'Z') ? static_cast<os_char>(*f + 32) : *f;
            const os_char b = (*d >= L'A' && *d <= L'Z') ? static_cast<os_char>(*d + 32) : *d;
            if (a != b)
                return false;
        }
        return *f == (os_char)'\\' || *f == (os_char)'/';
    }

    // "DamagedHelmet.0.Material_MR.material" → "Material_MR"
    // 规则：去 `.material` 后缀；`<Scene>.<数字>.` 前缀剥掉；两段形态取最后一段。
    std::string MaterialNameFromPackFile(const std::string &file)
    {
        std::string s = file;
        static const char kSuffix[] = ".material";
        const size_t kSuffixLen = sizeof(kSuffix) - 1;
        if (s.size() > kSuffixLen &&
            s.compare(s.size() - kSuffixLen, kSuffixLen, kSuffix) == 0)
            s.erase(s.size() - kSuffixLen);

        const size_t d1 = s.find('.');
        if (d1 == std::string::npos)
            return s;
        const size_t d2 = s.find('.', d1 + 1);
        if (d2 == std::string::npos)
            return s.substr(d1 + 1);

        bool numeric = d2 > d1 + 1;
        for (size_t i = d1 + 1; i < d2 && numeric; ++i)
            numeric = (s[i] >= '0' && s[i] <= '9');
        return numeric ? s.substr(d2 + 1) : s;
    }

    // 配方名最后一段 == 材质名（ASCII 大小写不敏感）
    bool RecipeMatchesMaterialName(const mtl::MaterialRecipe &recipe, const std::string &material_name)
    {
        const std::string &full = recipe.recipe_name;
        const size_t dot = full.find_last_of('.');
        const std::string last = (dot == std::string::npos) ? full : full.substr(dot + 1);
        return LowerAsciiString(last) == LowerAsciiString(material_name);
    }

    // 只读 V2 MaterialTable（tag 9）：行序 = 材质下标；非 V2/缺表 ⇒ 空。
    std::vector<std::string> ReadModelPackMaterialNames(hgl::io::minipack::MiniPackMemory *mpm)
    {
        std::vector<std::string> names;

        const int32 header_idx  = mpm->FindFile(AnsiStringView("ModelPackHeader"));
        const int32 payload_idx = mpm->FindFile(AnsiStringView("ModelPayload"));
        if (header_idx < 0 || payload_idx < 0)
            return names;

        const void *header_raw  = mpm->Map(header_idx);
        const uint32 header_len = mpm->GetFileLength(header_idx);
        const uint8_t *payload  = reinterpret_cast<const uint8_t *>(mpm->Map(payload_idx));
        const uint32 payload_len = mpm->GetFileLength(payload_idx);
        if (!header_raw || header_len < sizeof(ModelPackHeader) || !payload)
            return names;

        ModelPackHeader h{};
        std::memcpy(&h, header_raw, sizeof(ModelPackHeader));
        if (h.magic != kModelPackV2Magic || h.version_major != 2)
            return names;
        if (h.payload_size > payload_len ||
            h.dir_offset > h.payload_size ||
            h.dir_count * sizeof(ModelTableDesc) > h.payload_size - h.dir_offset)
            return names;
        if (h.material_count == 0)
            return names;

        const auto *dir = reinterpret_cast<const ModelTableDesc *>(payload + h.dir_offset);
        const ModelTableDesc *mat_td  = FindTableDesc(dir, h.dir_count, ModelTableType::MaterialTable);
        const ModelTableDesc *pool_td = FindTableDesc(dir, h.dir_count, ModelTableType::StringPool);
        if (!mat_td || !pool_td)
            return names;

        const uint8_t *mat_ptr = nullptr;  uint32_t mat_size = 0;
        const uint8_t *pool_ptr = nullptr; uint32_t pool_size = 0;
        if (!TryGetTableRange(payload, h.payload_size, mat_td, mat_ptr, mat_size))
            return names;
        if (!TryGetTableRange(payload, h.payload_size, pool_td, pool_ptr, pool_size))
            return names;
        if (mat_size % sizeof(PackedMaterial) != 0)
            return names;

        const auto *pm = reinterpret_cast<const PackedMaterial *>(mat_ptr);
        const uint32_t count = mat_size / static_cast<uint32_t>(sizeof(PackedMaterial));
        names.reserve(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            if (pm[i].file_offset > pool_size ||
                pm[i].file_length > pool_size ||
                pm[i].file_offset + pm[i].file_length > pool_size)
            {
                names.clear();
                return names;
            }
            const std::string file(reinterpret_cast<const char *>(pool_ptr + pm[i].file_offset),
                                   pm[i].file_length);
            names.push_back(MaterialNameFromPackFile(file));
        }
        return names;
    }

// ---- 内存索引探索辅助：命中走索引，未命中回退 OS 扫描（行为是超集） ----------

    /// 折叠路径字符：'\\' 归 '/'、ASCII 大写归小写 —— 与索引键口径一致。
    inline os_char FoldOsPathChar(os_char c)
    {
        if (c == OS_TEXT('\\')) return OS_TEXT('/');
        if (c >= OS_TEXT('A') && c <= OS_TEXT('Z'))
            return (os_char)(c + (OS_TEXT('a') - OS_TEXT('A')));
        return c;
    }

    /// 绝对路径启发式（盘符 / 起始分隔符）。误判只会导致前缀比对失败 → 回退。
    inline bool LooksAbsoluteOsPath(const OSString &p)
    {
        if (p.Length() >= 2 && p[1] == OS_TEXT(':')) return true;
        if (p.Length() >= 1 && FoldOsPathChar(p[0]) == OS_TEXT('/')) return true;
        return false;
    }

    /// `dir` 位于 `root` 之下时，截出 '/' 分隔的相对逻辑目录（保留原始大小写）。
    static bool MakeLogicalDirUnderRoot(const OSString &root, const OSString &dir, U8String &out)
    {
        out.Clear();
        const int n = root.Length();
        const int m = dir.Length();
        if (n <= 0 || m < n) return false;
        for (int i = 0; i < n; ++i)
            if (FoldOsPathChar(dir[i]) != FoldOsPathChar(root[i])) return false;
        int skip = n;
        if (m > n)
        {
            if (FoldOsPathChar(dir[n]) != OS_TEXT('/')) return false;  // 兄弟目录（res_xxx）
            ++skip;
        }
        int end = m;
        while (end > skip && FoldOsPathChar(dir[end - 1]) == OS_TEXT('/')) --end;
        if (end <= skip) return false;                                 // 模型就在挂载根：无逻辑目录
        out = hgl::ToU8String(dir.SubString(skip, end - skip));
        for (int i = 0; i < out.Length(); ++i)
            if (out[i] == u8char('\\')) out[i] = u8char('/');
        return true;
    }

    /// ASCII 折叠相等（与索引键同口径），用于按名字比对逻辑路径。
    static inline bool U8FoldedEqual(const U8String &a, const U8String &b)
    {
        if (a.Length() != b.Length()) return false;
        for (int i = 0; i < a.Length(); ++i)
        {
            u8char x = a[i], y = b[i];
            if (x >= (u8char)'A' && x <= (u8char)'Z') x += 32;
            if (y >= (u8char)'A' && y <= (u8char)'Z') y += 32;
            if (x != y) return false;
        }
        return true;
    }

    /// 内存索引探测结果：模型目录映射到挂载根下的逻辑目录。
    struct IndexProbe
    {
        asset::AssetSource *src = nullptr;   ///<空 ⇒ 未挂载/无索引，走 OS 老路径
        OSString            root;            ///<挂载根绝对路径
        U8String            logical_dir;     ///<'/' 分隔、保留大小写的逻辑目录
    };

    /// 把 `model_dir` 映射到挂载根下的逻辑目录；映射不上 ⇒ `probe.src` 为空。
    /// `pack_name` 非空 ⇒ 探测该具名挂载（包裹 URI 场景）；为空 ⇒ 默认 "res"。
    static bool ProbeIndexedModelDir(const OSString &model_dir, IndexProbe &probe,
                                     const char *pack_name = nullptr)
    {
        asset::AssetSource *s = GetMountedAssetSource(pack_name);
        if (!s || !s->HasIndexTable())
            return false;

        OSString cur;
        if (!hgl::filesystem::GetCurrentPath(cur))
            return false;

        OSString root;
        if (pack_name && GetAssetMountRoot(AnsiString(pack_name), root))
        {
            if (!LooksAbsoluteOsPath(root))
                root = hgl::filesystem::JoinPathWithFilename(cur, root);
        }
        else
            root = hgl::filesystem::JoinPathWithFilename(cur,
                                                         OSString(DEFAULT_ASSET_SOURCE_ROOT));
        probe.root = root;
        const OSString dir_abs = LooksAbsoluteOsPath(model_dir)
            ? model_dir
            : hgl::filesystem::JoinPathWithFilename(cur, model_dir);

        if (!MakeLogicalDirUnderRoot(probe.root, dir_abs, probe.logical_dir))
            return false;

        probe.src = s;
        return true;
    }
}

bool ReadModelPackMaterialNamesFromBytes(const void               *pack_bytes,
                                         const uint32              pack_size,
                                         const OSString           &pack_name,
                                         std::vector<std::string> &out_names)
{
    out_names.clear();

    if (!pack_bytes || pack_size == 0)
        return false;

    hgl::io::minipack::MiniPackMemory *mpm =
        hgl::io::minipack::GetMiniPackMemoryFromBytes(pack_name, pack_bytes, pack_size);
    if (!mpm)
    {
        MLogError(LoadStaticMesh,
            OS_TEXT("ReadModelPackMaterialNamesFromBytes: 无法打开模型包字节 ") + pack_name);
        return false;
    }

    out_names = ReadModelPackMaterialNames(mpm);
    delete mpm;
    return !out_names.empty();
}

bool ResolveSelfContainedModelMaterials(
    GraphicsContext                &ctx,
    const std::vector<std::string> &material_names,
    const OSString                 &model_dir,
    const OSString                 &material_data_block_hint,
    SelfContainedModelMaterials    &out)
{
    out = SelfContainedModelMaterials();

    if (material_names.empty() || model_dir.IsEmpty())
    {
        MLogError(LoadStaticMesh,
            OS_TEXT("ResolveSelfContainedModelMaterials: 材质名列表或模型目录为空"));
        return false;
    }

    // 内存索引探测：只用于圈定路径口径；装载与否由注册表说了算。
    IndexProbe probe;
    const bool indexed = ProbeIndexedModelDir(model_dir, probe);

    asset::AssetSource  *idx_src     = indexed ? probe.src : nullptr;
    const OSString       idx_root    = probe.root;
    const U8String       logical_dir = probe.logical_dir;

    // 3. 配方集合：只认注册表 —— 索引口径命中的路径先问表，空手回退
    //    「注册表全量 + 目录前缀过滤」。本函数永不触发装载（碰盘由调用方负责）。
    std::vector<const mtl::MaterialRecipe *> dir_recipes;
    const mtl::MaterialRecipeRegistry *registry = ctx.GetMaterialRecipeRegistry();
    if (!registry)
    {
        MLogError(LoadStaticMesh,
            OS_TEXT("ResolveSelfContainedModelMaterials: GraphicsContext 注册表未就绪"));
        return false;
    }

    if (idx_src)
    {
        hgl::ManagedArray<hgl::asset::AssetIndexEntry> recipe_entries;
        idx_src->FindInDirectory(logical_dir, hgl::asset::AssetType::MaterialRecipe, recipe_entries);
        for (int i = 0; i < recipe_entries.GetCount(); ++i)
        {
            const OSString path = idx_root + OS_TEXT("/") +
                                  hgl::ToOSString(recipe_entries[i]->physical_path);
            const mtl::MaterialRecipeAsset *asset = registry->FindAssetByPath(path);
            if (asset) dir_recipes.push_back(&asset->GetRecipe());
        }
    }
    if (dir_recipes.empty())
    {
        for (int i = 0; i < registry->GetCount(); ++i)
        {
            const mtl::MaterialRecipeAsset *asset = registry->GetAssetAt(i);
            if (asset && HasOsCharPrefixI(asset->GetSourcePath().c_str(), model_dir.c_str()))
                dir_recipes.push_back(&asset->GetRecipe());
        }
    }
    if (dir_recipes.empty())
    {
        MLogError(LoadStaticMesh,
            OS_TEXT("ResolveSelfContainedModelMaterials: 模型目录的配方尚未装载 ") + model_dir);
        return false;
    }

    // 4. 材质数据块：同样只认注册表 —— 索引口径先问表；空手回退 hint 直查 /
    //    「注册表全量 + 目录前缀 + 块扩展名」过滤。命中判据与车道落地键同源
    //    （`FindAssetByPath` 折叠绝对路径/大小写/分隔符，口径拼写差异无害）。
    mtl::MaterialDataBlockRegistry *blocks = ctx.GetMaterialDataBlockRegistry();
    GlobalSSBOBufferRegistry       *pool   = ctx.GetGlobalSSBOBufferRegistry();
    if (!blocks || !pool)
    {
        MLogError(LoadStaticMesh,
            OS_TEXT("ResolveSelfContainedModelMaterials: GraphicsContext 注册表未就绪"));
        return false;
    }

    OSString block_uri;
    if (idx_src)
    {
        hgl::ManagedArray<hgl::asset::AssetIndexEntry> block_entries;
        idx_src->FindInDirectory(logical_dir, hgl::asset::AssetType::MaterialData, block_entries);

        OSString cand;
        if (!material_data_block_hint.IsEmpty())
        {
            const U8String want = logical_dir + U8String(u8"/") +
                                  hgl::ToU8String(material_data_block_hint);
            for (int i = 0; i < block_entries.GetCount(); ++i)
            {
                if (U8FoldedEqual(block_entries[i]->logical_path, want))
                {
                    cand = idx_root + OS_TEXT("/") +
                           hgl::ToOSString(block_entries[i]->physical_path);
                    break;
                }
            }
        }
        else if (block_entries.GetCount() == 1)
        {
            cand = idx_root + OS_TEXT("/") +
                   hgl::ToOSString(block_entries[0]->physical_path);
        }

        if (!cand.IsEmpty() && blocks->FindAssetByPath(cand))
            block_uri = OSString(OS_TEXT("mtldata:file:")) + cand;
    }

        if (block_uri.IsEmpty())
        {
            if (!material_data_block_hint.IsEmpty())
            {
                const OSString want = model_dir + OS_TEXT("\\") +
                                      material_data_block_hint +
                                      OS_TEXT(".material_data.toml");
            if (blocks->FindAssetByPath(want))
                block_uri = OSString(OS_TEXT("mtldata:file:")) + want;
        }
        else
        {
            int found = 0;
            for (int i = 0; i < blocks->GetCount(); ++i)
            {
                mtl::MaterialDataBlockAsset *b = blocks->GetAssetAt(i);
                if (!b || b->GetSourcePath().IsEmpty())
                    continue;
                if (!HasOsCharPrefixI(b->GetSourcePath().c_str(), model_dir.c_str()))
                    continue;
                const OSString &p = b->GetSourcePath();
                if (!p.EndsWith(OS_TEXT(".material_data.toml"))
                 && !p.EndsWith(OS_TEXT(".mdb"))
                 && !p.EndsWith(OS_TEXT(".fb")))
                    continue;
                block_uri = OSString(OS_TEXT("mtldata:file:")) + p;
                ++found;
            }
            if (found > 1)
            {
                MLogError(LoadStaticMesh,
                    OS_TEXT("ResolveSelfContainedModelMaterials: 模型目录有多个 material_data 块，必须指定块名: ")
                    + model_dir);
                return false;
            }
        }
    }

    if (block_uri.IsEmpty())
    {
        MLogError(LoadStaticMesh,
            OS_TEXT("ResolveSelfContainedModelMaterials: 模型目录的 material_data 块尚未装载 ") + model_dir);
        return false;
    }

    // 5. 逐材质：名字匹配配方 + 行号 = 材质下标取绑定
    const size_t count = material_names.size();
    out.recipes.resize(count);
    out.bindings.resize(count);
    out.table.resize(count, nullptr);

    for (size_t i = 0; i < count; ++i)
    {
        const mtl::MaterialRecipe *picked = nullptr;
        for (const mtl::MaterialRecipe *r : dir_recipes)
        {
            if (RecipeMatchesMaterialName(*r, material_names[i]))
            {
                picked = r;
                break;
            }
        }
        if (!picked && dir_recipes.size() == 1)
            picked = dir_recipes[0];   // 单配方目录（单材质模型的常见形态）
        if (!picked)
        {
            MLogError(LoadStaticMesh,
                OS_TEXT("ResolveSelfContainedModelMaterials: 目录内没有配方匹配材质 ")
                + model_dir + OS_TEXT(" material=") + hgl::ToOSString(material_names[i].c_str()));
            return false;
        }

        GlobalSSBOBinding binding{};
        if (!blocks->GetRowBinding(pool, block_uri, static_cast<uint32_t>(i), binding))
        {
            MLogError(LoadStaticMesh,
                OS_TEXT("ResolveSelfContainedModelMaterials: 材质数据行绑定失败 row=")
                + OSString::numberOf(static_cast<int>(i)) + OS_TEXT(" block=") + block_uri);
            return false;
        }

        out.recipes[i] = *picked;
        out.recipes[i].material_ssbo_binding = binding;
        out.bindings[i] = binding;
        out.table[i] = &out.recipes[i];

        MLogInfo(LoadStaticMesh,
            OS_TEXT("ResolveSelfContainedModelMaterials: material[")
            + OSString::numberOf(static_cast<int>(i)) + OS_TEXT("] ")
            + hgl::ToOSString(material_names[i].c_str())
            + OS_TEXT(" → ") + hgl::ToOSString(picked->recipe_name.c_str()));
    }

    out.fallback = &out.recipes[0];
    MLogInfo(LoadStaticMesh,
        OS_TEXT("ResolveSelfContainedModelMaterials: ") + model_dir
        + OS_TEXT(" materials=") + OSString::numberOf(static_cast<int>(count))
        + OS_TEXT(" block=") + block_uri);
    return true;
}

bool BuildSelfContainedModelMaterials(
    GraphicsContext             &ctx,
    const OSString              &pack_path,
    const char                  *material_data_block_hint,
    SelfContainedModelMaterials &out)
{
    out = SelfContainedModelMaterials();

    // 「扫模型目录找配方与数据块」需要磁盘目录概念：包裹 URI 先经挂载索引
    // 反查转成物理路径（目录型包裹的包内路径就是真实文件树）；转译失败
    // （归档型包裹 / 未挂载 / 身份缺失）明确报错，而不是留下误导性的「文件打不开」。
    const hgl::asset::AssetURI uri = hgl::asset::AssetURI::Parse(pack_path);
    OSString model_path = pack_path;
    if (uri.IsPack())
    {
        if (!ResolveAssetPhysicalPath(pack_path, model_path) || model_path.IsEmpty())
        {
            MLogError(LoadStaticMesh,
                OS_TEXT("BuildSelfContainedModelMaterials: 包裹 URI 无法转译物理路径: ") + pack_path);
            return false;
        }
    }

    // 模型目录 = pack 所在目录（唯一的资源来源目录）
    const OSString model_dir = hgl::filesystem::Path(model_path).GetParent().ToOSString();
    if (model_dir.IsEmpty())
    {
        MLogError(LoadStaticMesh, OS_TEXT("BuildSelfContainedModelMaterials: 无法取得模型目录 ") + pack_path);
        return false;
    }

    hgl::io::minipack::MiniPackMemory *mpm =
        hgl::io::minipack::GetMiniPackMemory(model_path);
    if (!mpm)
    {
        MLogError(LoadStaticMesh, OS_TEXT("BuildSelfContainedModelMaterials: 无法打开模型包 ") + pack_path);
        return false;
    }
    std::vector<std::string> material_names = ReadModelPackMaterialNames(mpm);
    delete mpm;
    if (material_names.empty())
    {
        MLogError(LoadStaticMesh,
            OS_TEXT("BuildSelfContainedModelMaterials: MaterialTable 缺失或为空（非 MDL2 模型包）")
            + pack_path);
        return false;
    }

    // 装载触发段：本节是全函数唯一碰盘的地方。索引在档的配方与数据块逐个补载；
    // 无索引 / 索引空手 ⇒ 整目录装载（老行为）。数据块只建句柄不碰 GPU
    // （上传仍由 `GetRowBinding` → `GetOrLoadURI` 惰性执行）。异步路径不走这里——
    // 配方与数据块由材质车道扇出落地，门禁放行后直接调 `ResolveSelfContainedModelMaterials`。
    //
    // 补载优先走材质车道（读盘+解析在工作线程，本线程只泵提交）；车道起不来
    // （无设备/工具进程）⇒ 退回主线程同步装载。本函数是阻塞式一次性调用，
    // 投递后原地泵到在途清零再核验落地。
    IndexProbe probe;
    bool landed_from_index = false;
    bool blocks_from_index = false;
    mtl::MaterialRecipeRegistry    *registry = ctx.GetMaterialRecipeRegistry();
    mtl::MaterialDataBlockRegistry *blocks   = ctx.GetMaterialDataBlockRegistry();
    mtl::MaterialAsyncLoadLane     *lane     = ctx.GetMaterialAsyncLoadLane();
    const bool use_lane = (registry || blocks) && lane && lane->EnsureStarted();

    // 原地泵送：IO 完成回调 + 车道解析成品提交，直到在途清零（或预算耗尽）。
    auto drain_lane = [&]()
    {
        hgl::LoadedFileManager *file_manager = ctx.GetLoadedFileManager();
        for (int spin = 0; ctx.IsMaterialAsyncLoadPending(); ++spin)
        {
            if (spin >= 15000)   // ≈30s 预算：本地盘的配方批次应在秒级内落地
            {
                MLogWarning(LoadStaticMesh,
                    OS_TEXT("BuildSelfContainedModelMaterials: 材质车道在途超时，按已落地内容继续"));
                break;
            }
            if (file_manager)
                file_manager->Pump();
            ctx.PumpMaterialAsyncLoadLane();
            hgl::SleepSecond(0.002);
        }
        if (registry)
            registry->FinalizePending();   // 批次边界收尾：子配方可能先于父配方落地
    };

    hgl::ManagedArray<OSString> recipe_paths;
    hgl::ManagedArray<OSString> block_paths;
    if (ProbeIndexedModelDir(model_dir, probe,
                             uri.IsPack() ? uri.pack_name.c_str() : nullptr))
    {
        hgl::ManagedArray<hgl::asset::AssetIndexEntry> entries;
        probe.src->FindInDirectory(probe.logical_dir,
                                   hgl::asset::AssetType::MaterialRecipe, entries);
        for (int i = 0; i < entries.GetCount(); ++i)
            recipe_paths.Create(probe.root + OS_TEXT("/") +
                                hgl::ToOSString(entries[i]->physical_path));

        // 数据块：把本目录索引在档的块全部补载进注册表，选哪一块由
        // `ResolveSelfContainedModelMaterials` 按 hint / 唯一性在注册表里定夺。
        if (blocks)
        {
            entries.Clear();
            probe.src->FindInDirectory(probe.logical_dir,
                                       hgl::asset::AssetType::MaterialData, entries);
            for (int i = 0; i < entries.GetCount(); ++i)
                block_paths.Create(probe.root + OS_TEXT("/") +
                                   hgl::ToOSString(entries[i]->physical_path));
        }
    }

    bool any_posted = false;
    if (registry)
    {
        for (int i = 0; i < recipe_paths.GetCount(); ++i)
        {
            const OSString &path = *recipe_paths[i];
            if (registry->FindAssetByPath(path))
            {
                landed_from_index = true;
                continue;
            }
            if (use_lane)
            {
                if (!lane->BeginRecipeFile(path))
                    MLogWarning(LoadStaticMesh,
                        OS_TEXT("BuildSelfContainedModelMaterials: 索引配方投递失败 ") + path);
                any_posted = true;
                continue;
            }
            if (!ctx.LoadMaterialRecipeFile(path))
            {
                MLogWarning(LoadStaticMesh,
                    OS_TEXT("BuildSelfContainedModelMaterials: 索引配方加载失败 ") + path);
                continue;
            }
            landed_from_index = true;
        }
        if (!use_lane && recipe_paths.GetCount() > 0)
            registry->FinalizePending();   // 连续 LoadFile 的收尾：子配方可能先于父配方
    }
    if (blocks)
    {
        for (int i = 0; i < block_paths.GetCount(); ++i)
        {
            const OSString &path = *block_paths[i];
            if (blocks->FindAssetByPath(path))
            {
                blocks_from_index = true;
                continue;
            }
            if (use_lane)
            {
                if (!lane->BeginDataBlockFile(path))
                    MLogWarning(LoadStaticMesh,
                        OS_TEXT("BuildSelfContainedModelMaterials: 索引数据块投递失败 ") + path);
                any_posted = true;
                continue;
            }
            if (!blocks->LoadFile(path))
            {
                MLogWarning(LoadStaticMesh,
                    OS_TEXT("BuildSelfContainedModelMaterials: 索引数据块加载失败 ") + path);
                continue;
            }
            blocks_from_index = true;
        }
    }
    MLogInfo(LoadStaticMesh,
        OS_TEXT("BuildSelfContainedModelMaterials: lane=") + OSString::numberOf(use_lane ? 1 : 0)
            + OS_TEXT(" idxRecipes=") + OSString::numberOf(recipe_paths.GetCount())
            + OS_TEXT(" idxBlocks=") + OSString::numberOf(block_paths.GetCount())
            + OS_TEXT(" posted=") + OSString::numberOf(any_posted ? 1 : 0));
    if (use_lane && any_posted)
    {
        drain_lane();
        for (int i = 0; i < recipe_paths.GetCount(); ++i)
            if (registry && registry->FindAssetByPath(*recipe_paths[i]))
                landed_from_index = true;
        for (int i = 0; i < block_paths.GetCount(); ++i)
            if (blocks && blocks->FindAssetByPath(*block_paths[i]))
                blocks_from_index = true;
    }
    if (!landed_from_index)
    {
        if (use_lane)
        {
            if (lane->BeginRecipeDirectory(model_dir) < 0)
            {
                MLogError(LoadStaticMesh, OS_TEXT("BuildSelfContainedModelMaterials: 配方目录投递失败 ") + model_dir);
                return false;
            }
            drain_lane();
        }
        else
        {
            int recipe_files = 0, recipe_errors = 0;
            if (!ctx.LoadMaterialRecipeDirectory(model_dir, &recipe_files, &recipe_errors))
            {
                MLogError(LoadStaticMesh, OS_TEXT("BuildSelfContainedModelMaterials: 配方目录加载失败 ") + model_dir);
                return false;
            }
        }
    }
    if (blocks && !blocks_from_index)
    {
        if (use_lane)
        {
            if (lane->BeginDataBlockDirectory(model_dir) < 0)
            {
                MLogError(LoadStaticMesh, OS_TEXT("BuildSelfContainedModelMaterials: 数据块目录投递失败 ") + model_dir);
                return false;
            }
            drain_lane();
        }
        else
        {
            int block_files = 0, block_errors = 0;
            if (!blocks->LoadDirectory(model_dir, &block_files, &block_errors))
            {
                MLogError(LoadStaticMesh, OS_TEXT("BuildSelfContainedModelMaterials: 数据块目录加载失败 ") + model_dir);
                return false;
            }
        }
    }

    if (!ResolveSelfContainedModelMaterials(ctx, material_names, model_dir,
                                            hgl::ToOSString(material_data_block_hint), out))
    {
        MLogError(LoadStaticMesh,
            OS_TEXT("BuildSelfContainedModelMaterials: 材质装配失败 ") + pack_path);
        out = SelfContainedModelMaterials();
        return false;
    }

    return true;
}

}//namespace hgl::graph
