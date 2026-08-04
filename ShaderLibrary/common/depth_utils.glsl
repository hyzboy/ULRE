// @ulre begin
// @ulre name depth_utils
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// Reversed-Z depth utilities
// Usage: #include "depth_utils.glsl"

/**
 * Linearize a Reversed-Z depth value.
 * In Reversed-Z: near maps to 1.0, far (infinite) maps to 0.0
 */
float LinearizeDepth(float d, float near_z)
{
    return near_z / d;  // Reversed-Z: d=1 at near, d→0 at far
}

/**
 * Reconstruct camera-relative world position from NDC + depth.
 * Returns position relative to camera. For absolute world position,
 * add cameraPosWorld (from CameraInfo UBO).
 */
vec3 ReconstructWorldPos(vec2 ndc, float depth, mat4 inv_view_proj)
{
    vec4 clip = vec4(ndc * 2.0 - 1.0, depth, 1.0);
    vec4 world = inv_view_proj * clip;
    return world.xyz / world.w;
}
