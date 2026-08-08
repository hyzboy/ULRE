// @ulre begin
// @ulre name ntb_interface
// @ulre kind Shared
// @ulre priority 0
// @ulre end
// NTB Space Interface — defines the NTB provider input/output contract.
#ifndef NTB_INTERFACE_GLSL
#define NTB_INTERFACE_GLSL

#include "common/surface_interface.glsl"

struct NTBSpace
{
    vec3 N; // World Normal
    vec3 T; // World Tangent
    vec3 B; // World Bitangent
};

struct NTBInput
{
    SurfaceInput surface;
    uint dataIndex;
    float normalScale;
};

// A selected provider implements:
//   NTBSpace GetNTB(NTBInput input)

#endif // NTB_INTERFACE_GLSL
