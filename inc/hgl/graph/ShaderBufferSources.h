#pragma once

#include <hgl/graph/ShaderBufferSource.h>
#include <hgl/graph/ubo/UBOShaderSources.h>

namespace hgl::graph::mtl
{
    constexpr const ShaderBufferSource SBS_LocalToWorld{
        DescriptorSetType::PerObject, "l2w", "LocalToWorldData"
    };
    constexpr const ShaderBufferSource SBS_LocalToWorldIndexRows{
        DescriptorSetType::PerObject, "l2w_index_rows", "LocalToWorldIndexRows"
    };
    constexpr const ShaderBufferSource SBS_MaterialTextureLayerRows{
        DescriptorSetType::Material, "mtl_texture_layer_rows", "TextureLayerRows"
    };
    constexpr const ShaderBufferSource SBS_MaterialDataIndexRows{
        DescriptorSetType::PerObject, "mtl_data_index_rows", "DataIndexRows"
    };
    // 顶点数据 SSBO（MeshShader 方向：顶点输入统一为 SSBO）——每对象大 buffer
    constexpr const ShaderBufferSource SBS_VertexPosition{
        DescriptorSetType::PerObject, "VertexPosition", "VertexPositionData"
    };
    constexpr const ShaderBufferSource SBS_VertexUV{
        DescriptorSetType::PerObject, "VertexUV", "VertexUVData"
    };
    constexpr const ShaderBufferSource SBS_VertexNTB{
        DescriptorSetType::PerObject, "VertexNTB", "VertexNTBData"
    };
    constexpr const ShaderBufferSource SBS_VertexColor{
        DescriptorSetType::PerObject, "VertexColor", "VertexColorData"
    };
    constexpr const ShaderBufferSource SBS_VertexLuminance{
        DescriptorSetType::PerObject, "VertexLuminance", "VertexLuminanceData"
    };
    constexpr const ShaderBufferSource SBS_VertexTransformID{
        DescriptorSetType::PerObject, "VertexTransformID", "VertexTransformIDData"
    };
    constexpr const ShaderBufferSource SBS_VertexSize{
        DescriptorSetType::PerObject, "VertexSize", "VertexSizeData"
    };
    constexpr const ShaderBufferSource SBS_VertexIndex{
        DescriptorSetType::PerObject, "VertexIndex", "VertexIndexData"
    };
    // mesh per-draw 参数表（IndirectMeshDraw：mesh shader 经 gl_DrawID 查表的
    // per-draw 段偏移——替代 per-draw push constant，多 draw 合批的关键）
    constexpr const ShaderBufferSource SBS_MeshDrawParams{
        DescriptorSetType::PerObject, "mesh_draw_params", "MeshDrawParamsData"
    };

    // mesh per-draw 参数行——与 MeshShaderAssembler 生成的 GLSL struct MeshDrawParams
    // 严格同构（std430 全 4 字节成员，24B 无 padding）
    struct MeshDrawParams
    {
        uint32_t index_base;
        uint32_t vertex_base;
        uint32_t is_indexed;
        uint32_t total_vertices;
        float    char_height;       // CharQuad 专用：字符高度（基准线校正）；其余模式恒 0
        uint32_t first_instance;
    };
    static_assert(sizeof(MeshDrawParams) == 24, "MeshDrawParams must match GLSL std430 layout");
}
