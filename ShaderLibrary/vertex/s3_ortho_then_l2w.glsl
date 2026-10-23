// @ulre begin
// @ulre name s3_ortho_then_l2w
// @ulre kind Transform
// @ulre priority 0
// @ulre require Resource Viewport
// @ulre uses orient_world
// @ulre uses world_space
// @ulre end
// Stage 3: Ortho then LocalToWorld — 2D Ortho + HAS_L2W path.
// Applies l2w * (ortho * local_pos).
// Requires: viewport UBO, l2w SSBO, helpers/orient_world.glsl,
//           helpers/world_space.glsl
//
// GetWorldPos 返回 l2w 之后的位置（ortho 是像素→NDC 的投影，
// 不产生世界位置），世界法线只受 l2w 影响。

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
    return GetL2W() * (viewport.ortho_matrix * local_pos);
}
