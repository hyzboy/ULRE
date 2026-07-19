#pragma once

#include<hgl/mtl/ShaderBufferSource.h>

namespace hgl::graph{
struct UBODescriptor;
struct SSBODescriptor;
}//namespace hgl::graph

namespace hgl::graph::mtl{

UBODescriptor *CreateUBODescriptor(const ShaderBufferSource &sbs,const uint32_t flag_bits);
SSBODescriptor *CreateSSBODescriptor(const ShaderBufferSource &sbs,const uint32_t flag_bits);

// ShaderGen/private-layout helper only. Runtime resource binding must not use
// struct_name as the main routing key anymore.
const ShaderBufferSource *FindShaderBufferSourceByStructName(const char *struct_name);

constexpr const ShaderBufferSource SBS_ViewportInfo=
{
    DescriptorSetType::Scene,

    "viewport",
    "ViewportInfo"
};

constexpr const ShaderBufferSource SBS_CameraInfo=
{
    DescriptorSetType::Scene,

    "camera",
    "CameraInfo"
};

constexpr const ShaderBufferSource SBS_LocalToWorld=
{
    DescriptorSetType::Transform,

    "l2w",
    "LocalToWorldData"
};

constexpr const ShaderBufferSource SBS_LocalToWorldIndexRows=
{
    DescriptorSetType::Transform,

    "l2w_index_rows",
    "LocalToWorldIndexRows"
};

constexpr const ShaderBufferSource SBS_ColorPattle =
{
    DescriptorSetType::Material,

    "color_pattle",
    "ColorPattle"
};

constexpr const char MaterialInstanceStruct[]="MaterialInstance";

constexpr const ShaderBufferSource SBS_MaterialInstance=
{
    DescriptorSetType::Material,

    "mtl",
    "MaterialInstanceData"
};

constexpr const ShaderBufferSource SBS_MaterialTextureLayerRows=
{
    DescriptorSetType::Material,

    "mtl_texture_layer_rows",
    "TextureLayerRows"
};

constexpr const ShaderBufferSource SBS_MaterialDataIndexRows=
{
    DescriptorSetType::Material,

    "mtl_data_index_rows",
    "DataIndexRows"
};

constexpr const ShaderBufferSource SBS_JointInfo=
{
    DescriptorSetType::Transform,

    "joint",
    "JointInfo"
};

/**
* SkyInfo（全局环境/天空信息）
*/
constexpr const ShaderBufferSource SBS_SkyInfo=
{
    DescriptorSetType::Scene,

    "sky",
    "SkyInfo"
};

}//namespace hgl::graph::mtl
