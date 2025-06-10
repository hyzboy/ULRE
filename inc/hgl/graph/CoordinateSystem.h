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

    constexpr const char *CoordinateSystem2DName[]=
    {
        "NDC",
        "0to1",
        "Ortho"
    };

    inline const char *GetCoordinateSystem2DName(const enum class CoordinateSystem2D &cs)
    {
        RANGE_CHECK_RETURN_NULLPTR(cs)

        return CoordinateSystem2DName[size_t(cs)];
    }
}//namespace hgl::graph
