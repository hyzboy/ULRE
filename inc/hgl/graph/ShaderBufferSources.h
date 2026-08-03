#pragma once

#include <hgl/mtl/ShaderBufferSource.h>
#include <hgl/graph/ubo/UBOShaderSources.h>

namespace hgl::graph
{
    struct UBODescriptor;
    struct SSBODescriptor;
}

namespace hgl::graph::mtl
{
    UBODescriptor *CreateUBODescriptor(const ShaderBufferSource &source, uint32_t flag_bits);
    SSBODescriptor *CreateSSBODescriptor(const ShaderBufferSource &source, uint32_t flag_bits);
    const ShaderBufferSource *FindShaderBufferSourceByStructName(const char *struct_name);

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
