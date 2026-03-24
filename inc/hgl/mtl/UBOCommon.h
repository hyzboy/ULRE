#pragma once

#include<hgl/mtl/ShaderBufferSource.h>
#include<hgl/mtl/DescriptorBindingContract.h>

namespace hgl::graph{
struct UBODescriptor;
struct SSBODescriptor;
}//namespace hgl::graph

namespace hgl::graph::mtl{

UBODescriptor *CreateUBODescriptor(const UBODescriptorSemantic semantic,const uint32_t flag_bits);
SSBODescriptor *CreateSSBODescriptor(const SSBODescriptorSemantic semantic,const uint32_t flag_bits);
const ShaderBufferSource *FindShaderBufferSourceByStructName(const char *struct_name);

constexpr ShaderBufferSource MakeShaderBufferSourceBySemantic(const UBODescriptorSemantic semantic)
{
    const auto &meta = GetDescriptorSemanticMeta(semantic);

    return ShaderBufferSource{
        meta.set_type,
        DescriptorKind::UBO,
        semantic,
        SSBODescriptorSemantic::Unknown,
        meta.name,
        meta.struct_name,
    };
}

constexpr ShaderBufferSource MakeShaderBufferSourceBySemantic(const SSBODescriptorSemantic semantic)
{
    const auto &meta = GetDescriptorSemanticMeta(semantic);

    return ShaderBufferSource{
        meta.set_type,
        DescriptorKind::SSBO,
        UBODescriptorSemantic::Unknown,
        semantic,
        meta.name,
        meta.struct_name,
    };
}

inline constexpr ShaderBufferSource SBS_ViewportInfo = MakeShaderBufferSourceBySemantic(UBODescriptorSemantic::ViewportInfo);
inline constexpr ShaderBufferSource SBS_CameraInfo = MakeShaderBufferSourceBySemantic(UBODescriptorSemantic::CameraInfo);
inline constexpr ShaderBufferSource SBS_LocalToWorld = MakeShaderBufferSourceBySemantic(SSBODescriptorSemantic::LocalToWorld);
inline constexpr ShaderBufferSource SBS_TransformID = MakeShaderBufferSourceBySemantic(SSBODescriptorSemantic::TransformID);
inline constexpr ShaderBufferSource SBS_MaterialInstanceID = MakeShaderBufferSourceBySemantic(SSBODescriptorSemantic::MaterialInstanceID);
inline constexpr ShaderBufferSource SBS_ColorPattle = MakeShaderBufferSourceBySemantic(UBODescriptorSemantic::ColorPattle);

constexpr const char MaterialInstanceStruct[]="MaterialInstance";

inline constexpr ShaderBufferSource SBS_MaterialInstance = MakeShaderBufferSourceBySemantic(SSBODescriptorSemantic::MaterialInstance);
inline constexpr ShaderBufferSource SBS_JointInfo = MakeShaderBufferSourceBySemantic(SSBODescriptorSemantic::BoneJoint);
inline constexpr ShaderBufferSource SBS_MaterialInstanceTextureID = MakeShaderBufferSourceBySemantic(SSBODescriptorSemantic::MaterialInstanceTextureID);

/**
* SkyInfo（全局环境/天空信息）
*/
inline constexpr ShaderBufferSource SBS_SkyInfo = MakeShaderBufferSourceBySemantic(UBODescriptorSemantic::SkyInfo);

}//namespace hgl::graph::mtl
