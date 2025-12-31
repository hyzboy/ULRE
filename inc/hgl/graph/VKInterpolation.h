#pragma once
#include<hgl/graph/VKNamespace.h>

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
