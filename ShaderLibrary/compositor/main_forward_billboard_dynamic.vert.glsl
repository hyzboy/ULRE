#version 450

#include "compositor/vert_forward_ubo.glsl"
layout(location=POSITION_LOCATION) in vec3 Position;

#define VARYING_STAGE_VERT
#define HAS_TEXCOORD
#include "common/varying_interface.glsl"

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    mat4 l2w_mat = GetTransform();
    vec3 center = (l2w_mat * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 world_pos = center
                   + Position.x * camera.billboard_right
                   + Position.y * camera.billboard_up;

    fragTexCoord = vec2(Position.x + 0.5, Position.y * -1.0 + 0.5);

    gl_Position = camera.vp * vec4(world_pos, 1.0);
}
