#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VertexAttrib.h>

VK_NAMESPACE_BEGIN

// 旧的单一Assign VertexInput定义(已废弃)
// constexpr VkFormat ASSIGN_VAB_FMT = VK_FORMAT_R16G16_UINT;  // RG16UI格式
// constexpr const char *ASSIGN_VIS_NAME = "Assign";           // 顶点输入名称
// constexpr VAType ASSIGN_VAT_FMT = VAT_UVEC2;                // 顶点属性类型

// 新的分离的两个VertexInput定义
constexpr VkFormat TRANSFORM_INDEX_VAB_FMT = VK_FORMAT_R16_UINT;          // Transform索引格式(R16UI)
constexpr const char *TRANSFORM_INDEX_VIS_NAME = "TransformIndex";        // Transform索引顶点输入名称
constexpr VAType TRANSFORM_INDEX_VAT_FMT = VAT_UINT;                      // Transform索引顶点属性类型

constexpr VkFormat MATERIAL_INSTANCE_INDEX_VAB_FMT = VK_FORMAT_R16_UINT;  // MaterialInstance索引格式(R16UI)
constexpr const char *MATERIAL_INSTANCE_INDEX_VIS_NAME = "MaterialInstanceIndex";  // MaterialInstance索引顶点输入名称
constexpr VAType MATERIAL_INSTANCE_INDEX_VAT_FMT = VAT_UINT;              // MaterialInstance索引顶点属性类型

// VertexInputGroup 定义
namespace VertexInputGroup
{
    constexpr uint8 Basic           = 0;    // 基础顶点属性(位置、法线、纹理坐标等)
    constexpr uint8 JointID         = 1;    // 骨骼ID
    constexpr uint8 JointWeight     = 2;    // 骨骼权重
    constexpr uint8 Assign          = 3;    // 旧的单一分配数据(已废弃)
    constexpr uint8 TransformIndex  = 4;    // Transform索引
    constexpr uint8 MaterialInstanceIndex = 5;  // MaterialInstance索引
}

VK_NAMESPACE_END
