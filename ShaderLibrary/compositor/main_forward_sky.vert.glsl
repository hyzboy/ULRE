#version 450


#include "common/ubo_camera.glsl"
#include "common/ssbo_transform.glsl"
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
layout(location=POSITION_LOCATION) in vec3 Position;

#define VARYING_STAGE_VERT
#define HAS_DIRECTION
#include "common/varying_interface.glsl"

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    fragDirection = normalize(Position);

    mat4 l2w_mat = GetTransform();
    gl_Position = camera.vp * l2w_mat * vec4(Position, 1.0);
}
