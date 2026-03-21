#version 450


#include "common/ubo_camera.glsl"
#include "common/ssbo_transform.glsl"
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
layout(location=POSITION_LOCATION) in vec3 Position;
layout(location=COLOR_LOCATION) in vec4 Color;

#define VARYING_STAGE_VERT
#define HAS_VERTEX_COLOR
#include "common/varying_interface.glsl"

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    mat4 l2w_mat = GetTransform();
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);

    fragVertexColor = Color;

    gl_Position = camera.vp * worldPos;
}
