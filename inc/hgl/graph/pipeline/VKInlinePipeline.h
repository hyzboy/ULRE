#pragma once

#include<hgl/TypeFunc.h>
#include<hgl/graph/VKNamespace.h>

VK_NAMESPACE_BEGIN
enum class InlinePipeline
{
    Solid3D=0,
    Alpha3D,

    Solid2D,
    Alpha2D,

    DynamicLineWidth3D,     //动态线宽3D线条

    Sky,

    ENUM_CLASS_RANGE(Solid3D,Alpha2D)
};//enum class InlinePipeline

struct PipelineData;

/**
 * 获取内置管线数据
 */
const PipelineData *GetPipelineData(const InlinePipeline &);
VK_NAMESPACE_END
