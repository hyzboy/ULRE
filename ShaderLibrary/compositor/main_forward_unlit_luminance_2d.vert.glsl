#version 450


#include "common/ubo_camera.glsl"
#include "common/ssbo_transform.glsl"
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
layout(location=POSITION_LOCATION) in vec2 Position;
layout(location=LUMINANCE_LOCATION) in float Luminance;

layout(location=0) flat out uint fragMaterialInstanceID;
layout(location=1)      out float fragLuminance;

void main()
{
    mat4 l2w_mat = GetTransform();
    vec4 worldPos = l2w_mat * vec4(Position, 0.0, 1.0);
    fragMaterialInstanceID = GetMaterialInstanceID();
    fragLuminance = Luminance;

    gl_Position = camera.vp * worldPos;
}
