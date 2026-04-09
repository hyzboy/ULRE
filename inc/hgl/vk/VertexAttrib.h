#pragma once
#include <hgl/common/VertexAttribDef.h>
#include <hgl/vk/VKFormat.h>

namespace hgl::graph
{
	struct VertexInputAttribute;

	const VkFormat GetVulkanFormat(const VABaseType &base_type,const uint vec_size);
	const VkFormat GetVulkanFormat(const VAType *type);
	const VkFormat GetVulkanFormat(const VertexInputAttribute *sa);
}
