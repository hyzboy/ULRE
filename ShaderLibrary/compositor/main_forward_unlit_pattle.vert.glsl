#version 450

#include "compositor/vert_forward_ubo.glsl"
#include "common/ubo_color_pattle.glsl"

layout(location=POSITION_LOCATION) in vec3  Position;
layout(location=COLOR_LOCATION) in uint  ColorIndex;

#define VARYING_STAGE_VERT
#define HAS_VERTEX_COLOR
#include "common/varying_interface.glsl"

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    mat4 l2w_mat = GetTransform();
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);

    fragVertexColor = unpackUnorm4x8(color_pattle.color[ColorIndex]);

    gl_Position = camera.vp * worldPos;
}
