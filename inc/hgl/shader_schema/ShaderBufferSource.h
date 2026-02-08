#pragma once

#include<hgl/shader_schema/DescriptorSetType.h>

namespace hgl::shader_schema
{
	struct ShaderBufferDesc
	{
		const DescriptorSetType set_type;

		const char *name;
	};

	struct ShaderBufferSource:public ShaderBufferDesc
	{
		const char *struct_name;
		const char *codes;
	};
}//namespace hgl::shader_schema

// Backward compatibility aliases for hgl::graph
namespace hgl::graph
{
	using hgl::shader_schema::ShaderBufferDesc;
	using hgl::shader_schema::ShaderBufferSource;
}//namespace hgl::graph
