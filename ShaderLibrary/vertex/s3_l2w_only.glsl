// @ulre begin
// @ulre name s3_l2w_only
// @ulre kind Transform
// @ulre priority 0
// @ulre uses orient_world
// @ulre uses world_space
// @ulre end
// Stage 3: LocalToWorld only — 2D NDC/ZeroToOne + HAS_L2W path.
// Applies only the LocalToWorld matrix, no camera VP.
// Requires: l2w SSBO, helpers/orient_world.glsl, helpers/world_space.glsl
//
// 这里的 l2w 本身就是最终投影（2D 路径），所以 GetWorldPos 与 GetClipPos
// 是同一个结果 —— 但语义上它是「世界空间」，世界法线也必须走逆转置，
// 否则 2D 材质一旦带非等比 l2w 缩放，法线光照就会歪。

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
    return GetL2W() * local_pos;
}
