#ifndef ULRE_POS_VAB_VEC3_GLSL
#define ULRE_POS_VAB_VEC3_GLSL

// position_provider/vab_vec3.glsl
//
// Position source: vertex attribute buffer, 3-component (x, y, z).
// This is the canonical path for standard 3-D mesh rendering.
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

layout(location=POSITION_LOCATION) in vec3 inPosition;

vec3 GetPositionLocal()
{
    return inPosition;
}

#endif // ULRE_POS_VAB_VEC3_GLSL
