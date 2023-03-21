#pragma once

#define STD_MTL_NAMESPACE_BEGIN namespace hgl{namespace graph{namespace mtl{
#define STD_MTL_NAMESPACE_END   }}}

#define STD_MTL_NAMESPACE_USING using namespace hgl::graph::mtl

enum class CoordinateSystem2D
{
    NDC,
    ZeroToOne,          //左上角为0,0；右下角为1,1
    Ortho               //左上角为0,0；右下角为(width-1),(height-1)
};

namespace GlobalShaderUBO
{
    constexpr const char ViewportInfo[]="ViewportInfo";
    constexpr const char CameraInfo[]="CameraInfo";
}
