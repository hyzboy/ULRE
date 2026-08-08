// @ulre begin
// @ulre name lit_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic WorldPosition
// @ulre require ProducedSemantic WorldNormal
// @ulre require ProducedSemantic UV0
// @ulre require Resource MaterialData
// @ulre uses material_source_interface
// @ulre uses surface_interface
// @ulre uses ntb_interface
// @ulre end
// lit_surface.glsl — Lit surface assembly.
// Material data/texture extraction and NTB resolution are replaceable providers.

#include "common/surface_interface.glsl"
#include "common/material_source_interface.glsl"
#include "common/ntb_interface.glsl"


SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    MaterialSourceInput source_input;
    source_input.surface = si;
    source_input.dataIndex = dataIndex;
    const MaterialSourceOutput material = EvalMaterialSource(source_input);

    NTBInput ntb_input;
    ntb_input.surface = si;
    ntb_input.dataIndex = dataIndex;
    ntb_input.normalScale = material.normalScale;
    const NTBSpace ntb = GetNTB(ntb_input);

    SurfaceOutput surf;
    surf.baseColor = material.baseColor;
    surf.normal    = ntb.N;
    surf.metallic  = material.metallic;
    surf.roughness = material.roughness;
    surf.fresnel   = material.fresnel;
    surf.ao        = material.ao;
    surf.emissive  = material.emissive;
    surf.alpha     = material.alpha;

    return surf;
}

float EvalAlpha(SurfaceInput si, uint dataIndex)
{
    MaterialSourceInput source_input;
    source_input.surface = si;
    source_input.dataIndex = dataIndex;
    return EvalMaterialSource(source_input).alpha;
}
