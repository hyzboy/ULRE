// position_provider/vab_uvec2.glsl
//
// Position source: vertex attribute buffer, 2-component unsigned integer (x, y).
// Typical use: UI / 2-D game streams where positions are packed as uint16 or
// uint32 values (e.g. tile/sprite pixel coords, atlas offsets).
//
// GetPosition() converts the unsigned integer pair to float and pads z=0, w=1.
// The compositor applies a 2-D orthographic MVP to produce clip-space output.
//
// Prerequisites injected by emitter:
//   POSITION_LOCATION  – vertex input location for the position attribute
//
// @sfm version: 1
// @sfm kind: vab
// @sfm output_space: local
// @sfm consumes_vab: true
// @sfm allow_dim_override: true
// @sfm input: vab inPosition uvec2

#ifndef ULRE_POS_VAB_UVEC2_GLSL
#define ULRE_POS_VAB_UVEC2_GLSL

#ifndef POSITION_LOCATION
    #define POSITION_LOCATION 0
#endif

layout(location=POSITION_LOCATION) in uvec2 inPosition;

vec4 GetPosition()
{
    return vec4(vec2(inPosition), 0.0, 1.0);
}

#endif // ULRE_POS_VAB_UVEC2_GLSL
