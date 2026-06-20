#ifndef ULRE_POS_VAB_UVEC2_GLSL
#define ULRE_POS_VAB_UVEC2_GLSL

// position_provider/vab_uvec2.glsl
//
// Position source: vertex attribute buffer, 2-component unsigned integer (x, y).
// GetPositionLocal() converts to vec3 with z = 0 in local space.

#ifndef POSITION_LOCATION
    #define POSITION_LOCATION 0
#endif

layout(location=POSITION_LOCATION) in uvec2 inPosition;

vec3 GetPositionLocal()
{
    return vec3(float(inPosition.x), float(inPosition.y), 0.0);
}

#endif // ULRE_POS_VAB_UVEC2_GLSL
