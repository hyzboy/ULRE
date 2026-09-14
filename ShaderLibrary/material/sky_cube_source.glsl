// @ulre begin
// @ulre name sky_cube_source
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require ProducedSemantic WorldPosition
// @ulre texture_reference sky_cube Fragment required
// @ulre uses material_source_interface
// @ulre uses bindless_textures
// @ulre end
// === Material Source: SkyCube ===
// 天空球 Cubemap 采样：以世界坐标方向采样 samplerCube
// （球体位于原点时，normalize(worldPos) 即面上方向）
#include "common/material_source_interface.glsl"
#include "common/bindless_textures.glsl"

vec4 SampleSkyCube(MaterialSourceInput sourceInput)
{
    const uvec2 tex_ref      = MTL_TEX(sourceInput.dataIndex).sky_cube;
    const vec3  view_dir     = normalize(sourceInput.surface.worldPos);

    return SampleCube(tex_ref.x, TrilinearSampler, view_dir);
}

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput sourceInput)
{
    const vec4 color = SampleSkyCube(sourceInput);

    MaterialSourceOutput materialResult;
    materialResult.baseColor = color.rgb;
    materialResult.metallic  = 0.0;
    materialResult.roughness = 1.0;
    materialResult.fresnel   = 0.0;
    materialResult.normalScale = 1.0;
    materialResult.ao = 1.0;
    materialResult.emissive = vec3(0.0);
    materialResult.alpha = color.a;
    return materialResult;
}
