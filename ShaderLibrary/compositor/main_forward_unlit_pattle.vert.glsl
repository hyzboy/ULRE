#version 450


#extension GL_EXT_scalar_block_layout : require

#include "common/ubo_camera.glsl"
#include "common/ssbo_transform.glsl"
layout(scalar, set=PERMATERIAL_SET, binding=COLOR_PATTLE_BINDING) uniform ColorPattle { vec4 color[256]; } color_pattle;

layout(location=POSITION_LOCATION) in vec3  Position;
layout(location=COLOR_LOCATION) in uint  ColorIndex;

layout(location=0) out vec4 fragVertexColor;

void main()
{
    mat4 l2w_mat = GetTransform();
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);

    fragVertexColor = color_pattle.color[ColorIndex];

    gl_Position = camera.vp * worldPos;
}
