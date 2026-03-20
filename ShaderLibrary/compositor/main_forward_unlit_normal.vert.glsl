#version 450


#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

#include "common/l2w_ssbo.glsl"
layout(location=POSITION_LOCATION) in vec3 Position;
layout(location=NORMAL_LOCATION) in vec3 Normal;

layout(location=1) out vec3 fragWorldPos;
layout(location=2) out vec3 fragWorldNormal;

void main()
{
    mat4 l2w_mat = GetTransform();
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);
    vec3 worldNormal = normalize(mat3(l2w_mat) * Normal);
    fragWorldPos   = worldPos.xyz;
    fragWorldNormal = worldNormal;

    gl_Position = camera.vp * worldPos;
}
