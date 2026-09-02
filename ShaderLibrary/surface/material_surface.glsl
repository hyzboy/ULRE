// @ulre begin
// @ulre name material_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre slot surface_provider
// @ulre provides_capability surface_base_color|surface_normal|surface_metallic|surface_roughness|surface_opacity
// @ulre uses surface_interface
// @ulre uses material_source_interface
// @ulre end

#ifndef MATERIAL_SURFACE_GLSL
#define MATERIAL_SURFACE_GLSL

#include "common/surface_interface.glsl"
#include "common/material_source_interface.glsl"
#if HGL_USE_NTB_PROVIDER
#include "common/ntb_interface.glsl"
#endif

#ifndef HGL_COVERAGE_ONLY
SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    MaterialSourceInput sourceInput;
    sourceInput.surface = si;
    sourceInput.dataIndex = dataIndex;
    const MaterialSourceOutput material =
        EvalMaterialSource(sourceInput);

    vec3 resolvedNormal = normalize(si.worldNormal);
#if HGL_USE_NTB_PROVIDER
    NTBInput ntbInput;
    ntbInput.surface = si;
    ntbInput.dataIndex = dataIndex;
    ntbInput.normalScale = material.normalScale;
    resolvedNormal = GetNTB(ntbInput).N;
#endif

    SurfaceOutput surface;
    surface.baseColor = material.baseColor;
    surface.normal = resolvedNormal;
    surface.metallic = material.metallic;
    surface.roughness = material.roughness;
    surface.fresnel = material.fresnel;
    surface.ao = material.ao;
    surface.emissive = material.emissive;
    surface.alpha = material.alpha;
    return surface;
}
#endif

float EvalAlpha(SurfaceInput si, uint dataIndex)
{
    MaterialSourceInput sourceInput;
    sourceInput.surface = si;
    sourceInput.dataIndex = dataIndex;
    return EvalMaterialAlpha(sourceInput);
}

#endif
