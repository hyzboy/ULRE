// LoadStaticMesh.cpp — 从 .scene minipack 加载场景树到 StaticMesh
// Core scene loader for StaticMesh asset packs.

#include <hgl/graph/mesh/LoadStaticMesh.h>
#include <hgl/graph/geo/LoadGeometry.h>
#include <hgl/graph/mesh/StaticMesh.h>
#include <hgl/math/Matrix.h>      // TranslateMatrix / ToMatrix / ScaleMatrix（TRS→矩阵组合）
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/asset/MaterialDataBlockRegistry.h>
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

#pragma pack(push, 1)
// Mirrors exporters::TRS (glm::vec3 + glm::quat + glm::vec3 = 40 bytes)
// Use plain float arrays to avoid GLM_FORCE_DEFAULT_ALIGNED_GENTYPES padding.
struct PackedTRS
{
    float translation[3];   // 12 bytes
    float rotation[4];      // 16 bytes
    float scale[3];         // 12 bytes
};
static_assert(sizeof(PackedTRS) == 40, "PackedTRS size mismatch");

constexpr uint32_t kScenePackV2Magic = 0x324E4353u; // 'SCN2'

enum class SceneTableType : uint32_t
{
    NameTable      = 1,
    NodeTable      = 2,
    NodePrimIndex  = 3,
    NodeChildIndex = 4,
    RootIndex      = 5,
    TRSTable       = 6,
    BoundsTable    = 7,
    PrimitiveTable = 8,
    MaterialTable  = 9,
    GeometryTable  = 10,
    StringPool     = 11,
    GeometryViewTable = 12,
    GeometryBlob      = 13,
};

struct ScenePackHeader
{
    uint32_t magic;
    uint16_t version_major;
    uint16_t version_minor;
    uint32_t flags;
    int32_t  scene_name_index;

    uint32_t node_count;
    uint32_t root_count;
    uint32_t primitive_count;
    uint32_t material_count;
    uint32_t geometry_count;

    uint32_t dir_offset;
    uint32_t dir_count;
    uint32_t payload_size;
    uint32_t reserved;
};

struct SceneTableDesc
{
    uint32_t type;
    uint32_t offset;
    uint32_t size;
    uint32_t stride;
};

struct PackedNode
{
    int32_t original_index;
    int32_t name_index;
    int32_t trs_index;          // 局部 TRS 表下标；<0 = 单位变换（产物不存矩阵）
    int32_t bounds_index;
    int32_t first_primitive;
    int32_t primitive_count;
    int32_t first_child;
    int32_t child_count;
};

struct PackedPrimitive
{
    int32_t  original_index;
    int32_t  geometry_index;
    int32_t  material_index;
    uint32_t geometry_file_offset;
    uint32_t geometry_file_length;
};

// 与 GLTFConvert SceneExportPack 的 MaterialTable 行布局一致（表行序 = 材质下标）。
struct PackedMaterial
{
    int32_t  original_index;
    uint32_t file_offset;   // 指向 StringPool 的 "<Scene>.<idx>.<Name>.material"
    uint32_t file_length;
};

struct PackedGeometryView
{
    int32_t  original_index;
    uint32_t blob_offset;
    uint32_t blob_size;
    uint32_t blob_align;
};

#pragma pack(pop)

std::vector<std::string> ParseNameTable(const void *raw, uint32_t bytes);

static const SceneTableDesc *FindTableDesc(const SceneTableDesc *dir, uint32_t dir_count, SceneTableType type)
{
    const uint32_t t = static_cast<uint32_t>(type);
    for (uint32_t i = 0; i < dir_count; ++i)
    {
        if (dir[i].type == t)
            return dir + i;
    }
    return nullptr;
}

static bool TryGetTableRange(const uint8_t *payload, uint32_t payload_size, const SceneTableDesc *td, const uint8_t *&ptr, uint32_t &size)
{
    if (!td)
        return false;
    if (td->offset > payload_size || td->size > payload_size || td->offset + td->size > payload_size)
        return false;
    ptr = payload + td->offset;
    size = td->size;
    return true;
}

static bool TryLoadScene(
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
    const int32 header_idx = mpm->FindFile(AnsiStringView("ScenePackHeader"));
    const int32 payload_idx = mpm->FindFile(AnsiStringView("ScenePayload"));

    if (header_idx < 0 || payload_idx < 0)
        return false;

    const void *header_raw = mpm->Map(header_idx);
    const uint32 header_len = mpm->GetFileLength(header_idx);
    const uint8_t *payload = reinterpret_cast<const uint8_t *>(mpm->Map(payload_idx));
    const uint32 payload_len = mpm->GetFileLength(payload_idx);

    if (!header_raw || header_len < sizeof(ScenePackHeader) || !payload)
        return false;

    ScenePackHeader h{};
    std::memcpy(&h, header_raw, sizeof(ScenePackHeader));

    if (h.magic != kScenePackV2Magic)
        return false;
    if (h.version_major != 2)
        return false;
    if (h.payload_size > payload_len)
        return false;
    if (h.dir_offset > h.payload_size || h.dir_count * sizeof(SceneTableDesc) > h.payload_size || h.dir_offset + h.dir_count * sizeof(SceneTableDesc) > h.payload_size)
        return false;

    const auto *dir = reinterpret_cast<const SceneTableDesc *>(payload + h.dir_offset);

    const SceneTableDesc *name_td = FindTableDesc(dir, h.dir_count, SceneTableType::NameTable);
    const SceneTableDesc *node_td = FindTableDesc(dir, h.dir_count, SceneTableType::NodeTable);
    const SceneTableDesc *node_prim_td = FindTableDesc(dir, h.dir_count, SceneTableType::NodePrimIndex);
    const SceneTableDesc *node_child_td = FindTableDesc(dir, h.dir_count, SceneTableType::NodeChildIndex);
    const SceneTableDesc *root_td = FindTableDesc(dir, h.dir_count, SceneTableType::RootIndex);
    const SceneTableDesc *trs_td = FindTableDesc(dir, h.dir_count, SceneTableType::TRSTable);
    const SceneTableDesc *bounds_td = FindTableDesc(dir, h.dir_count, SceneTableType::BoundsTable);
    const SceneTableDesc *primitive_td = FindTableDesc(dir, h.dir_count, SceneTableType::PrimitiveTable);
    const SceneTableDesc *string_pool_td = FindTableDesc(dir, h.dir_count, SceneTableType::StringPool);
    const SceneTableDesc *geo_view_td = FindTableDesc(dir, h.dir_count, SceneTableType::GeometryViewTable);
    const SceneTableDesc *geo_blob_td = FindTableDesc(dir, h.dir_count, SceneTableType::GeometryBlob);

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
                MLogError(LoadStaticMesh, OS_TEXT("LoadStaticMeshScene: invalid GeometryBlob range in ") + pack_path);
                continue;
            }

            const void *geo_blob = gb_ptr + gv[i].blob_offset;

            const OSString geo_debug_name =
                pack_path + OS_TEXT("#GeometryBlob[") + OSString::numberOf(i) + OS_TEXT("]");

            Geometry *geo = LoadGeometryFromMiniPackBytes(device, geometry_vertex_format, geo_blob, gv[i].blob_size, geo_debug_name);
            if (!geo)
            {
                MLogError(LoadStaticMesh, OS_TEXT("LoadStaticMeshScene: failed to load inlined geometry #") + OSString::numberOf(i) + OS_TEXT(" from ") + pack_path);
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
                            MLogError(LoadStaticMesh, OS_TEXT("LoadStaticMeshScene: invalid V2 geometry filename range in ") + pack_path);
                            geometry_list.push_back(nullptr);
                            continue;
                        }

                        const OSString geo_path = base_dir + OS_TEXT("/") + hgl::ToOSString(geo_file);

                        geo = LoadGeometry(device, geometry_vertex_format, geo_path);
                        if (!geo)
                        {
                            MLogError(LoadStaticMesh, OS_TEXT("LoadStaticMeshScene: failed to load geometry: ") + geo_path);
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
                MLogError(LoadStaticMesh, OS_TEXT("LoadStaticMeshScene: missing geometry for primitive #") + OSString::numberOf(i) + OS_TEXT(" in ") + pack_path);
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

// ---- NodeList parser --------------------------------------------------------
// Per-node stream (all int32_t):
//   originalIndex, nameIndex, trsIndex, boundsIndex,
//   primCount, prims[primCount],
//   childCount, children[childCount]
// 局部变换只存 TRS（T5）；无 trsIndex（<0）⇒ 单位变换。

/// 从局部 TRS 沿层级组合出 local/world 矩阵（两份装载路径共用）。
/// 约定：没有 TRS 行的节点 ⇒ 局部变换是单位矩阵（与导出侧 SceneExportNodes.cpp 一致）。
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

std::vector<StaticMeshNode> ParseNodeList(
    const void *raw, uint32_t bytes,
    const std::vector<std::string>        &names,
    const PackedTRS                       *trs_arr,    uint32_t trs_count,
    const math::BoundingVolumesData       *bounds_arr, uint32_t bounds_count)
{
    std::vector<StaticMeshNode> nodes;
    if (!raw || bytes < sizeof(int32_t))
        return nodes;

    const int32_t *cursor = reinterpret_cast<const int32_t *>(raw);
    const int32_t *end    = cursor + bytes / sizeof(int32_t);

    auto try_read = [&](int32_t &out) -> bool
    {
        if (cursor >= end) return false;
        out = *cursor++;
        return true;
    };

    while (cursor < end)
    {
        StaticMeshNode node;

        int32_t originalIndex, nameIndex, trsIndex, boundsIndex;
        if (!try_read(originalIndex))  break;
        if (!try_read(nameIndex))      break;
        if (!try_read(trsIndex))       break;
        if (!try_read(boundsIndex))    break;

        // Name
        if (nameIndex >= 0 && nameIndex < (int32_t)names.size())
            node.name = names[nameIndex];

        // TRS（局部变换的唯一来源；无则单位，见 ComposeNodeMatrices）
        if (trsIndex >= 0 && trsIndex < (int32_t)trs_count)
        {
            const PackedTRS &t = trs_arr[trsIndex];
            node.hasTRS      = true;
            node.translation = math::Vector3f(t.translation[0], t.translation[1], t.translation[2]);
            node.rotation    = math::Quatf(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]);
            node.scale       = math::Vector3f(t.scale[0], t.scale[1], t.scale[2]);
        }

        // Bounds
        if (boundsIndex >= 0 && boundsIndex < (int32_t)bounds_count)
        {
            math::BoundingVolumesData bvd = bounds_arr[boundsIndex];
            bvd.To(&node.nodeBounds);
            node.boundsValid = true;
        }

        // Primitive indices
        int32_t prim_count = 0;
        if (!try_read(prim_count)) break;
        bool ok = true;
        node.primitiveIndices.reserve(prim_count);
        for (int32_t j = 0; j < prim_count; ++j)
        {
            int32_t pi;
            if (!try_read(pi)) { ok = false; break; }
            node.primitiveIndices.push_back(pi);
        }
        if (!ok) break;

        // Children
        int32_t child_count = 0;
        if (!try_read(child_count)) break;
        node.children.reserve(child_count);
        for (int32_t j = 0; j < child_count; ++j)
        {
            int32_t ci;
            if (!try_read(ci)) { ok = false; break; }
            node.children.push_back(ci);
        }
        if (!ok) break;

        nodes.push_back(std::move(node));
    }

    // Back-propagate parent indices from children arrays
    for (int32_t i = 0; i < (int32_t)nodes.size(); ++i)
    {
        for (int32_t c : nodes[i].children)
        {
            if (c >= 0 && c < (int32_t)nodes.size())
                nodes[c].parentIndex = i;
        }
    }

    ComposeNodeMatrices(nodes);          // 产物只存 TRS ⇒ local/world 在这里组合

    return nodes;
}

}  // anonymous namespace


// ---- Public API -------------------------------------------------------------

/// 解析一个**已打开**的 minipack（内存映射或字节视图）。
/// @note 本函数在返回前 `delete mpm`（接管所有权），调用方不得再使用该指针。
static bool BuildSceneFromMiniPack(
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
        MLogError(LoadStaticMesh, OS_TEXT("LoadStaticMeshSceneAsPrimitiveAssets: invalid argument"));
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

    if (!TryLoadScene(
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
        MLogInfo(LoadStaticMesh, OS_TEXT("LoadStaticMeshSceneAsPrimitiveAssets: ScenePackV2 not available, fallback to V1"));

        // V2 若在图元循环后才失败可能留下部分数据；V1 路径全量重建，先清空。
        geometry_list.clear();
        material_indices.clear();

        std::vector<std::string> names;
        {
            const int32 idx = mpm->FindFile(AnsiStringView("NameTable"));
            if (idx >= 0)
                names = ParseNameTable(mpm->Map(idx), mpm->GetFileLength(idx));
        }

        const PackedTRS *trs_arr  = nullptr;
        uint32           trs_count = 0;
        {
            const int32 idx = mpm->FindFile(AnsiStringView("TRSTable"));
            if (idx >= 0)
            {
                trs_arr   = reinterpret_cast<const PackedTRS *>(mpm->Map(idx));
                trs_count = mpm->GetFileLength(idx) / static_cast<uint32>(sizeof(PackedTRS));
            }
        }

        const BoundingVolumesData *bounds_arr  = nullptr;
        uint32                     bounds_count = 0;
        {
            const int32 idx = mpm->FindFile(AnsiStringView("BoundsTable"));
            if (idx >= 0)
            {
                bounds_arr   = reinterpret_cast<const BoundingVolumesData *>(mpm->Map(idx));
                bounds_count = mpm->GetFileLength(idx) / static_cast<uint32>(sizeof(BoundingVolumesData));
            }
        }

        {
            const int32 idx = mpm->FindFile(AnsiStringView("PrimitiveList"));
            if (idx >= 0)
            {
                const uint8_t *buf = reinterpret_cast<const uint8_t *>(mpm->Map(idx));
                const uint8_t *end = buf + mpm->GetFileLength(idx);

                while (buf + 3 * sizeof(int32_t) + sizeof(uint32_t) <= end)
                {
                    int32_t originalIndex, geoIndex, matIndex;
                    uint32_t fileLen;
                    std::memcpy(&originalIndex, buf, sizeof(int32_t));  buf += sizeof(int32_t);
                    std::memcpy(&geoIndex,      buf, sizeof(int32_t));  buf += sizeof(int32_t);
                    std::memcpy(&matIndex,      buf, sizeof(int32_t));  buf += sizeof(int32_t);
                    std::memcpy(&fileLen,       buf, sizeof(uint32_t)); buf += sizeof(uint32_t);
                    (void)originalIndex;
                    (void)geoIndex;

                    if (buf + fileLen > end)
                    {
                        MLogError(LoadStaticMesh,
                            OS_TEXT("LoadStaticMeshSceneAsPrimitiveAssets: PrimitiveList entry truncated in ") + pack_path);
                        break;
                    }
                    material_indices.push_back(matIndex);

                    std::string geo_file(reinterpret_cast<const char *>(buf), fileLen);
                    buf += fileLen;

                    const OSString geo_path = base_dir + OS_TEXT("/") + hgl::ToOSString(geo_file);

                    Geometry *geo = LoadGeometry(device, geometry_vertex_format, geo_path);
                    if (!geo)
                    {
                        MLogError(LoadStaticMesh,
                            OS_TEXT("LoadStaticMeshSceneAsPrimitiveAssets: failed to load geometry: ") + geo_path);
                        geometry_list.push_back(nullptr);
                        continue;
                    }

                    geo_mgr->Add(geo);
                    geometry_list.push_back(geo);
                }
            }
        }

        {
            const int32 idx = mpm->FindFile(AnsiStringView("NodeList"));
            if (idx >= 0)
            {
                scene_nodes = ParseNodeList(
                    mpm->Map(idx), mpm->GetFileLength(idx),
                    names,
                    trs_arr,    trs_count,
                    bounds_arr, bounds_count);
            }
        }

        for (int32_t i = 0; i < int32_t(scene_nodes.size()); ++i)
        {
            if (scene_nodes[i].parentIndex < 0)
                root_nodes.push_back(i);
        }
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

bool LoadStaticMeshSceneAsPrimitiveAssets(
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

    MiniPackMemory *mpm = GetMiniPackMemory(pack_path);
    if (!mpm)
    {
        MLogError(LoadStaticMesh, OS_TEXT("LoadStaticMeshSceneAsPrimitiveAssets: cannot open pack: ") + pack_path);
        return false;
    }

    return BuildSceneFromMiniPack(device,geo_mgr,geometry_vertex_format,recipe,
                                  mpm,pack_path,base_dir,
                                  out_assets,out_nodes,out_root_nodes,
                                  material_recipes,material_recipe_count);
}

bool LoadStaticMeshSceneFromMemory(
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
        MLogError(LoadStaticMesh, OS_TEXT("LoadStaticMeshSceneFromMemory: cannot parse pack: ") + pack_name);
        return false;
    }

    return BuildSceneFromMiniPack(device,geo_mgr,geometry_vertex_format,recipe,
                                  mpm,pack_name,base_dir,
                                  out_assets,out_nodes,out_root_nodes,
                                  material_recipes,material_recipe_count);
}

bool LoadStaticMeshSceneFromStream(
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
        MLogError(LoadStaticMesh, OS_TEXT("LoadStaticMeshSceneFromStream: null stream: ") + pack_name);
        return false;
    }

    // 场景包解析全程需要随机访问（FindFile/Map），因此先整块读入。
    // 这块内存的生命周期严格覆盖下面这一次同步解析，不跨线程保留。
    int64 pack_size = 0;
    void *pack_bytes = hgl::io::LoadFromInputStream(&pack_size, stream);

    if (!pack_bytes || pack_size <= 0)
    {
        delete[] static_cast<char *>(pack_bytes);

        MLogError(LoadStaticMesh, OS_TEXT("LoadStaticMeshSceneFromStream: cannot read stream: ") + pack_name);
        return false;
    }

    const bool result = LoadStaticMeshSceneFromMemory(
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
    std::vector<std::string> ReadScenePackMaterialNames(hgl::io::minipack::MiniPackMemory *mpm)
    {
        std::vector<std::string> names;

        const int32 header_idx  = mpm->FindFile(AnsiStringView("ScenePackHeader"));
        const int32 payload_idx = mpm->FindFile(AnsiStringView("ScenePayload"));
        if (header_idx < 0 || payload_idx < 0)
            return names;

        const void *header_raw  = mpm->Map(header_idx);
        const uint32 header_len = mpm->GetFileLength(header_idx);
        const uint8_t *payload  = reinterpret_cast<const uint8_t *>(mpm->Map(payload_idx));
        const uint32 payload_len = mpm->GetFileLength(payload_idx);
        if (!header_raw || header_len < sizeof(ScenePackHeader) || !payload)
            return names;

        ScenePackHeader h{};
        std::memcpy(&h, header_raw, sizeof(ScenePackHeader));
        if (h.magic != kScenePackV2Magic || h.version_major != 2)
            return names;
        if (h.payload_size > payload_len ||
            h.dir_offset > h.payload_size ||
            h.dir_count * sizeof(SceneTableDesc) > h.payload_size - h.dir_offset)
            return names;
        if (h.material_count == 0)
            return names;

        const auto *dir = reinterpret_cast<const SceneTableDesc *>(payload + h.dir_offset);
        const SceneTableDesc *mat_td  = FindTableDesc(dir, h.dir_count, SceneTableType::MaterialTable);
        const SceneTableDesc *pool_td = FindTableDesc(dir, h.dir_count, SceneTableType::StringPool);
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
}

bool BuildSelfContainedSceneMaterials(
    GraphicsContext             &ctx,
    const OSString              &pack_path,
    const char                  *material_data_block_hint,
    SelfContainedSceneMaterials &out)
{
    out = SelfContainedSceneMaterials();

    // 1. 模型目录 = pack 所在目录（唯一的资源来源目录）
    const OSString model_dir = hgl::filesystem::Path(pack_path).GetParent().ToOSString();
    if (model_dir.IsEmpty())
    {
        MLogError(LoadStaticMesh, OS_TEXT("BuildSelfContainedSceneMaterials: 无法取得模型目录 ") + pack_path);
        return false;
    }

    // 2. 材质表名字（V2 专有）
    hgl::io::minipack::MiniPackMemory *mpm =
        hgl::io::minipack::GetMiniPackMemory(pack_path);
    if (!mpm)
    {
        MLogError(LoadStaticMesh, OS_TEXT("BuildSelfContainedSceneMaterials: 无法打开场景包 ") + pack_path);
        return false;
    }
    std::vector<std::string> material_names = ReadScenePackMaterialNames(mpm);
    delete mpm;
    if (material_names.empty())
    {
        MLogError(LoadStaticMesh,
            OS_TEXT("BuildSelfContainedSceneMaterials: MaterialTable 缺失或为空（V1 包不支持自装配）")
            + pack_path);
        return false;
    }

    // 3. 只扫模型目录的配方（进 GraphicsContext 注册表）
    int recipe_files = 0, recipe_errors = 0;
    if (!ctx.LoadMaterialRecipeDirectory(model_dir, &recipe_files, &recipe_errors))
    {
        MLogError(LoadStaticMesh, OS_TEXT("BuildSelfContainedSceneMaterials: 配方目录加载失败 ") + model_dir);
        return false;
    }

    std::vector<const mtl::MaterialRecipe *> dir_recipes;
    {
        const mtl::MaterialRecipeRegistry *registry = ctx.GetMaterialRecipeRegistry();
        for (int i = 0; i < registry->GetCount(); ++i)
        {
            const mtl::MaterialRecipeAsset *asset = registry->GetAssetAt(i);
            if (asset && HasOsCharPrefixI(asset->GetSourcePath().c_str(), model_dir.c_str()))
                dir_recipes.push_back(&asset->GetRecipe());
        }
    }
    if (dir_recipes.empty())
    {
        MLogError(LoadStaticMesh, OS_TEXT("BuildSelfContainedSceneMaterials: 模型目录没有可用配方 ") + model_dir);
        return false;
    }

    // 4. 材质数据块（同目录唯一，或多份时按文件名 hint 指定）
    OSString block_path;
    if (material_data_block_hint && material_data_block_hint[0])
    {
        block_path = model_dir + OS_TEXT("\\") +
                     hgl::ToOSString(material_data_block_hint) +
                     OS_TEXT(".material_data.toml");
        if (!hgl::filesystem::FileExist(block_path))
        {
            MLogError(LoadStaticMesh, OS_TEXT("BuildSelfContainedSceneMaterials: 指定数据块不存在 ") + block_path);
            return false;
        }
    }
    else
    {
        hgl::ValueArray<hgl::filesystem::FileInfo> file_list;
        if (hgl::filesystem::GetFileInfoList(file_list, model_dir, false, true, false) < 0)
        {
            MLogError(LoadStaticMesh, OS_TEXT("BuildSelfContainedSceneMaterials: 目录扫描失败 ") + model_dir);
            return false;
        }
        int found = 0;
        for (int i = 0; i < file_list.GetCount(); ++i)
        {
            const hgl::filesystem::FileInfo &f = file_list[i];
            if (!f.is_file)
                continue;
            if (!OSString(f.name).EndsWith(OS_TEXT(".material_data.toml")))
                continue;
            block_path = OSString(f.fullname);
            ++found;
        }
        if (found == 0)
        {
            MLogError(LoadStaticMesh, OS_TEXT("BuildSelfContainedSceneMaterials: 模型目录没有 material_data 块 ") + model_dir);
            return false;
        }
        if (found > 1)
        {
            MLogError(LoadStaticMesh,
                OS_TEXT("BuildSelfContainedSceneMaterials: 模型目录有多个 material_data 块，必须指定块名: ")
                + model_dir);
            return false;
        }
    }

    const OSString block_uri = OSString(OS_TEXT("mtldata:file:")) + block_path;

    mtl::MaterialDataBlockRegistry *blocks = ctx.GetMaterialDataBlockRegistry();
    GlobalSSBOBufferRegistry       *pool   = ctx.GetGlobalSSBOBufferRegistry();
    if (!blocks || !pool)
    {
        MLogError(LoadStaticMesh, OS_TEXT("BuildSelfContainedSceneMaterials: GraphicsContext 注册表未就绪"));
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
                OS_TEXT("BuildSelfContainedSceneMaterials: 目录内没有配方匹配材质 ")
                + model_dir + OS_TEXT(" material=") + hgl::ToOSString(material_names[i].c_str()));
            return false;
        }

        GlobalSSBOBinding binding{};
        if (!blocks->GetRowBinding(pool, block_uri, static_cast<uint32_t>(i), binding))
        {
            MLogError(LoadStaticMesh,
                OS_TEXT("BuildSelfContainedSceneMaterials: 材质数据行绑定失败 row=")
                + OSString::numberOf(static_cast<int>(i)) + OS_TEXT(" block=") + block_path);
            return false;
        }

        out.recipes[i] = *picked;
        out.recipes[i].material_ssbo_binding = binding;
        out.bindings[i] = binding;
        out.table[i] = &out.recipes[i];

        MLogInfo(LoadStaticMesh,
            OS_TEXT("BuildSelfContainedSceneMaterials: material[")
            + OSString::numberOf(static_cast<int>(i)) + OS_TEXT("] ")
            + hgl::ToOSString(material_names[i].c_str())
            + OS_TEXT(" → ") + hgl::ToOSString(picked->recipe_name.c_str()));
    }

    out.fallback = &out.recipes[0];
    MLogInfo(LoadStaticMesh,
        OS_TEXT("BuildSelfContainedSceneMaterials: ") + pack_path
        + OS_TEXT(" materials=") + OSString::numberOf(static_cast<int>(count))
        + OS_TEXT(" block=") + block_path);
    return true;
}

}//namespace hgl::graph
