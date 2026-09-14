// @ulre begin
// @ulre name sky_cube_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre slot surface_provider
// @ulre require ProducedSemantic WorldPosition
// @ulre texture_reference sky_cube Fragment required
// @ulre uses surface_interface
// @ulre uses bindless_textures
// @ulre end
// === Surface Function: SkyCube ===
// 天空球 Cubemap 采样：Sky 模板把 fragDirection 写入 si.worldPos，
// 球体位于原点时 normalize(world_pos) 即面上方向
#include "common/bindless_textures.glsl"

SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    const uvec2 tex_ref  = MTL_TEX(dataIndex).tex_sky_cube;
    const vec3  view_dir = normalize(si.worldPos);

    const vec4 color = SampleCube(tex_ref.x, TrilinearSampler, view_dir);

    SurfaceOutput so;
    so.baseColor = color.rgb;
    so.alpha     = 1.0;
    return so;
}
