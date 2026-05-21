// position_provider/vab_uvec2.glsl
//
// Position source: vertex attribute buffer, 2-component unsigned integer (x, y).
// Typical use: UI / 2-D game streams where positions are packed as uint16 or
// uint32 values (e.g. tile/sprite pixel coords, atlas offsets).
//
// Integer arithmetic interface:
//   uvec2 GetPositionCoord()   – raw unsigned integer pixel coord; use this for
//                                any offset / atlas-packing / clipping arithmetic
//                                that must stay in integer domain before the transform.
//
// Float output contract (called by compositor after all integer work is done):
//   vec4 GetPosition()         – converts uvec2 → vec2, pads z=0 w=1 (local space).
//                                The compositor then applies a 2-D orthographic MVP.
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

/// Returns the raw unsigned integer pixel coordinate.
/// Use for integer-domain arithmetic (atlas offset, tile packing, clipping, etc.)
/// before the final MVP transform.
uvec2 GetPositionCoord()
{
    return inPosition;
}

/// Final output: converts the integer coord to float local space.
/// Called by the compositor; do NOT apply MVP again after this.
vec4 GetPosition()
{
    return vec4(vec2(inPosition), 0.0, 1.0);
}

#endif // ULRE_POS_VAB_UVEC2_GLSL
