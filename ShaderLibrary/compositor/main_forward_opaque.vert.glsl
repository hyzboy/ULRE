#version 450


#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

#include "common/l2w_ssbo.glsl"
#if GEOMETRY_FETCH_SSBO
    #include "common/vertex_fetch_ssbo.glsl"
#else
    layout(location=POSITION_LOCATION) in vec3 inPosition;
    layout(location=NORMAL_LOCATION) in vec3 inNormal;
    layout(location=TEXCOORD_LOCATION) in vec2 inUV0;
#endif

layout(location=0) out vec3 fragWorldPos;
layout(location=1) out vec3 fragWorldNormal;
layout(location=2) out vec2 fragUV0;

void main()
{
    mat4 l2w_mat = GetTransform();

#if GEOMETRY_FETCH_SSBO
    vec3 pos = FetchPosition(gl_VertexIndex);
    vec3 normal = FetchNormal(gl_VertexIndex);
    vec2 uv0 = FetchUV0(gl_VertexIndex);
#else
    vec3 pos = inPosition;
    vec3 normal = inNormal;
    vec2 uv0 = inUV0;
#endif

    vec4 worldPos = l2w_mat * vec4(pos, 1.0);       fragWorldPos = worldPos.xyz;
    fragWorldNormal = mat3(l2w_mat) * normal;
    fragUV0 = uv0;

    gl_Position = camera.vp * worldPos;   }
