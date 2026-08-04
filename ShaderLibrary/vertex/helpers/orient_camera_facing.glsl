// @ulre begin
// @ulre name orient_camera_facing
// @ulre kind Utility
// @ulre priority 0
// @ulre require Resource Camera
// @ulre uses orient_world
// @ulre end
// Stage 3 helper: orient_camera_facing — camera-facing billboard orientation.
// Requires: camera UBO (SCENE_CAMERA_UBO declared before this include)
//           orient_world.glsl (GetL2W for object center)

#ifndef HELPER_ORIENT_CAMERA_FACING_GLSL
#define HELPER_ORIENT_CAMERA_FACING_GLSL

#include "helpers/orient_world.glsl"

// Returns the world-space center of the object (translation column of L2W).
vec3 GetWorldCenter()
{
    mat4 l2w_mat = GetL2W();
    return (l2w_mat * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
}

// Returns a world-space billboard position: center + right*x + up*y.
// local_xy: the 2D local quad offset (from vertex attribute).
vec3 GetCameraFacingWorldPos(vec2 local_xy)
{
    vec3 center = GetWorldCenter();
    return center
         + local_xy.x * camera.camera_facing_right
         + local_xy.y * camera.camera_facing_up;
}

#endif // HELPER_ORIENT_CAMERA_FACING_GLSL
