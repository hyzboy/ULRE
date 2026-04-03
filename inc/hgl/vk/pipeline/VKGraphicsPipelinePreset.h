#pragma once

#include<hgl/type/EnumUtil.h>

namespace hgl::graph{
enum class GraphicsPipelinePreset
{
    Solid3D=0,
    Alpha3D,
    Masked3D,
    Dither3D,
    AlphaToCoverage3D,

    GizmoOverlay3D,

    Solid2D,
    Alpha2D,

    DynamicLineWidth3D,     //动态线宽3D线条

    Sky,

    ENUM_CLASS_RANGE(Solid3D,Alpha2D)
};//enum class GraphicsPipelinePreset

struct GraphicsPipelineData;

/**
 * 获取内置管线数据
 */
const GraphicsPipelineData *GetGraphicsPipelineData(const GraphicsPipelinePreset &);
}//namespace hgl::graph
