#pragma once

#include<hgl/TypeFunc.h>
namespace hgl::graph
{
    enum class CoordinateSystem2D
    {
        NDC,
        ZeroToOne,          //左上角为0,0；右下角为1,1
        Ortho,              //左上角为0,0；右下角为(width-1),(height-1)

        ENUM_CLASS_RANGE(NDC,Ortho)
    };
}//namespace hgl::graph
