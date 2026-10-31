// @ulre begin
// @ulre name transform_camera_facing_world
// @ulre kind Transform
// @ulre priority 0
// @ulre require Resource Camera
// @ulre uses orient_camera_facing
// @ulre uses world_space
// @ulre end
// Stage 3: Camera-Facing Billboard (World Scale) — billboard oriented toward camera.
// The quad extends in camera-right/up directions, scaled in world space.
// Requires: camera UBO, l2w SSBO, helpers/orient_camera_facing.glsl,
//           helpers/world_space.glsl
//
// 注意 GetLocalPos() 返回的是 vec4，但此处只有 .xy 是「世界单位偏移」，
// .zw 不带空间信息；世界位置由 GetCameraFacingWorldPos 给出，
// 法线仍然来自 l2w 的逆转置。

#include "module/vertex/helpers/orient_camera_facing.glsl"
#include "module/vertex/helpers/world_space.glsl"

vec3 GetWorldPos(vec4 local_pos)
{
    // local_pos.xy = local quad offset (world units); z/w unused from Stage 2.
    return GetCameraFacingWorldPos(local_pos.xy);
}

vec3 GetWorldNormal(vec3 local_normal)
{
    return NormalToWorldSpace(GetL2W(), local_normal);
}

vec4 GetClipPos(vec4 local_pos)
{
    return camera.vp * vec4(GetWorldPos(local_pos), 1.0);
}
