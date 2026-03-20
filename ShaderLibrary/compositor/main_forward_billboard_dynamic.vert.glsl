#version 450


#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

#include "common/l2w_ssbo.glsl"
layout(location=POSITION_LOCATION) in vec3 Position;

layout(location=0) out vec2 fragTexCoord;

void main()
{
    mat4 l2w_mat = GetTransform();
    vec3 center = (l2w_mat * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 world_pos = center
                   + Position.x * camera.billboard_right
                   + Position.y * camera.billboard_up;

    fragTexCoord = vec2(Position.x + 0.5, Position.y * -1.0 + 0.5);

    gl_Position = camera.vp * vec4(world_pos, 1.0);
}
