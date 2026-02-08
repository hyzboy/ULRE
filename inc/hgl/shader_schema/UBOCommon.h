#pragma once

#include<hgl/shader_schema/VkTypes.h>
#include<hgl/shader_schema/StdMaterial.h>
#include<hgl/shader_schema/ShaderBufferSource.h>

VK_NAMESPACE_BEGIN
struct UBODescriptor;
VK_NAMESPACE_END

STD_MTL_NAMESPACE_BEGIN

UBODescriptor *CreateUBODescriptor(const ShaderBufferSource &sbs,const uint32_t flag_bits);

constexpr const ShaderBufferSource SBS_ViewportInfo=
{
	DescriptorSetType::RenderTarget,

	"viewport",
	"ViewportInfo",

	R"(
	mat4 ortho_matrix;

	uvec2 canvas_resolution;
	uvec2 viewport_resolution;
	vec2 inv_viewport_resolution;
)"
};

constexpr const ShaderBufferSource SBS_CameraInfo=
{
	DescriptorSetType::Camera,

	"camera",
	"CameraInfo",

	R"(
	mat4 projection;
	mat4 inverse_projection;

	mat4 view;
	mat4 inverse_view;

	mat4 vp;
	mat4 inverse_vp;

	vec4 frustum_planes[6];

	mat4 sky;

	vec3 pos;                   //eye
	vec3 view_line;             //pos-target
	vec3 world_up;

	vec3 billboard_up;
	vec3 billboard_right;

	float znear,zfar;)"
};

constexpr const char LocalToWorldStruct[]="LocalToWorld";

constexpr const ShaderBufferSource SBS_LocalToWorld=
{
	DescriptorSetType::PerFrame,

	"l2w",
	"LocalToWorldData",

	R"(
	mat4 mats[L2W_MAX_COUNT];
)"
};

constexpr const ShaderBufferSource SBS_ColorPattle =
{
	DescriptorSetType::PerMaterial,

	"color_pattle",
	"ColorPattle",

	"vec4 color[256];"
};

// UBO must use a fixed-size array; SSBO can use a dynamic array.

constexpr const char MaterialInstanceStruct[]="MaterialInstance";

constexpr const ShaderBufferSource SBS_MaterialInstance=
{
	DescriptorSetType::PerMaterial,

	"mtl",
	"MaterialInstanceData",

	R"(
	MaterialInstance mi[MI_MAX_COUNT];)"
};

constexpr const ShaderBufferSource SBS_JointInfo=
{
	DescriptorSetType::PerFrame,

	"joint",
	"JointInfo",

	R"(
		mat4 mats[];
	)"
};

/**
* SkyInfo (global environment / sky info)
*/
constexpr const ShaderBufferSource SBS_SkyInfo=
{
	DescriptorSetType::World,

	"sky",
	"SkyInfo",

	R"(
	vec4 base_sky_color;       // Sky base color
	vec4 sun_direction;        // w=0
	vec4 sun_color;            // linear-space RGBA
	vec4 halo_color;
	vec4 moon_color;

	float sun_ang_deg;         // Sun angular diameter (deg)
	float sun_intensity;       // 0 at night
	float moon_intensity;      // Moon intensity
	float halo_intensity;      // Halo intensity
)"
};

STD_MTL_NAMESPACE_END

// Backward compatibility aliases for hgl::graph::mtl
namespace hgl::graph::mtl
{
    using hgl::shader_schema::mtl::CreateUBODescriptor;
    // All SBS_* constants are also available via the namespace alias
}//namespace hgl::graph::mtl
