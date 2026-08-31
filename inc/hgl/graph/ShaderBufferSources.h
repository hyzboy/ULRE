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
    constexpr const ShaderBufferSource SBS_MaterialPrivateDataIndexRows{
        DescriptorSetType::PerObject, "mtl_private_data_index", "MaterialPrivateDataIndex"
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

    // mesh per-draw 参数行——与 MeshShaderAssembler 生成的 GLSL struct MeshDrawParams
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
        M(first_instance, "uint",  uint32_t)

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

    // std430 布局断言：字段连续 4 字节对齐（无 padding），顺序 = 表顺序。
    constexpr bool MeshDrawParamsStd430Contiguous() noexcept
    {
        const size_t offsets[] =
        {
    #define HGL_MDP_OFFSET_FIELD(name, glsl_type, cpu_type) offsetof(MeshDrawParams, name),
            HGL_MESH_DRAW_PARAMS_FIELD_LIST(HGL_MDP_OFFSET_FIELD)
    #undef HGL_MDP_OFFSET_FIELD
        };
        for (uint32 i = 0; i < kMeshDrawParamsFieldCount; ++i)
        {
            if (offsets[i] != static_cast<size_t>(i) * 4u)
                return false;
        }
        return true;
    }
    static_assert(MeshDrawParamsStd430Contiguous(),
        "MeshDrawParams 字段必须连续 4 字节对齐（std430），顺序必须与字段表一致");
    static_assert(sizeof(MeshDrawParams) == 24, "MeshDrawParams must match GLSL std430 layout");
}
