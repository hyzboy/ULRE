#pragma once
#include <hgl/common/VertexAttribDef.h>
#include <hgl/vk/VKFormat.h>

namespace hgl::graph
{
	struct VertexInputAttribute;

	// DEPRECATED: Derives VkFormat from VABaseType + vec_size (legacy implicit mapping, bridge-layer only).
	[[deprecated("This is a legacy bridge function. Use VertexAttributeSpec with explicit storage_format instead.")]]
	const VkFormat GetVulkanFormat(const VABaseType &base_type,const uint vec_size);
	
	// DEPRECATED: Derives VkFormat from VAType pointer (legacy implicit mapping, bridge-layer only).
	// New code should use VertexAttributeSpec with explicit storage_format.
	[[deprecated("This is a legacy bridge function. Use VertexAttributeSpec with explicit storage_format instead.")]]
	const VkFormat GetVulkanFormat(const VAType *type);
	
	// DEPRECATED: Derives VkFormat from VertexInputAttribute (legacy fallback).
	// New code should pre-populate storage_format in VertexInputAttribute or use VertexAttributeSpec.
	[[deprecated("This is a legacy fallback function. Ensure VertexInputAttribute.storage_format is pre-populated.")]]
	const VkFormat GetVulkanFormat(const VertexInputAttribute *sa);
}
