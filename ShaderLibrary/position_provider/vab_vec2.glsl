// position_provider/vab_vec2.glsl
//
// Position source: vertex attribute buffer, 2-component (x, y).
// GetPosition() pads z = 0 and w = 1 to produce a vec4 in local space.
//
// Prerequisites injected by emitter:
//   POSITION_LOCATION  – vertex input location for the position attribute
//
// @sfm version: 1
// @sfm kind: vab
// @sfm output_space: local
// @sfm consumes_vab: true
// @sfm allow_dim_override: true
// @sfm input: vab inPosition vec2

#ifndef ULRE_POS_VAB_VEC2_GLSL
#define ULRE_POS_VAB_VEC2_GLSL

#ifndef POSITION_LOCATION
    #define POSITION_LOCATION 0
#endif

layout(location=POSITION_LOCATION) in vec2 inPosition;

vec4 GetPosition()
{
    return vec4(inPosition, 0.0, 1.0);
}

#endif // ULRE_POS_VAB_VEC2_GLSL
