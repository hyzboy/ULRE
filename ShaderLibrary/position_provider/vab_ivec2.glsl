#ifndef ULRE_POS_VAB_IVEC2_GLSL
#define ULRE_POS_VAB_IVEC2_GLSL

// position_provider/vab_ivec2.glsl
//
// Position source: vertex attribute buffer, 2-component signed integer (x, y).
// GetPositionLocal() converts to vec3 with z = 0 in local space.

#ifndef POSITION_LOCATION
    #define POSITION_LOCATION 0
#endif

layout(location=POSITION_LOCATION) in ivec2 inPosition;

vec3 GetPositionLocal()
{
    return vec3(float(inPosition.x), float(inPosition.y), 0.0);
}

#endif // ULRE_POS_VAB_IVEC2_GLSL
