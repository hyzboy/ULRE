#pragma once

#include<hgl/type/UnorderedMap.h>
#include<hgl/shader_schema/VkTypes.h>
#include<hgl/shader_schema/DescriptorSetType.h>
#include<hgl/shader_schema/ShaderDescriptor.h>

VK_NAMESPACE_BEGIN
struct ShaderDescriptorSet
{
	DescriptorSetType set_type;

	int set;
	int count;

	UnorderedMap<AnsiString,ShaderDescriptor*>  descriptor_map;

public:

	/**
	* 添加一个描述符
	* @param shader_stage_flag_bits 着色器阶段标志
	* @param new_sd 新的描述符
	* @return 新的描述符(注: 正常操作一定要使用此返回值，new_sd有可能因为重复被删掉)
	*/
	ShaderDescriptor *AddDescriptor(uint32_t shader_stage_flag_bits,ShaderDescriptor *new_sd);
};

using ShaderDescriptorSetArray=ShaderDescriptorSet[DESCRIPTOR_SET_TYPE_COUNT];
VK_NAMESPACE_END
