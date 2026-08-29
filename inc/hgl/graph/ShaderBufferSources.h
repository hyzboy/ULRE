#pragma once

#include <hgl/graph/ShaderBufferSource.h>
#include <hgl/graph/ubo/UBOShaderSources.h>

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
