#pragma once

#include<hgl/type/EnumUtil.h>

namespace hgl::graph
{
    enum class PipelinePreset
    {
        Auto = 0xFF, // 由材质定义推导默认 preset
        Solid3D = 0,
        Alpha3D,

        GizmoOverlay3D,

        Solid2D,
        Alpha2D,

        DynamicLineWidth3D,     // 动态线宽 3D 线条

        Sky,

        ENUM_CLASS_RANGE(Solid3D, Alpha2D)
    };//enum class PipelinePreset
}//namespace hgl::graph
