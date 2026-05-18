#pragma once

/// MaterialBuildFlags.h
///
/// 本文件保留 IncludeL2W 枚举供 Material3DCreateConfig 使用。
/// IncludeSky / IncludeCamera 已删除 —— 资源需求现由 vertex policy
/// @sfm:require 注解和 MaterialVariantRow 自动推导。

#include<hgl/type/String.h>

namespace hgl::graph
{
    namespace mtl
    {
        enum class IncludeL2W:uint8
        {
            Without=0,
            With
        };
    }//namespace mtl
}//namespace hgl::graph
