#version 450


#include "common/ubo_camera.glsl"
#include "common/ssbo_transform.glsl"
layout(location=POSITION_LOCATION) in vec3 Position;
layout(location=COLOR_LOCATION) in vec4 Color;

layout(location=0) out vec4 fragVertexColor;

void main()
{
    mat4 l2w_mat = GetTransform();
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);

    fragVertexColor = Color;

    gl_Position = camera.vp * worldPos;
}
