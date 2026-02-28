#pragma once

#include <hgl/graph/mtl/FixedMaterialDef.h>
#include <hgl/graph/mtl/ShaderComposition.h>
#include <hgl/vk/VKRenderAssign.h>

namespace hgl::graph::mtl {
namespace {

constexpr FixedVertexEntry SKY_MINIMAL_VERTEX[] = {
    { VAT_VEC3, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Position },
    { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VK_VERTEX_INPUT_RATE_INSTANCE, Assign::TransformID::VIS_NAME },
};

#if defined(HGL_L2W_USE_SSBO) && HGL_L2W_USE_SSBO
constexpr DescriptorKind SKY_MINIMAL_L2W_KIND = DescriptorKind::SSBO;
#else
constexpr DescriptorKind SKY_MINIMAL_L2W_KIND = DescriptorKind::UBO;
#endif

constexpr FixedDescriptorEntry SKY_MINIMAL_DESCRIPTORS[] = {
    { DescriptorSetType::RenderTarget, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
    { DescriptorSetType::Camera, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
    { DescriptorSetType::Camera, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "sky", "SkyInfo", nullptr },
    { DescriptorSetType::PerFrame, SKY_MINIMAL_L2W_KIND, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
};

constexpr const char SKY_MINIMAL_VERT_GLSL[] = R"(
void main()
{
    Output.Direction = normalize(Position);

    gl_Position = GetPosition3D();
})";

constexpr const char SKY_MINIMAL_FRAG_GLSL[] = R"(
vec3 getSky(vec3 dir, vec3 to_light)
{
    float h = clamp(dir.z, 0.0, 1.0);

    vec3 base = sky.base_sky_color.rgb;
    vec3 grad = base * exp2(-(1.0 - h) * 0.8);

    float cos_t = clamp(dot(dir, to_light), -1.0, 1.0);
    float sun_rad = radians(sky.sun_ang_deg);
    float region = smoothstep(cos(sun_rad * 6.0), 1.0, cos_t);

    float horizon = 1.0 - h;
    vec3 warmTint = mix(vec3(1.0), vec3(1.0, 0.4, 0.05) * 1.2, clamp(horizon, 0.0, 1.0));
    vec3 scatterMix = mix(grad, warmTint, region * 0.5 * sky.sun_intensity);

    float atmosphere = sqrt(max(0.0, 1.0 - h));
    return mix(grad, scatterMix, atmosphere * 0.7);
}

vec3 getSun(vec3 dir, vec3 to_light)
{
    float cos_t = clamp(dot(dir, to_light), -1.0, 1.0);
    float sun_rad = radians(sky.sun_ang_deg);
    float sun_cos = cos(sun_rad);

    float coreN = clamp((cos_t - sun_cos) / max(1e-5, 1.0 - sun_cos), 0.0, 1.0);
    float h = clamp(dir.z, 0.0, 1.0);

    float hard = pow(coreN, 100.0);
    hard *= pow(h, 1.0 / 1.65);

    float glow = pow(coreN, 6.0);
    glow *= pow(h, 0.5);

    float sunMask = clamp(hard + glow, 0.0, 1.0);
    return sky.sun_color.rgb * sunMask * sky.sun_intensity;
}

void main()
{
    vec3 dir = normalize(Input.Direction);

    vec3 to_light = normalize(sky.sun_direction.xyz);

    vec3 sky_color = getSky(dir, to_light);

    vec3 sun_color = getSun(dir, to_light);

    FragColor = vec4(sky_color + sun_color, 1.0);
})";

constexpr const char SKY_MINIMAL_VS_BUSINESS[] = R"(
vec4 VertexShaderBusiness(const VertexInput vi)
{
    Output.Direction = normalize(vi.Position);
    return vec4(vi.Position, 1.0);
}
)";

constexpr const char SKY_MINIMAL_FS_BUSINESS[] = R"(
vec3 getSky(vec3 dir, vec3 to_light)
{
    float h = clamp(dir.z, 0.0, 1.0);

    vec3 base = sky.base_sky_color.rgb;
    vec3 grad = base * exp2(-(1.0 - h) * 0.8);

    float cos_t = clamp(dot(dir, to_light), -1.0, 1.0);
    float sun_rad = radians(sky.sun_ang_deg);
    float region = smoothstep(cos(sun_rad * 6.0), 1.0, cos_t);

    float horizon = 1.0 - h;
    vec3 warmTint = mix(vec3(1.0), vec3(1.0, 0.4, 0.05) * 1.2, clamp(horizon, 0.0, 1.0));
    vec3 scatterMix = mix(grad, warmTint, region * 0.5 * sky.sun_intensity);

    float atmosphere = sqrt(max(0.0, 1.0 - h));
    return mix(grad, scatterMix, atmosphere * 0.7);
}

vec3 getSun(vec3 dir, vec3 to_light)
{
    float cos_t = clamp(dot(dir, to_light), -1.0, 1.0);
    float sun_rad = radians(sky.sun_ang_deg);
    float sun_cos = cos(sun_rad);

    float coreN = clamp((cos_t - sun_cos) / max(1e-5, 1.0 - sun_cos), 0.0, 1.0);
    float h = clamp(dir.z, 0.0, 1.0);

    float hard = pow(coreN, 100.0);
    hard *= pow(h, 1.0 / 1.65);

    float glow = pow(coreN, 6.0);
    glow *= pow(h, 0.5);

    float sunMask = clamp(hard + glow, 0.0, 1.0);
    return sky.sun_color.rgb * sunMask * sky.sun_intensity;
}

vec4 FragmentShaderBusiness()
{
    vec3 dir = normalize(Input.Direction);
    vec3 to_light = normalize(sky.sun_direction.xyz);
    vec3 sky_color = getSky(dir, to_light);
    vec3 sun_color = getSun(dir, to_light);
    return vec4(sky_color + sun_color, 1.0);
}
)";

constexpr VertexShaderBusiness SKY_MINIMAL_VERTEX_BUSINESS { SKY_MINIMAL_VS_BUSINESS };
constexpr FragmentShaderBusiness SKY_MINIMAL_FRAGMENT_BUSINESS { SKY_MINIMAL_FS_BUSINESS };

constexpr FixedMaterialDef SKY_MINIMAL_DEF {
    "SkyMinimal",
    PrimitiveType::Triangles,
    SKY_MINIMAL_VERTEX,
    uint32_t(sizeof(SKY_MINIMAL_VERTEX) / sizeof(SKY_MINIMAL_VERTEX[0])),
    SKY_MINIMAL_DESCRIPTORS,
    uint32_t(sizeof(SKY_MINIMAL_DESCRIPTORS) / sizeof(SKY_MINIMAL_DESCRIPTORS[0])),
    nullptr,
    0,
    SKY_MINIMAL_VERT_GLSL,
    nullptr,
    SKY_MINIMAL_FRAG_GLSL,
};

const ComposedMaterialDef SKY_MINIMAL_COMPOSED_DEF {
    "SkyMinimal",
    PrimitiveType::Triangles,
    SKY_MINIMAL_VERTEX,
    uint32_t(sizeof(SKY_MINIMAL_VERTEX) / sizeof(SKY_MINIMAL_VERTEX[0])),
    SKY_MINIMAL_DESCRIPTORS,
    uint32_t(sizeof(SKY_MINIMAL_DESCRIPTORS) / sizeof(SKY_MINIMAL_DESCRIPTORS[0])),
    &SKY_MINIMAL_VERTEX_BUSINESS,
    &SKY_MINIMAL_FRAGMENT_BUSINESS,
    ShaderOutputMode::SingleRTAlphaBlend,
    false,
    nullptr,
    0,
};

}
}
