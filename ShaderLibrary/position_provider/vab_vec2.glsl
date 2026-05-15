#ifndef ULRE_POS_VAB_VEC2_GLSL
#define ULRE_POS_VAB_VEC2_GLSL

// position_provider/vab_vec2.glsl
//
// Position source: vertex attribute buffer, 2-component (x, y).
// GetPositionLocal() pads z = 0 to produce a vec3 in object (local) space.
//
// MANIFEST: {
//   "vab_count": 1,
//   "position_space": "local",
//   "ssbo": [], "ubo": [], "samplers": []
// }
//
// Prerequisites injected by emitter:
//   POSITION_LOCATION  – vertex input location for the position attribute

#ifndef POSITION_LOCATION
    #define POSITION_LOCATION 0
#endif

layout(location=POSITION_LOCATION) in vec2 inPosition;

vec3 GetPositionLocal()
{
    return vec3(inPosition, 0.0);
}

#endif // ULRE_POS_VAB_VEC2_GLSL
