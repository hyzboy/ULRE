#pragma once

#include<hgl/graph/VKNamespace.h>
#include<hgl/TypeFunc.h>

VK_NAMESPACE_BEGIN

enum class BlendMode
{
    Opaque,
    Mask,
    Transparent,        ///<普通的Alpha混合透明
    PreMulti,           ///<预乘的Alpha混合透明
    Add,                ///<加法混合
    Subtract,           ///<减法混合
    ReverseSubtract,    ///<反减混合
    Min,                ///<最小混合
    Max,                ///<最大混合

    ENUM_CLASS_RANGE(Opaque,Max)
};

VK_NAMESPACE_END
