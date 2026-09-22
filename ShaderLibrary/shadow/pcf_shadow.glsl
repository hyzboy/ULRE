// @ulre begin
// @ulre name pcf_shadow
// @ulre kind Utility
// @ulre priority 0
// @ulre slot shadow_provider
// @ulre provides_capability shadow_factor
// @ulre uses surface_interface
// @ulre uses bindless_textures
// @ulre uses scene_ubo
// @ulre end

#ifndef PCF_SHADOW_GLSL
#define PCF_SHADOW_GLSL

#include "common/surface_interface.glsl"
#include "common/bindless_textures.glsl"
#include "ubo/scene_ubo.glsl"

#ifndef ShadowMapSampler
#define ShadowMapSampler 7u
#endif

float EvalPCFShadow(vec3 worldPos)
{
    if (shadow.shadow_tex.x == 0u)
        return 1.0;

    const vec4 light_clip = shadow.shadow_vp * vec4(worldPos, 1.0);
    if (light_clip.w <= 0.0)
        return 1.0;

    const vec3 light_ndc = light_clip.xyz / light_clip.w;

    // 检查是否在 reversed-Z 深度范围内 [0, 1]
    if (light_ndc.z < 0.0 || light_ndc.z > 1.0)
        return 1.0;

    const vec2 shadow_uv = vec2(0.5) + 0.5 * light_ndc.xy;

    const float bias       = shadow.shadow_params.x;
    const float pcf_radius = shadow.shadow_params.y;
    const vec2  texel      = shadow.inv_shadow_map_size;

    const float margin = texel.x * (pcf_radius + 1.0);
    if (shadow_uv.x < margin || shadow_uv.x > (1.0 - margin) ||
        shadow_uv.y < margin || shadow_uv.y > (1.0 - margin))
    {
        return 1.0;
    }

    const float current_depth = light_ndc.z;
    const float layer         = float(shadow.shadow_tex.y);
    const uint  tex_handle    = shadow.shadow_tex.x;

    float occluded = 0.0;
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            const vec2  tap = shadow_uv + vec2(float(dx), float(dy)) * (texel * pcf_radius);
            const float d   = Sample2DArray(tex_handle, ShadowMapSampler, tap, layer).r;

            occluded += (d > current_depth + bias) ? 1.0 : 0.0;
        }
    }

    const float unshadowed = 1.0 - occluded * (1.0 / 9.0);
    return mix(shadow.shadow_params.z, 1.0, unshadowed);
}

float GetShadowFactor(SurfaceInput surface)
{
    return EvalPCFShadow(surface.worldPos);
}

#endif // PCF_SHADOW_GLSL
