// @ulre begin
// @ulre name transform_ortho_viewport
// @ulre kind Transform
// @ulre priority 0
// @ulre require Resource Viewport
// @ulre uses scene_ubo
// @ulre end
// Stage 3: Ortho Viewport — 2D orthographic projection.
// Applies the viewport ortho matrix to local_pos. Used for Ortho 2D materials.

#include "module/descriptor/ubo/scene_ubo.glsl"

vec4 GetClipPos(vec4 local_pos)
{
    return viewport.ortho_matrix * local_pos;
}
