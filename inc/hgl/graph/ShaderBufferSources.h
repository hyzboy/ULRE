#pragma once

#include <hgl/graph/ShaderBufferSource.h>
#include <hgl/graph/ubo/UBOShaderSources.h>

namespace hgl::graph::mtl
{
    constexpr const ShaderBufferSource SBS_LocalToWorld{
        DescriptorSetType::Transform, "l2w", "LocalToWorldData"
    };
    constexpr const ShaderBufferSource SBS_LocalToWorldIndexRows{
        DescriptorSetType::Transform, "l2w_index_rows", "LocalToWorldIndexRows"
    };
    constexpr const ShaderBufferSource SBS_MaterialTextureLayerRows{
        DescriptorSetType::Material, "mtl_texture_layer_rows", "TextureLayerRows"
    };
    constexpr const ShaderBufferSource SBS_MaterialDataIndexRows{
        DescriptorSetType::Material, "mtl_data_index_rows", "DataIndexRows"
    };
    constexpr const ShaderBufferSource SBS_JointInfo{
        DescriptorSetType::Transform, "joint", "JointInfo"
    };
}
