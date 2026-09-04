// @ulre begin
// @ulre name ntb_orthonormal
// @ulre kind Utility
// @ulre priority 0
// @ulre uses ntb_interface
// @ulre end
// Concrete orthonormal tangent-frame construction.

#ifndef NTB_ORTHONORMAL_GLSL
#define NTB_ORTHONORMAL_GLSL

#include "common/ntb_interface.glsl"

NTBSpace BuildOrthoNTB(vec3 normal)
{
    NTBSpace ntb;
    ntb.N = normalize(normal);
    const vec3 up =
        abs(ntb.N.z) < 0.999
            ? vec3(0.0, 0.0, 1.0)
            : vec3(1.0, 0.0, 0.0);
    ntb.T = normalize(cross(up, ntb.N));
    ntb.B = cross(ntb.N, ntb.T);
    return ntb;
}

#endif // NTB_ORTHONORMAL_GLSL
