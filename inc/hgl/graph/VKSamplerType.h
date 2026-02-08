#pragma once
#include<hgl/graph/VKNamespace.h>
#include<hgl/shader_schema/SamplerType.h>

VK_NAMESPACE_BEGIN
using hgl::shader_schema::SamplerType;
using hgl::shader_schema::GetSamplerTypeName;
using hgl::shader_schema::ParseSamplerType;

constexpr const VkImageViewType SamplerImageViewType[]=
{
    VK_IMAGE_VIEW_TYPE_1D,
    VK_IMAGE_VIEW_TYPE_2D,
    VK_IMAGE_VIEW_TYPE_3D,
    VK_IMAGE_VIEW_TYPE_CUBE,

    VK_IMAGE_VIEW_TYPE_1D_ARRAY,
    VK_IMAGE_VIEW_TYPE_2D_ARRAY,
    VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,

    VK_IMAGE_VIEW_TYPE_2D,
    VK_IMAGE_VIEW_TYPE_2D_ARRAY,

    VK_IMAGE_VIEW_TYPE_1D,
    VK_IMAGE_VIEW_TYPE_2D,
    VK_IMAGE_VIEW_TYPE_CUBE,

    VK_IMAGE_VIEW_TYPE_1D_ARRAY,
    VK_IMAGE_VIEW_TYPE_2D_ARRAY,
    VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,
};

inline const VkImageViewType GetSamplerImageViewType(const SamplerType st)
{
    if(!RangeCheck(st))
        return(VK_IMAGE_VIEW_TYPE_MAX_ENUM);

    return SamplerImageViewType[static_cast<int>(st)];
}

VK_NAMESPACE_END
