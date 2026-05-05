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
        ColorPalette,

        ENUM_CLASS_RANGE(Unknown, ColorPalette)
    };

    enum class SSBODescriptorSemantic : uint8
    {
        Unknown = 0,
        TransformID,
        TransformData,
        MaterialBindingInstanceID,
        MaterialBindingInstanceData,
        MaterialBindingInstanceTexture,
        BoneJoint,
        BoneJointWeight,
        VertexStreams,
        IndexStreams,

        ENUM_CLASS_RANGE(Unknown, IndexStreams)
    };
}//namespace hgl::graph::mtl
