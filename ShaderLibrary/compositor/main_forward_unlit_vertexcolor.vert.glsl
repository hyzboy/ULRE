#version 450


#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

#include "common/l2w_ssbo.glsl"
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
