// @ulre begin
// @ulre name s3_camera_facing_world
// @ulre kind Transform
// @ulre priority 0
// @ulre require Resource Camera
// @ulre uses orient_camera_facing
// @ulre end
// Stage 3: Camera-Facing Billboard (World Scale) — billboard oriented toward camera.
// The quad extends in camera-right/up directions, scaled in world space.
// Requires: camera UBO, l2w SSBO, helpers/orient_camera_facing.glsl

#include "helpers/orient_camera_facing.glsl"

vec4 GetClipPos(vec4 local_pos)
{
    // local_pos.xy = local quad offset (world units); z/w unused from Stage 2.
    vec3 world_pos = GetCameraFacingWorldPos(local_pos.xy);
    return camera.vp * vec4(world_pos, 1.0);
}
