// @ulre begin
// @ulre name scale_fixed_pixels
// @ulre kind Utility
// @ulre priority 0
// @ulre require Resource Viewport
// @ulre end
// Stage 3 helper: scale_fixed_pixels — fixed pixel-size billboard scale.
// Requires: camera UBO, viewport UBO (SCENE_VIEWPORT_UBO declared before this include)

#ifndef HELPER_SCALE_FIXED_PIXELS_GLSL
#define HELPER_SCALE_FIXED_PIXELS_GLSL

// Given a clip-space center and a local_xy offset (in pixels), compute the
// final clip position of the billboard vertex at a fixed pixel size.
//
// pixel_scale: world-space billboard size in pixels (from MI or push constant).
vec4 ApplyFixedPixelScale(vec4 clip_center, vec2 local_xy_pixels)
{
    vec2 ndc_center = clip_center.xy / clip_center.w;
    vec2 ndc_per_pixel = 2.0 / vec2(viewport.viewport_resolution);
    vec2 ndc_offset = local_xy_pixels * ndc_per_pixel;
    return vec4((ndc_center + ndc_offset) * clip_center.w, clip_center.z, clip_center.w);
}

#endif // HELPER_SCALE_FIXED_PIXELS_GLSL
