#pragma once

#include<hgl/shader_schema/VkTypes.h>

VK_NAMESPACE_BEGIN
enum class Interpolation:uint8
{
    Smooth,
    NoPerspective,
    Flat,

    ENUM_CLASS_RANGE(Smooth,Flat)
};//

constexpr const char *InterpolationName[]=
{
    "smooth",
    "noperspective",
    "flat",
};//

inline const char *GetInterpolationName(const Interpolation &i)
{
    RANGE_CHECK_RETURN_NULLPTR(i);

    return InterpolationName[(size_t)i];
}
VK_NAMESPACE_END

// Backward compatibility aliases for hgl::graph
namespace hgl::graph
{
    using hgl::shader_schema::Interpolation;
    using hgl::shader_schema::InterpolationName;
    using hgl::shader_schema::GetInterpolationName;
}//namespace hgl::graph
