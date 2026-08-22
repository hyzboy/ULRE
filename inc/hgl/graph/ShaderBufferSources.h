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
    constexpr const ShaderBufferSource SBS_JointInfo{
        DescriptorSetType::PerObject, "joint", "JointInfo"
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
    constexpr const ShaderBufferSource SBS_VertexJoint{
        DescriptorSetType::PerObject, "VertexJoint", "VertexJointData"
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
    constexpr const ShaderBufferSource SBS_VertexIndex{
        DescriptorSetType::PerObject, "VertexIndex", "VertexIndexData"
    };
}
