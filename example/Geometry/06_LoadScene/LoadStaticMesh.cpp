// LoadStaticMesh.cpp — 从 .scene minipack 加载场景树到 StaticMesh
// 此文件暂置于 example/Geometry/，以便与 LoadGeometry.cpp 共享翻译单元。
// 等 LoadGeometry 迁入引擎库后，此文件可随之迁移至 src/SceneGraph/mesh/。

#include <hgl/graph/mesh/LoadStaticMesh.h>
#include <hgl/graph/mesh/StaticMesh.h>
#include <hgl/graph/mesh/Primitive.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/io/MiniPack.h>
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

// LoadGeometry 在同一可执行文件的 LoadGeometry.cpp 中定义
Geometry *LoadGeometry(VulkanDevice *device, const VIL *vil, const OSString &filename);
Geometry *LoadGeometryFromMiniPackBytes(VulkanDevice *device, const VIL *vil, const void *bytes, const uint32 size, const OSString &debug_name);

namespace
{

// ---- packed structures (mirror the exporter layout) ------------------------

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
    MatrixTable    = 7,
    BoundsTable    = 8,
    PrimitiveTable = 9,
    MaterialTable  = 10,
    GeometryTable  = 11,
    StringPool     = 12,
    GeometryViewTable = 13,
    GeometryBlob      = 14,
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
    int32_t local_matrix_index;
    int32_t world_matrix_index;
    int32_t trs_index;
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
    const VIL                *vil,
    MaterialInstance * const *mi_array,
    int                       mi_count,
    GraphicsPipeline                 *default_pipeline,
    const OSString           &pack_path,
    const OSString           &base_dir,
    std::vector<Primitive *> &prim_list,
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
    const SceneTableDesc *matrix_td = FindTableDesc(dir, h.dir_count, SceneTableType::MatrixTable);
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

    const math::Matrix4f *matrices = nullptr;
    uint32_t matrix_count = 0;
    {
        const uint8_t *ptr = nullptr;
        uint32_t size = 0;
        if (TryGetTableRange(payload, h.payload_size, matrix_td, ptr, size))
        {
            matrices = reinterpret_cast<const math::Matrix4f *>(ptr);
            matrix_count = size / static_cast<uint32_t>(sizeof(math::Matrix4f));
        }
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

            Geometry *geo = LoadGeometryFromMiniPackBytes(device, vil, geo_blob, gv[i].blob_size, geo_debug_name);
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

    prim_list.clear();
    {
        const uint8_t *ptr = nullptr;
        uint32_t size = 0;
        if (!TryGetTableRange(payload, h.payload_size, primitive_td, ptr, size))
            return false;
        if (size % sizeof(PackedPrimitive) != 0)
            return false;

        const auto *pp = reinterpret_cast<const PackedPrimitive *>(ptr);
        const uint32_t pcount = size / static_cast<uint32_t>(sizeof(PackedPrimitive));
        prim_list.reserve(pcount);
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
                            prim_list.push_back(nullptr);
                            continue;
                        }

                        const OSString geo_path = base_dir + OS_TEXT("/") + hgl::ToOSString(geo_file);

                        geo = LoadGeometry(device, vil, geo_path);
                        if (!geo)
                        {
                            MLogError(LoadStaticMesh, OS_TEXT("LoadStaticMeshScene: failed to load geometry: ") + geo_path);
                            prim_list.push_back(nullptr);
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
                prim_list.push_back(nullptr);
                continue;
            }

            MaterialInstance *mi = mi_array[(pp[i].material_index >= 0 ? pp[i].material_index : 0) % mi_count];
            Primitive *prim = DirectCreatePrimitive(geo, mi, default_pipeline);
            if (!prim)
            {
                MLogError(LoadStaticMesh, OS_TEXT("LoadStaticMeshScene: DirectCreatePrimitive failed for primitive #") + OSString::numberOf(i) + OS_TEXT(" in ") + pack_path);
                prim_list.push_back(nullptr);
                continue;
            }

            prim_list.push_back(prim);
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

            if (pn[i].local_matrix_index >= 0 && pn[i].local_matrix_index < static_cast<int32_t>(matrix_count))
                node.localMatrix = matrices[pn[i].local_matrix_index];

            if (pn[i].world_matrix_index >= 0 && pn[i].world_matrix_index < static_cast<int32_t>(matrix_count))
                node.worldMatrix = matrices[pn[i].world_matrix_index];

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
//   originalIndex, nameIndex, localMatIndex, worldMatIndex,
//   trsIndex, boundsIndex,
//   primCount, prims[primCount],
//   childCount, children[childCount]

std::vector<StaticMeshNode> ParseNodeList(
    const void *raw, uint32_t bytes,
    const std::vector<std::string>        &names,
    const math::Matrix4f                  *matrices,   uint32_t matrix_count,
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

        int32_t originalIndex, nameIndex, localMatIndex, worldMatIndex, trsIndex, boundsIndex;
        if (!try_read(originalIndex))  break;
        if (!try_read(nameIndex))      break;
        if (!try_read(localMatIndex))  break;
        if (!try_read(worldMatIndex))  break;
        if (!try_read(trsIndex))       break;
        if (!try_read(boundsIndex))    break;

        // Name
        if (nameIndex >= 0 && nameIndex < (int32_t)names.size())
            node.name = names[nameIndex];

        // Local matrix
        if (localMatIndex >= 0 && localMatIndex < (int32_t)matrix_count)
            node.localMatrix = matrices[localMatIndex];

        // World matrix
        if (worldMatIndex >= 0 && worldMatIndex < (int32_t)matrix_count)
            node.worldMatrix = matrices[worldMatIndex];

        // TRS
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

    return nodes;
}

}  // anonymous namespace


// ---- Public API -------------------------------------------------------------

StaticMesh *LoadStaticMeshScene(
    VulkanDevice             *device,
    GeometryManager          *geo_mgr,
    const VIL                *vil,
    MaterialInstance * const *mi_array,
    int                       mi_count,
    GraphicsPipeline                 *default_pipeline,
    const OSString           &pack_path,
    const OSString           &base_dir)
{
    using namespace hgl::io::minipack;
    using namespace hgl::math;

    if (!device || !geo_mgr || !vil || !mi_array || mi_count <= 0 || !default_pipeline)
    {
        MLogError(LoadStaticMesh, OS_TEXT("LoadStaticMeshScene: null argument"));
        return nullptr;
    }

    MiniPackMemory *mpm = GetMiniPackMemory(pack_path);
    if (!mpm)
    {
        MLogError(LoadStaticMesh, OS_TEXT("LoadStaticMeshScene: cannot open pack: ") + pack_path);
        return nullptr;
    }

    // ---- Fast path: ScenePackV2 (header + payload) -------------------------
    {
        std::vector<Primitive *>   prim_list;
        std::vector<StaticMeshNode> scene_nodes;
        std::vector<int32_t>       root_nodes;

        if (TryLoadScene(
                mpm,
                device,
                geo_mgr,
                vil,
                mi_array,
                mi_count,
                default_pipeline,
                pack_path,
                base_dir,
                prim_list,
                scene_nodes,
                root_nodes))
        {
            delete mpm;

            StaticMesh *sm = new StaticMesh();

            for (Primitive *prim : prim_list)
            {
                if (prim)
                    sm->AddPrimitive(prim);
            }

            for (StaticMeshNode &node : scene_nodes)
                sm->AddNode(std::move(node));

            if (!root_nodes.empty())
                sm->SetRootNodes(std::move(root_nodes));

            return sm;
        }
    }

    MLogInfo(LoadStaticMesh, OS_TEXT("LoadStaticMeshScene: ScenePackV2 not available, fallback to V1"));

    // ---- 1. NameTable -------------------------------------------------------
    std::vector<std::string> names;
    {
        const int32 idx = mpm->FindFile(AnsiStringView("NameTable"));
        if (idx >= 0)
            names = ParseNameTable(mpm->Map(idx), mpm->GetFileLength(idx));
    }

    // ---- 2. MatrixTable (raw glm::mat4 array) --------------------------------
    const Matrix4f *matrices     = nullptr;
    uint32          matrix_count = 0;
    {
        const int32 idx = mpm->FindFile(AnsiStringView("MatrixTable"));
        if (idx >= 0)
        {
            matrices     = reinterpret_cast<const Matrix4f *>(mpm->Map(idx));
            matrix_count = mpm->GetFileLength(idx) / static_cast<uint32>(sizeof(Matrix4f));
        }
    }

    // ---- 3. TRSTable (raw PackedTRS array, 40 bytes each) -------------------
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

    // ---- 4. BoundsTable (raw BoundingVolumesData array, 100 bytes each) -----
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

    // ---- 5. RootList (int32 count + int32[count]) ---------------------------
    std::vector<int32_t> root_nodes;
    {
        const int32 idx = mpm->FindFile(AnsiStringView("RootList"));
        if (idx >= 0)
        {
            const int32_t *data     = reinterpret_cast<const int32_t *>(mpm->Map(idx));
            const uint32   byte_len = mpm->GetFileLength(idx);
            if (data && byte_len >= sizeof(int32_t))
            {
                const int32_t count = data[0];
                const int32_t max_readable = static_cast<int32_t>(byte_len / sizeof(int32_t)) - 1;
                root_nodes.reserve(count);
                for (int32_t j = 0; j < count && j < max_readable; ++j)
                    root_nodes.push_back(data[j + 1]);
            }
        }
    }

    // ---- 6. PrimitiveList → load Geometry + create Primitive ----------------
    // Per entry (byte stream): int32 originalIndex, int32 geoIndex, int32 matIndex,
    //                          uint32 fileLen, char[fileLen]  (no NUL terminator)
    std::vector<Primitive *> prim_list;
    {
        const int32 idx = mpm->FindFile(AnsiStringView("PrimitiveList"));
        if (idx >= 0)
        {
            const uint8_t *buf = reinterpret_cast<const uint8_t *>(mpm->Map(idx));
            const uint8_t *end = buf + mpm->GetFileLength(idx);

            while (buf + 3 * sizeof(int32_t) + sizeof(uint32_t) <= end)
            {
                int32_t  originalIndex, geoIndex, matIndex;
                uint32_t fileLen;

                std::memcpy(&originalIndex, buf, sizeof(int32_t));  buf += sizeof(int32_t);
                std::memcpy(&geoIndex,      buf, sizeof(int32_t));  buf += sizeof(int32_t);
                std::memcpy(&matIndex,      buf, sizeof(int32_t));  buf += sizeof(int32_t);
                std::memcpy(&fileLen,       buf, sizeof(uint32_t)); buf += sizeof(uint32_t);

                if (buf + fileLen > end)
                {
                    MLogError(LoadStaticMesh,
                        OS_TEXT("LoadStaticMeshScene: PrimitiveList entry truncated in ") + pack_path);
                    break;
                }

                std::string geo_file(reinterpret_cast<const char *>(buf), fileLen);
                buf += fileLen;

                const OSString geo_path = base_dir + OS_TEXT("/") + hgl::ToOSString(geo_file);

                Geometry *geo = LoadGeometry(device, vil, geo_path);
                if (!geo)
                {
                    MLogError(LoadStaticMesh,
                        OS_TEXT("LoadStaticMeshScene: failed to load geometry: ") + geo_path);
                    prim_list.push_back(nullptr);
                    continue;
                }

                geo_mgr->Add(geo);

                MaterialInstance *mi = mi_array[(matIndex >= 0 ? matIndex : 0) % mi_count];
                Primitive *prim = DirectCreatePrimitive(geo, mi, default_pipeline);
                if (!prim)
                {
                    MLogError(LoadStaticMesh,
                        OS_TEXT("LoadStaticMeshScene: DirectCreatePrimitive failed for ") + geo_path);
                    prim_list.push_back(nullptr);
                    continue;
                }

                prim_list.push_back(prim);
            }
        }
    }

    // ---- 7. NodeList --------------------------------------------------------
    std::vector<StaticMeshNode> scene_nodes;
    {
        const int32 idx = mpm->FindFile(AnsiStringView("NodeList"));
        if (idx >= 0)
        {
            scene_nodes = ParseNodeList(
                mpm->Map(idx), mpm->GetFileLength(idx),
                names,
                matrices,   matrix_count,
                trs_arr,    trs_count,
                bounds_arr, bounds_count);
        }
    }

    delete mpm;

    // ---- 8. Assemble StaticMesh ---------------------------------------------
    StaticMesh *sm = new StaticMesh();

    for (Primitive *prim : prim_list)
    {
        if (prim)
            sm->AddPrimitive(prim);
    }

    for (StaticMeshNode &node : scene_nodes)
        sm->AddNode(std::move(node));

    if (!root_nodes.empty())
        sm->SetRootNodes(std::move(root_nodes));

    return sm;
}

}  // namespace hgl::graph
