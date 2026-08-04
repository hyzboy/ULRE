// @ulre begin
// @ulre name s3_world_camera_vp
// @ulre kind Transform
// @ulre priority 0
// @ulre require Resource Camera
// @ulre uses orient_world
// @ulre end
// Stage 3: World Camera VP — standard 3D transform: L2W → camera VP.
// Requires: l2w SSBO, camera UBO, helpers/orient_world.glsl

#include "helpers/orient_world.glsl"

vec4 GetClipPos(vec4 local_pos)
{
    return camera.vp * GetL2W() * local_pos;
}
