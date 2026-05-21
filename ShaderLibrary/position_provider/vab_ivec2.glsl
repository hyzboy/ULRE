// position_provider/vab_ivec2.glsl
//
// Position source: vertex attribute buffer, 2-component signed integer (x, y).
// Typical use: UI / Text2D pixel-coordinate streams where positions are stored
// as int16 or int32 values (e.g. screen-pixel coords, glyph offsets).
//
// Integer arithmetic interface:
//   ivec2 GetPositionCoord()   – raw signed integer pixel coord; use this for
//                                any offset / clipping / scroll arithmetic that
//                                must stay in integer domain before the transform.
//
// Float output contract (called by compositor after all integer work is done):
//   vec4 GetPosition()         – converts ivec2 → vec2, pads z=0 w=1 (local space).
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
// @sfm input: vab inPosition ivec2

#ifndef ULRE_POS_VAB_IVEC2_GLSL
#define ULRE_POS_VAB_IVEC2_GLSL

#ifndef POSITION_LOCATION
    #define POSITION_LOCATION 0
#endif

layout(location=POSITION_LOCATION) in ivec2 inPosition;

/// Returns the raw signed integer pixel coordinate.
/// Use for integer-domain arithmetic (scroll, glyph offset, clipping, etc.)
/// before the final MVP transform.
ivec2 GetPositionCoord()
{
    return inPosition;
}

/// Final output: converts the integer coord to float local space.
/// Called by the compositor; do NOT apply MVP again after this.
vec4 GetPosition()
{
    return vec4(vec2(inPosition), 0.0, 1.0);
}

#endif // ULRE_POS_VAB_IVEC2_GLSL
