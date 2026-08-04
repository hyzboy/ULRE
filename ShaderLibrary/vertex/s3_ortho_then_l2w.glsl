// @ulre begin
// @ulre name s3_ortho_then_l2w
// @ulre kind Transform
// @ulre priority 0
// @ulre require Resource Viewport
// @ulre uses orient_world
// @ulre end
// Stage 3: Ortho then LocalToWorld — legacy 2D Ortho + HAS_L2W path.
// Matches old TransformPolicyApplyOrthoThenL2W semantics: l2w * (ortho * local_pos).
// Requires: viewport UBO, l2w SSBO, helpers/orient_world.glsl

#include "helpers/orient_world.glsl"

vec4 GetClipPos(vec4 local_pos)
{
    return GetL2W() * (viewport.ortho_matrix * local_pos);
}
