#pragma once

#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/AABB.h>

VK_NAMESPACE_BEGIN

struct PrimitiveData
{
    uint32_t vertex_count;

    uint32_t va_count;

    VABAccess *vab_list;

    IBAccess ib_access;

    AABB BoundingBox;
};//struct PrimitiveData

VK_NAMESPACE_END
