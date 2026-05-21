// position_provider/vab_vec3.glsl
//
// Position source: vertex attribute buffer, 3-component (x, y, z).
// This is the canonical path for standard 3-D mesh rendering.
//
// Prerequisites injected by emitter:
//   POSITION_LOCATION  – vertex input location for the position attribute
//
// @sfm version: 1
// @sfm kind: vab
// @sfm output_space: local
// @sfm consumes_vab: true
// @sfm allow_dim_override: true
// @sfm input: vab inPosition vec3

#ifndef ULRE_POS_VAB_VEC3_GLSL
#define ULRE_POS_VAB_VEC3_GLSL

#ifndef POSITION_LOCATION
    #define POSITION_LOCATION 0
#endif

layout(location=POSITION_LOCATION) in vec3 inPosition;

vec4 GetPosition()
{
    return vec4(inPosition, 1.0);
}

#endif // ULRE_POS_VAB_VEC3_GLSL
