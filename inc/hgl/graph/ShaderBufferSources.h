#pragma once

#include <hgl/graph/ShaderBufferSource.h>
#include <hgl/graph/ubo/UBOShaderSources.h>
#include <cstddef>

namespace hgl::graph::mtl
{
    // mesh per-draw 参数行——与 MeshTemplateEmitter 生成的 GLSL struct MeshDrawParams
    // 严格同构（std430 头部 6×4B 成员 + 8×uint64 设备地址尾，共 88B；offsetof 连续断言）。
    //
    // 单一真源（X 列表）：CPU struct 成员 / GLSL 字段名 / GLSL 字段类型 /
    // std430 布局断言全部从这一份生成——改字段只改这里，GLSL 发射侧
    // （MeshShaderVertexAdapter 的 EmitVertexAdapter）遍历名字+类型表发射，
    // 漂移（改名/调序/漏字段）由下方 static_assert 编译期抓死。
    #define HGL_MESH_DRAW_PARAMS_FIELD_LIST(M)   \
        M(index_base,             "uint",     uint32_t) \
        M(vertex_base,            "uint",     uint32_t) \
        M(is_indexed,             "uint",     uint32_t) \
        M(total_vertices,         "uint",     uint32_t) \
        M(char_height,            "float",    float)    \
        M(first_instance,         "uint",     uint32_t) \
        M(addr_position,          "uint64_t", uint64_t) \
        M(addr_uv,                "uint64_t", uint64_t) \
        M(addr_ntb,               "uint64_t", uint64_t) \
        M(addr_color,             "uint64_t", uint64_t) \
        M(addr_luminance,         "uint64_t", uint64_t) \
        M(addr_transform_id,      "uint64_t", uint64_t) \
        M(addr_size,              "uint64_t", uint64_t) \
        M(addr_index,             "uint64_t", uint64_t) \
        M(addr_meshlets,          "uint64_t", uint64_t) \
        M(addr_meshlet_vertices,  "uint64_t", uint64_t) \
        M(addr_meshlet_triangles, "uint64_t", uint64_t)

    struct MeshDrawParams
    {
    #define HGL_MDP_CPU_FIELD(name, glsl_type, cpu_type) cpu_type name;
        HGL_MESH_DRAW_PARAMS_FIELD_LIST(HGL_MDP_CPU_FIELD)

    #undef HGL_MDP_CPU_FIELD
    };

    // GLSL 字段名（发射器遍历，顺序 = std430 布局顺序）
    constexpr const char *const kMeshDrawParamsFieldNames[] =
    {
    #define HGL_MDP_NAME_FIELD(name, glsl_type, cpu_type) #name,
        HGL_MESH_DRAW_PARAMS_FIELD_LIST(HGL_MDP_NAME_FIELD)
    #undef HGL_MDP_NAME_FIELD
    };

    // GLSL 字段类型（std430 scalar/vec 基元）
    constexpr const char *const kMeshDrawParamsFieldGLSLTypes[] =
    {
    #define HGL_MDP_GLSL_FIELD(name, glsl_type, cpu_type) glsl_type,
        HGL_MESH_DRAW_PARAMS_FIELD_LIST(HGL_MDP_GLSL_FIELD)
    #undef HGL_MDP_GLSL_FIELD
    };

    constexpr uint32 kMeshDrawParamsFieldCount =
        static_cast<uint32>(sizeof(kMeshDrawParamsFieldNames) / sizeof(kMeshDrawParamsFieldNames[0]));

    // 布局断言（std430，全标量成员无 padding）：
    //   头部 6×4B（offset 0..20）+ 8×uint64 基址（offset 24 起，8B 步进）= 88B。
    constexpr bool MeshDrawParamsLayoutValid() noexcept
    {
        const size_t offsets[] =
        {
    #define HGL_MDP_OFFSET_FIELD(name, glsl_type, cpu_type) offsetof(MeshDrawParams, name),
            HGL_MESH_DRAW_PARAMS_FIELD_LIST(HGL_MDP_OFFSET_FIELD)
    #undef HGL_MDP_OFFSET_FIELD
        };
        constexpr size_t kCount = sizeof(offsets)/sizeof(offsets[0]);
        // 头部 6 字段必须严格 4B 连续
        for (uint32 i = 0; i < 6; ++i)
        {
            if (offsets[i] != i * 4u)
                return false;
        }
        // 8 个基址字段必须 8B 连续（自 offset 24 起）
        for (uint32 i = 6; i < kCount; ++i)
        {
            if (offsets[i] != 24u + (i - 6u) * 8u)
                return false;
        }
        return sizeof(MeshDrawParams) == 112;
    }
    static_assert(MeshDrawParamsLayoutValid(),
        "MeshDrawParams 布局必须与 GLSL std430 声明逐字段一致（24B 头部 + 11×uint64 基址 = 112B）");

    // mesh per-draw 命令参数行（IndirectMeshDraw）：
    // 几何体的 112B 顶点流参数与 BDA 已池化在全局 MeshDrawParamsPool 中，
    // 每个 draw batch 仅需下发 GeometryID 与 first_instance（8B）。
    #define HGL_MESH_DRAW_COMMAND_FIELD_LIST(M) \
        M(geometry_id,    "uint", uint32_t)     \
        M(first_instance, "uint", uint32_t)

    struct MeshDrawCommand
    {
    #define HGL_MDC_CPU_FIELD(name, glsl_type, cpu_type) cpu_type name;
        HGL_MESH_DRAW_COMMAND_FIELD_LIST(HGL_MDC_CPU_FIELD)
    #undef HGL_MDC_CPU_FIELD
    };

    constexpr const char *const kMeshDrawCommandFieldNames[] =
    {
    #define HGL_MDC_NAME_FIELD(name, glsl_type, cpu_type) #name,
        HGL_MESH_DRAW_COMMAND_FIELD_LIST(HGL_MDC_NAME_FIELD)
    #undef HGL_MDC_NAME_FIELD
    };

    constexpr const char *const kMeshDrawCommandFieldGLSLTypes[] =
    {
    #define HGL_MDC_GLSL_FIELD(name, glsl_type, cpu_type) glsl_type,
        HGL_MESH_DRAW_COMMAND_FIELD_LIST(HGL_MDC_GLSL_FIELD)
    #undef HGL_MDC_GLSL_FIELD
    };

    constexpr uint32 kMeshDrawCommandFieldCount =
        static_cast<uint32>(sizeof(kMeshDrawCommandFieldNames) / sizeof(kMeshDrawCommandFieldNames[0]));

    constexpr bool MeshDrawCommandLayoutValid() noexcept
    {
        return sizeof(MeshDrawCommand) == 8
            && offsetof(MeshDrawCommand, geometry_id) == 0
            && offsetof(MeshDrawCommand, first_instance) == 4;
    }
    static_assert(MeshDrawCommandLayoutValid(), "MeshDrawCommand 布局必须为 2×uint32 (8B)");

    // 每个 draw item 的材质实例索引行（8B）。
    // payload_index 指向全局池（PBRSurface / EmissiveSurface / TransmissionSurface）的行号；
    // texture_reference_index 指向每材质 MaterialTextureReferencePool 的行号。
    struct MaterialInstanceAddresses
    {
        uint32_t payload_index = 0;
        uint32_t texture_reference_index = 0;
    };

    static_assert(sizeof(MaterialInstanceAddresses) == 8);
    static_assert(offsetof(MaterialInstanceAddresses, payload_index) == 0);
    static_assert(offsetof(MaterialInstanceAddresses, texture_reference_index) == 4);

    // ── 根地址表（RootAddresses）——push constant 承载的全局表设备地址 ──────────
    // 全部 SSBO 走 BDA 后，shader 每个 buffer_reference 起点都需要一个地址来源；
    // 8 张表（MeshDrawParams/L2W/L2WIndex/mtl_data_addrs/texture_references/文本三表）的地址
    // 集中在此，渲染路径每 MaterialBatch 渲染前一次 PushConstants 下发（64B）。
    // 单一真源（X 列表）：CPU struct / GLSL 字段名 / GLSL 字段类型从这一份生成，
    // A3 发射 push_constant block 时遍历名字+类型表——改字段只改这里。
    #define HGL_ROOT_ADDRESSES_FIELD_LIST(M)  \
        M(addr_mesh_draw_params,   uint64_t)  \
        M(addr_l2w,                uint64_t)  \
        M(addr_l2w_index,          uint64_t)  \
        M(addr_mtl_data_addrs,     uint64_t)  \
        M(addr_texture_references, uint64_t)  \
        M(addr_text_char_info,     uint64_t)  \
        M(addr_text_char_style,    uint64_t)  \
        M(addr_text_char_instance, uint64_t)

    struct RootAddresses
    {
    #define HGL_RA_CPU_FIELD(name, cpu_type) cpu_type name;
        HGL_ROOT_ADDRESSES_FIELD_LIST(HGL_RA_CPU_FIELD)

    #undef HGL_RA_CPU_FIELD
    };

    // GLSL 字段名（A3 发射 push_constant block 遍历）
    constexpr const char *const kRootAddressesFieldNames[] =
    {
    #define HGL_RA_NAME_FIELD(name, cpu_type) #name,
        HGL_ROOT_ADDRESSES_FIELD_LIST(HGL_RA_NAME_FIELD)
    #undef HGL_RA_NAME_FIELD
    };

    // GLSL 字段类型（全 uint64_t——buffer_reference 地址）
    constexpr const char *const kRootAddressesFieldGLSLTypes[] =
    {
    #define HGL_RA_GLSL_FIELD(name, cpu_type) "uint64_t",
        HGL_ROOT_ADDRESSES_FIELD_LIST(HGL_RA_GLSL_FIELD)
    #undef HGL_RA_GLSL_FIELD
    };

    constexpr uint32 kRootAddressesFieldCount =
        static_cast<uint32>(sizeof(kRootAddressesFieldNames) / sizeof(kRootAddressesFieldNames[0]));

    // 布局断言：全 uint64 标量，无 padding——offset 0..56 按 8B 步进，sizeof == 64
    constexpr bool RootAddressesLayoutValid() noexcept
    {
        const size_t offsets[] =
        {
    #define HGL_RA_OFFSET_FIELD(name, cpu_type) offsetof(RootAddresses, name),
            HGL_ROOT_ADDRESSES_FIELD_LIST(HGL_RA_OFFSET_FIELD)
    #undef HGL_RA_OFFSET_FIELD
        };
        for (uint32 i = 0; i < kRootAddressesFieldCount; ++i)
        {
            if (offsets[i] != i * 8u)
                return false;
        }
        return sizeof(RootAddresses) == 64;
    }
    static_assert(RootAddressesLayoutValid(),
        "RootAddresses 布局必须 8×uint64 连续（offset 0..56，sizeof=64）——与 GLSL push_constant block 逐字段一致");
}
