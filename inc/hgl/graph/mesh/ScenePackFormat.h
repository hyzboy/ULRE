// ScenePackFormat.h — SCN2 场景包（`.scene` minipack）二进制词汇表
//
// 这里的结构与 GLTFConvert 的 SceneExportPack 一一对应，任何一侧改动都必须同步。
// 历史上这份声明是 LoadStaticMesh.cpp 匿名命名空间里的唯一副本；依赖图提取
// （阶段三）与异步场景流送（阶段四）都要读同一张表目录，所以提到头文件，
// 避免出现第二份会漂移的副本。

#pragma once

#include <cstdint>
#include <cstring>

namespace hgl::graph::scenepack
{
    constexpr uint32_t kScenePackV2Magic = 0x324E4353u; // 'SCN2'

    enum class SceneTableType : uint32_t
    {
        NameTable         = 1,
        NodeTable         = 2,
        NodePrimIndex     = 3,
        NodeChildIndex    = 4,
        RootIndex         = 5,
        TRSTable          = 6,
        BoundsTable       = 7,
        PrimitiveTable    = 8,
        MaterialTable     = 9,
        GeometryTable     = 10,
        StringPool        = 11,
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

#pragma pack(push, 1)
    // 与 exporters::TRS 一致（glm::vec3 + glm::quat + glm::vec3 = 40 字节）。
    // 用裸 float 数组而不是 glm 类型，避免 GLM_FORCE_DEFAULT_ALIGNED_GENTYPES 引入填充。
    struct PackedTRS
    {
        float translation[3];   // 12
        float rotation[4];      // 16
        float scale[3];         // 12
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

    static_assert(sizeof(PackedTRS) == 40, "PackedTRS size mismatch");

    inline const SceneTableDesc *FindTableDesc(const SceneTableDesc *dir, uint32_t dir_count, SceneTableType type)
    {
        const uint32_t t = static_cast<uint32_t>(type);
        for (uint32_t i = 0; i < dir_count; ++i)
        {
            if (dir[i].type == t)
                return dir + i;
        }
        return nullptr;
    }

    inline bool TryGetTableRange(const uint8_t *payload, uint32_t payload_size, const SceneTableDesc *td, const uint8_t *&ptr, uint32_t &size)
    {
        if (!td)
            return false;
        if (td->offset > payload_size || td->size > payload_size || td->offset + td->size > payload_size)
            return false;
        ptr = payload + td->offset;
        size = td->size;
        return true;
    }

    /// 校验 `ScenePackHeader` 条目与 `ScenePayload` 条目，并解出表目录。
    /// 两个条目在 minipack 里是分开的：header 独立存储，payload 只含表数据。
    /// 成功时 `out_dir` 指向 payload 内部，生命周期与 payload 相同。
    inline bool ReadPayloadDirectory(const void *header_raw, uint32_t header_len,
                                     const void *payload_raw, uint32_t payload_len,
                                     ScenePackHeader &out_header, const SceneTableDesc *&out_dir)
    {
        out_dir = nullptr;

        const uint8_t *payload = reinterpret_cast<const uint8_t *>(payload_raw);
        if (!header_raw || !payload)
            return false;
        if (header_len < sizeof(ScenePackHeader))
            return false;

        std::memcpy(&out_header, header_raw, sizeof(ScenePackHeader));

        if (out_header.magic != kScenePackV2Magic)
            return false;
        if (out_header.version_major != 2)
            return false;
        if (out_header.payload_size > payload_len)
            return false;

        const uint64_t dir_bytes = static_cast<uint64_t>(out_header.dir_count) * sizeof(SceneTableDesc);
        if (out_header.dir_offset > out_header.payload_size
            || dir_bytes > out_header.payload_size
            || static_cast<uint64_t>(out_header.dir_offset) + dir_bytes > out_header.payload_size)
            return false;

        out_dir = reinterpret_cast<const SceneTableDesc *>(payload + out_header.dir_offset);
        return true;
    }
}//namespace hgl::graph::scenepack
