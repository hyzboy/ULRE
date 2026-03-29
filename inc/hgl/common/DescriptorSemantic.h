#pragma once

#include<hgl/type/EnumUtil.h>

namespace hgl::graph::mtl
{
    enum class DescriptorKind : uint8
    {
        UBO,
        SSBO,
        Texture,
        TextureSampler,
    };

    enum class UBODescriptorSemantic : uint8
    {
        Unknown = 0,
        ViewportInfo,
        CameraInfo,
        SkyInfo,
        ColorPattle,

        ENUM_CLASS_RANGE(Unknown, ColorPattle)
    };

    enum class SSBODescriptorSemantic : uint8
    {
        Unknown = 0,
        TransformID,
        TransformData,
        MaterialInstanceID,
        MaterialInstanceData,
        MaterialInstanceTextureID,
        BoneJoint,
        BoneJointWeight,

        ENUM_CLASS_RANGE(Unknown, BoneJointWeight)
    };
}//namespace hgl::graph::mtl
