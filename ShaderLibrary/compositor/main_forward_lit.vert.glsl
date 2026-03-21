#version 450


#include "common/ubo_camera.glsl"
#include "common/ssbo_transform.glsl"
layout(location=POSITION_LOCATION) in vec3 Position;
layout(location=TEXCOORD_LOCATION) in vec2 TexCoord;
layout(location=NORMAL_LOCATION) in vec3 Normal;

layout(location=1) out vec3 fragWorldPos;
layout(location=2) out vec3 fragWorldNormal;
layout(location=3) out vec2 fragUV0;

void main()
{
    mat4 l2w_mat = GetTransform();
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);
    vec3 worldNormal = normalize(mat3(l2w_mat) * Normal);
    fragWorldPos   = worldPos.xyz;
    fragWorldNormal = worldNormal;
    fragUV0        = TexCoord;

    gl_Position = camera.vp * worldPos;
}
