#version 450


#include "common/ubo_camera.glsl"
#include "common/ssbo_transform.glsl"
layout(location=POSITION_LOCATION) in vec3 Position;

layout(location=0) out vec3 fragDirection;

void main()
{
    fragDirection = normalize(Position);

    mat4 l2w_mat = GetTransform();
    gl_Position = camera.vp * l2w_mat * vec4(Position, 1.0);
}
