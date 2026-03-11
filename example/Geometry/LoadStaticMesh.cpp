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

namespace
{

// ---- packed structures (mirror the exporter layout) ------------------------

#pragma pack(push, 1)
// Mirrors exporters::TRS (glm::vec3 + glm::quat + glm::vec3 = 40 bytes)
struct PackedTRS
{
    math::Vector3f translation;
    math::Quatf    rotation;        // binary: x, y, z, w
    math::Vector3f scale;
};
static_assert(sizeof(PackedTRS) == 40, "PackedTRS size mismatch");
#pragma pack(pop)

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
            node.translation = t.translation;
            node.rotation    = t.rotation;
            node.scale       = t.scale;
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
    VulkanDevice     *device,
    GeometryManager  *geo_mgr,
    const VIL        *vil,
    MaterialInstance *default_mi,
    Pipeline         *default_pipeline,
    const OSString   &pack_path,
    const OSString   &base_dir)
{
    using namespace hgl::io::minipack;
    using namespace hgl::math;

    if (!device || !geo_mgr || !vil || !default_mi || !default_pipeline)
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

                Primitive *prim = DirectCreatePrimitive(geo, default_mi, default_pipeline);
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
