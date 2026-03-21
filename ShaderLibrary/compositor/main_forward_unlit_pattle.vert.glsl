#version 450


#extension GL_EXT_scalar_block_layout : require

#include "common/ubo_camera.glsl"
#include "common/ssbo_transform.glsl"
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
#include "common/ubo_color_pattle.glsl"

layout(location=POSITION_LOCATION) in vec3  Position;
layout(location=COLOR_LOCATION) in uint  ColorIndex;

layout(location=0) flat out uint fragMaterialInstanceID;
layout(location=1) out vec4 fragVertexColor;

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    mat4 l2w_mat = GetTransform();
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);

    fragVertexColor = color_pattle.color[ColorIndex];

    gl_Position = camera.vp * worldPos;
}
