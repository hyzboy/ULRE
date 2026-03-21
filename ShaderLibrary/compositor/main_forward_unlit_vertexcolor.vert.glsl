#version 450


#include "common/ubo_camera.glsl"
#include "common/ssbo_transform.glsl"
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
layout(location=POSITION_LOCATION) in vec3 Position;
layout(location=COLOR_LOCATION) in vec4 Color;

layout(location=0) flat out uint fragMaterialInstanceID;
layout(location=1) out vec4 fragVertexColor;

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    mat4 l2w_mat = GetTransform();
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);

    fragVertexColor = Color;

    gl_Position = camera.vp * worldPos;
}
