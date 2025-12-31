#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VertexAttrib.h>

VK_NAMESPACE_BEGIN

// 旧的单一Assign VertexInput定义(已废弃)
// constexpr VkFormat ASSIGN_VAB_FMT = VK_FORMAT_R16G16_UINT;  // RG16UI格式
// constexpr const char *ASSIGN_VIS_NAME = "Assign";           // 顶点输入名称
// constexpr VAType ASSIGN_VAT_FMT = VAT_UVEC2;                // 顶点属性类型

namespace Assign
{
    namespace TransformID
    {
        constexpr VkFormat          VAB_FMT         = VK_FORMAT_R16_UINT;      // Transform索引格式(R16UI)
        constexpr const char *      VIS_NAME        = "TransformID";           // Transform索引顶点输入名称
        constexpr VAType            VAT_FMT         = VAT_UINT;                // Transform索引顶点属性类型
        constexpr const uint32_t    STRIDE_BYTES    = sizeof(uint16);          // Transform索引顶点属性字节大小
    }//namespace Transform

    namespace MaterialInstanceID
    {
        constexpr VkFormat          VAB_FMT         = VK_FORMAT_R16_UINT;      // MaterialInstance索引格式(R16UI)
        constexpr const char *      VIS_NAME        = "MaterialInstanceID";    // MaterialInstance索引顶点输入名称
        constexpr VAType            VAT_FMT         = VAT_UINT;                // MaterialInstance索引顶点属性类型
        constexpr const uint32_t    STRIDE_BYTES    = sizeof(uint16);          // MaterialInstance索引顶点属性字节大小
    }//namespace MaterialInstance
}//namespace Assign

VK_NAMESPACE_END
