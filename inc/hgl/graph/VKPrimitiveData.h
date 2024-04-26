#pragma once

#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/AABB.h>

VK_NAMESPACE_BEGIN

/*
    1.截止2024.4.27，根据vulkan.gpuinfo.org统计，只有9%的设备maxVertexInputAttributes为16，不存在低于16的设备。
         9.0%的设备为28 - 31
        70.7%的设备为32
         9.6%的设备为64

        由于我们暂时没有发现需要使用16个以上属性的情况，所以这里暂定使用16。
        (如果时间过去久远，可再次查询此值是否可改成更高的值，以及是否需要)

    2.为何va_name使用char[][]而不是String以及动态分配内存？

        就是为了必避动态分配内存，以及可以直接memcpy处理，所以此处这样定义。
*/

struct VABAccessInfo
{
    char            va_name[VERTEX_ATTRIB_NAME_MAX_LENGTH+1];
    VABAccess       vab_access;
};

constexpr const uint HGL_MAX_VERTEX_ATTRIB_COUNT=16;        ///<最大顶点属性数量

struct PrimitiveData
{
    VkDeviceSize    vertex_count;

    uint32_t        va_count;

    VABAccessInfo   vab_list[HGL_MAX_VERTEX_ATTRIB_COUNT];

    IBAccess        ib_access;

    AABB            BoundingBox;
};//struct PrimitiveData

constexpr const uint PRIMITIVE_DATA_SIZE=sizeof(PrimitiveData);

VK_NAMESPACE_END
