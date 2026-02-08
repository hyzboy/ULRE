#pragma once

namespace hgl::shader_schema::mtl::SamplerName
{
	constexpr const char BaseColor[] = "TextureBaseColor";
	constexpr const char Text[] = "TextureText";
}//namespace hgl::shader_schema::mtl::SamplerName

// Backward compatibility aliases for hgl::graph
namespace hgl::graph::mtl::SamplerName
{
	using hgl::shader_schema::mtl::SamplerName::BaseColor;
	using hgl::shader_schema::mtl::SamplerName::Text;
}//namespace hgl::graph::mtl::SamplerName
