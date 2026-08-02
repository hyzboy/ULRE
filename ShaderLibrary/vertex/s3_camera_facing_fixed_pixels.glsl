// Stage 3: Camera-Facing Billboard (Fixed Pixel Size) — billboard at fixed screen size.
// Object center transforms to clip space; then local offset is added in NDC/pixel space.
// Requires: camera UBO, viewport UBO, l2w SSBO,
//           helpers/orient_world.glsl, helpers/scale_fixed_pixels.glsl

#include "helpers/orient_world.glsl"
#include "helpers/scale_fixed_pixels.glsl"

vec4 GetClipPos(vec4 local_pos)
{
    // Compute clip-space object center.
    vec3 world_center = (GetL2W() * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec4 clip_center = camera.vp * vec4(world_center, 1.0);

    // local_pos.xy = local quad offset in pixels.
    return ApplyFixedPixelScale(clip_center, local_pos.xy);
}
