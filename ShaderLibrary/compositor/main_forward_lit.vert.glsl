#version 450


#include "common/ubo_camera.glsl"
#include "common/ssbo_transform.glsl"
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
layout(location=POSITION_LOCATION) in vec3 Position;
layout(location=TEXCOORD_LOCATION) in vec2 TexCoord;
layout(location=NORMAL_LOCATION) in vec3 Normal;

#define VARYING_STAGE_VERT
#define HAS_WORLD_POS
#define HAS_WORLD_NORMAL
#define HAS_UV0
#include "common/varying_interface.glsl"

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    mat4 l2w_mat = GetTransform();
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);
    vec3 worldNormal = normalize(mat3(l2w_mat) * Normal);
    fragWorldPos   = worldPos.xyz;
    fragWorldNormal = worldNormal;
    fragUV0        = TexCoord;

    gl_Position = camera.vp * worldPos;
}
