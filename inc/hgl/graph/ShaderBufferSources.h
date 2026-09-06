#pragma once

#include <hgl/graph/ShaderBufferSource.h>
#include <hgl/graph/ubo/UBOShaderSources.h>
#include <cstddef>

namespace hgl::graph::mtl
{
    constexpr const ShaderBufferSource SBS_LocalToWorld{
        DescriptorSetType::PerObject, "l2w", "LocalToWorldData"
    };
    constexpr const ShaderBufferSource SBS_LocalToWorldIndex{
        DescriptorSetType::PerObject, "l2w_index", "LocalToWorldIndex"
    };
    constexpr const ShaderBufferSource SBS_MaterialTextureLayerRows{
        DescriptorSetType::Material,  "mtl_texture_layer_rows", "TextureLayerRows"
    };
    // 每 draw 项 → 材质数据行设备地址表（8B 行，W3.3 起为唯一行表；
    // 取代旧 mtl_private_data_index 4B 行号表）。
    constexpr const ShaderBufferSource SBS_MaterialDataAddresses{
        DescriptorSetType::PerObject, "mtl_data_addrs", "MaterialDataAddresses"
    };
    // 顶点数据 SSBO（Vertex 集：顶点输入统一为 SSBO，Phase 5 自 PerObject 迁出）——每对象大 buffer
    constexpr const ShaderBufferSource SBS_VertexPosition{
        DescriptorSetType::Vertex, "VertexPosition", "VertexPositionData"
    };
    constexpr const ShaderBufferSource SBS_VertexUV{
        DescriptorSetType::Vertex, "VertexUV", "VertexUVData"
    };
    constexpr const ShaderBufferSource SBS_VertexNTB{
        DescriptorSetType::Vertex, "VertexNTB", "VertexNTBData"
    };
    constexpr const ShaderBufferSource SBS_VertexColor{
        DescriptorSetType::Vertex, "VertexColor", "VertexColorData"
    };
    constexpr const ShaderBufferSource SBS_VertexLuminance{
        DescriptorSetType::Vertex, "VertexLuminance", "VertexLuminanceData"
    };
    constexpr const ShaderBufferSource SBS_VertexTransformID{
        DescriptorSetType::Vertex, "VertexTransformID", "VertexTransformIDData"
    };
    constexpr const ShaderBufferSource SBS_VertexSize{
        DescriptorSetType::Vertex, "VertexSize", "VertexSizeData"
    };
    constexpr const ShaderBufferSource SBS_VertexIndex{
        DescriptorSetType::Vertex, "VertexIndex", "VertexIndexData"
    };
    // mesh per-draw 参数表（IndirectMeshDraw：mesh shader 经 gl_DrawID 查表的
    // per-draw 段偏移——替代 per-draw push constant，多 draw 合批的关键）
    constexpr const ShaderBufferSource SBS_MeshDrawParams{
        DescriptorSetType::PerObject, "mesh_draw_params", "MeshDrawParamsData"
    };

    // mesh per-draw 参数行——与 MeshTemplateEmitter 生成的 GLSL struct MeshDrawParams
    // 严格同构（std430 全 4 字节成员，24B 无 padding）。
    //
    // 单一真源（X 列表）：CPU struct 成员 / GLSL 字段名 / GLSL 字段类型 /
    // std430 布局断言全部从这一份生成——改字段只改这里，GLSL 发射侧
    // （MeshShaderVertexAdapter 的 EmitVertexAdapter）遍历名字+类型表发射，
    // 漂移（改名/调序/漏字段）由下方 static_assert 编译期抓死。
    #define HGL_MESH_DRAW_PARAMS_FIELD_LIST(M)   \
        M(index_base,     "uint",  uint32_t)     \
        M(vertex_base,    "uint",  uint32_t)     \
        M(is_indexed,     "uint",  uint32_t)     \
        M(total_vertices, "uint",  uint32_t)     \
        M(char_height,    "float", float)        \
        M(first_instance, "uint",  uint32_t)     \
        M(addr_position,      "uint64_t", uint64_t) \
        M(addr_uv,            "uint64_t", uint64_t) \
        M(addr_ntb,           "uint64_t", uint64_t) \
        M(addr_color,         "uint64_t", uint64_t) \
        M(addr_luminance,     "uint64_t", uint64_t) \
        M(addr_transform_id,  "uint64_t", uint64_t) \
        M(addr_size,          "uint64_t", uint64_t) \
        M(addr_index,         "uint64_t", uint64_t)

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
        return sizeof(MeshDrawParams) == 88;
    }
    static_assert(MeshDrawParamsLayoutValid(),
        "MeshDrawParams 布局必须与 GLSL std430 声明逐字段一致（24B 头部 + 8×uint64 基址 = 88B）");
}
