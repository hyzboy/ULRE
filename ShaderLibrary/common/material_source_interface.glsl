// @ulre begin
// @ulre name material_source_interface
// @ulre kind Shared
// @ulre priority 0
// @ulre end
// Material source provider contract.

#ifndef MATERIAL_SOURCE_INTERFACE_GLSL
#define MATERIAL_SOURCE_INTERFACE_GLSL

#include "common/surface_interface.glsl"

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
};

// A selected material-source module implements:
//   MaterialSourceOutput EvalMaterialSource(MaterialSourceInput input)

#endif // MATERIAL_SOURCE_INTERFACE_GLSL
