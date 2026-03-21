#version 450


#include "common/ubo_camera.glsl"
#include "common/ssbo_transform.glsl"
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
layout(location=POSITION_LOCATION) in vec3 Position;
layout(location=NORMAL_LOCATION) in vec3 Normal;

layout(location=0) flat out uint fragMaterialInstanceID;
layout(location=1) out vec3 fragWorldPos;
layout(location=2) out vec3 fragWorldNormal;

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    mat4 l2w_mat = GetTransform();
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);
    vec3 worldNormal = normalize(mat3(l2w_mat) * Normal);
    fragWorldPos   = worldPos.xyz;
    fragWorldNormal = worldNormal;

    gl_Position = camera.vp * worldPos;
}
