#pragma once

#include<hgl/graph/BlendMode.h>
#include<hgl/graph/RenderBufferName.h>
#include<hgl/type/SortedSets.h>

VK_NAMESPACE_BEGIN

enum class RenderOrder
{
    First,          ///<最先渲染

    NearToFar,      ///<从近到远
    Irrorder,       ///<无序渲染
    FarToNear,      ///<从远到近

    Last,           ///<最后渲染

    ENUM_CLASS_RANGE(First,Last)
};//enum class RenderOrder

struct RenderWorkConfig
{
    BlendMode blend_mode;
    RenderOrder render_order;

    SortedSets<RENDER_BUFFER_NAME> output_buffer;
};//struct RenderWorkConfig;

VK_NAMESPACE_END
