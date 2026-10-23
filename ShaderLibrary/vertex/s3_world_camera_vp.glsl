// @ulre begin
// @ulre name s3_world_camera_vp
// @ulre kind Transform
// @ulre priority 0
// @ulre require Resource Camera
// @ulre uses orient_world
// @ulre uses world_space
// @ulre end
// Stage 3: World Camera VP — standard 3D transform: L2W → camera VP.
// Requires: l2w SSBO, camera UBO, helpers/orient_world.glsl,
//           helpers/world_space.glsl
//
// 世界空间契约（凡是有世界空间的 s3 模块都必须提供这三个接口）：
//   vec3 GetWorldPos(vec4 local_pos)    物体空间位置 → 世界空间位置
//   vec3 GetWorldNormal(vec3 local_normal) 物体空间法线 → 世界空间法线
//   vec4 GetClipPos(vec4 local_pos)     物体空间位置 → clip 空间位置
// GetClipPos 由 GetWorldPos 派生，杜绝「位置走一条路、法线走另一条」的漂移。

#include "vertex/helpers/orient_world.glsl"
#include "vertex/helpers/world_space.glsl"

vec3 GetWorldPos(vec4 local_pos)
{
    return (GetL2W() * local_pos).xyz;
}

vec3 GetWorldNormal(vec3 local_normal)
{
    return NormalToWorldSpace(GetL2W(), local_normal);
}

vec4 GetClipPos(vec4 local_pos)
{
    return camera.vp * vec4(GetWorldPos(local_pos), 1.0);
}
