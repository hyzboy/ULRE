#pragma once

#include<hgl/vk/VKNamespace.h>
#include<vulkan/vulkan.h>

namespace hgl::graph{

// the following enum are to support

// VkPolygonMode

// VkFrontFace
// VkStencilOp
// VkCompareOp
// VkLogicOp
// VkBlendFactor
// VkBlendOp
// VkDynamicState

template<typename E> const char *VkEnum2String(const E &value);
template<typename E> const E String2VkEnum(const char *);

const char *VkCullMode2String(const VkCullModeFlags &);
const VkCullModeFlags String2VkCullMode(const char *);

}//namespace hgl::graph
