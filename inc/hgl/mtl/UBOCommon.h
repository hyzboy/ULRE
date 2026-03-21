#pragma once

#include<hgl/mtl/ShaderBufferSource.h>
#include<hgl/mtl/DescriptorBindingContract.h>

namespace hgl::graph{
struct UBODescriptor;
struct SSBODescriptor;
}//namespace hgl::graph

namespace hgl::graph::mtl{

UBODescriptor *CreateUBODescriptor(const ShaderBufferSource &sbs,const uint32_t flag_bits);
SSBODescriptor *CreateSSBODescriptor(const ShaderBufferSource &sbs,const uint32_t flag_bits);
const ShaderBufferSource *FindShaderBufferSourceByStructName(const char *struct_name);

constexpr ShaderBufferSource MakeShaderBufferSourceBySemantic(const DescriptorSemantic semantic)
{
    const auto &meta = GetDescriptorSemanticMeta(semantic);

    return ShaderBufferSource{
        meta.set_type,
        meta.name,
        meta.struct_name,
    };
}

inline constexpr ShaderBufferSource SBS_ViewportInfo = MakeShaderBufferSourceBySemantic(DescriptorSemantic::ViewportInfo);
inline constexpr ShaderBufferSource SBS_CameraInfo = MakeShaderBufferSourceBySemantic(DescriptorSemantic::CameraInfo);
inline constexpr ShaderBufferSource SBS_LocalToWorld = MakeShaderBufferSourceBySemantic(DescriptorSemantic::LocalToWorld);
inline constexpr ShaderBufferSource SBS_TransformID = MakeShaderBufferSourceBySemantic(DescriptorSemantic::TransformID);
inline constexpr ShaderBufferSource SBS_MaterialInstanceID = MakeShaderBufferSourceBySemantic(DescriptorSemantic::MaterialInstanceID);
inline constexpr ShaderBufferSource SBS_ColorPattle = MakeShaderBufferSourceBySemantic(DescriptorSemantic::ColorPattle);

constexpr const char MaterialInstanceStruct[]="MaterialInstance";

inline constexpr ShaderBufferSource SBS_MaterialInstance = MakeShaderBufferSourceBySemantic(DescriptorSemantic::MaterialInstance);
inline constexpr ShaderBufferSource SBS_JointInfo = MakeShaderBufferSourceBySemantic(DescriptorSemantic::BoneJoint);

/**
* SkyInfo（全局环境/天空信息）
*/
inline constexpr ShaderBufferSource SBS_SkyInfo = MakeShaderBufferSourceBySemantic(DescriptorSemantic::SkyInfo);

}//namespace hgl::graph::mtl
