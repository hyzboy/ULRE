// @ulre begin
// @ulre name material_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre slot surface_provider
// @ulre provides_capability surface_base_color|surface_normal|surface_metallic|surface_roughness|surface_opacity
// @ulre uses surface_interface
// @ulre uses material_source_interface
// @ulre uses ntb_interface
// @ulre end

#ifndef MATERIAL_SURFACE_GLSL
#define MATERIAL_SURFACE_GLSL

// NTB 相关符号无条件存在：所有使用本模块的模板都把 ntb_provider 声明为必需槽，
// 不需要法线贴图的模板用空对象 ntb/ntb_none.glsl 补位（见该文件的说明）。
// 这里不允许再出现 HGL_USE_NTB_PROVIDER 之类的条件编译 —— 换实现 = 换槽模块。
#include "common/surface_interface.glsl"
#include "common/material_source_interface.glsl"
#include "common/ntb_interface.glsl"

#ifndef HGL_COVERAGE_ONLY
SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    MaterialSourceInput sourceInput;
    sourceInput.surface = si;
    sourceInput.dataIndex = dataIndex;
    const MaterialSourceOutput material =
        EvalMaterialSource(sourceInput);

    NTBInput ntbInput;
    ntbInput.surface = si;
    ntbInput.dataIndex = dataIndex;
    ntbInput.normalScale = material.normalScale;
    const vec3 resolvedNormal = GetNTB(ntbInput).N;

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
