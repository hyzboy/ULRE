#pragma once

#include<hgl/type/DataType.h>

namespace hgl::shader_schema
{
	struct RenderTargetOutputConfig
	{
		uint color;                 ///<要输出几个颜色缓冲区
		bool depth;                 ///<是否输出到深度缓冲区
		bool stencil;               ///<是否输出到模板缓冲区
	};
}//namespace hgl::shader_schema

// Backward compatibility aliases for hgl::graph
namespace hgl::graph
{
	using hgl::shader_schema::RenderTargetOutputConfig;
}//namespace hgl::graph
