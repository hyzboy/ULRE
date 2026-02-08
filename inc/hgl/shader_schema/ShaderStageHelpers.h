#pragma once

#include<hgl/shader_schema/VkTypes.h>
#include<hgl/shader_schema/VertexInputAttribute.h>

namespace hgl::shader_schema
{
	const uint GetShaderCountByBits(const uint32_t bits);                   ///<根据ShaderStage位数据统计有多少个shader
	const uint GetMaxShaderStage(const uint32_t bits);                      ///<根据ShaderStage位数据获取最大的ShaderStage位
	const char *GetShaderStageName(const VkShaderStageFlagBits &);          ///<获取指定ShaderStage位的名称
	const uint GetShaderStageFlagBits(const char *,int len=0);              ///<根据名称获取ShaderStage位数据

	const VkFormat GetVulkanFormat(const VertexInputAttribute *sa);
}//namespace hgl::shader_schema

// Backward compatibility aliases for hgl::graph
namespace hgl::graph
{
	using hgl::shader_schema::GetShaderCountByBits;
	using hgl::shader_schema::GetMaxShaderStage;
	using hgl::shader_schema::GetShaderStageName;
	using hgl::shader_schema::GetShaderStageFlagBits;
	using hgl::shader_schema::GetVulkanFormat;
}//namespace hgl::graph
