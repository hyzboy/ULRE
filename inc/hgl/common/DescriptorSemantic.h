#pragma once

#include<hgl/CoreType.h>

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
        Custom,
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
        Custom,
    };
}//namespace hgl::graph::mtl
