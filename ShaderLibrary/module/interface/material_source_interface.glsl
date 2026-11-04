// @ulre begin
// @ulre name material_source_interface
// @ulre kind Shared
// @ulre priority 0
// @ulre uses surface_interface
// @ulre end
// Material source provider contract.

#ifndef MATERIAL_SOURCE_INTERFACE_GLSL
#define MATERIAL_SOURCE_INTERFACE_GLSL

#include "module/interface/surface_interface.glsl"

struct MaterialSourceInput
{
    SurfaceInput surface;
    uint dataIndex;
};

struct MaterialSourceOutput
{
    vec3  baseColor;
    float metallic;
    float roughness;
    float fresnel;
    float normalScale;
    float ao;
    vec3  emissive;
    float alpha;
    // 高级材质参数（与 SurfaceOutput / LightingInput 同名直通）。
    // 每个 provider 必须显式赋值：无高级数据的 = 0；
    // material_source_pbr_advanced 从 64B 高级行读真值。
    float clearCoat;
    float clearCoatRoughness;
    vec3  sheenColor;
    float sheenRoughness;
    float anisotropy;
    float subsurfacePower;
    float thickness;
};

// A selected material-source module implements:
//   MaterialSourceOutput EvalMaterialSource(MaterialSourceInput input)

#endif // MATERIAL_SOURCE_INTERFACE_GLSL
