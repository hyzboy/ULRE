#version 450

// === Compositor Template: Forward Opaque VS ===
// 自动生成 — 不要手动编辑此文件

// Scene UBO declarations (shared with all shaders)
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO(0, 1);
layout(set=1, binding=0) readonly buffer L2W_SSBO { mat4 transforms[]; };  // camera-relative L2W

#if GEOMETRY_FETCH_SSBO
    // SSBO 顶点获取
    #include "common/vertex_fetch_ssbo.glsl"
#else
    // VBO 顶点获取
    layout(location=0) in vec3 inPosition;
    layout(location=1) in vec3 inNormal;
    layout(location=2) in vec2 inUV0;
#endif

// Instance ID 获取
#if GEOMETRY_FETCH_SSBO
    // SSBO 平台: gl_InstanceIndex
    #define GET_INSTANCE_ID() gl_InstanceIndex
#else
    // VBO 平台: instance-rate attribute
    layout(location=10) in uint inInstanceID;
    #define GET_INSTANCE_ID() inInstanceID
#endif

layout(location=0) out vec3 fragWorldPos;
layout(location=1) out vec3 fragWorldNormal;
layout(location=2) out vec2 fragUV0;
layout(location=3) flat out uint fragInstanceID;

void main()
{
    uint instanceID = GET_INSTANCE_ID();
    mat4 l2w = transforms[instanceID];

#if GEOMETRY_FETCH_SSBO
    vec3 pos = FetchPosition(gl_VertexIndex);
    vec3 normal = FetchNormal(gl_VertexIndex);
    vec2 uv0 = FetchUV0(gl_VertexIndex);
#else
    vec3 pos = inPosition;
    vec3 normal = inNormal;
    vec2 uv0 = inUV0;
#endif

    vec4 worldPos = l2w * vec4(pos, 1.0);   // camera-relative world position
    fragWorldPos = worldPos.xyz;
    fragWorldNormal = mat3(l2w) * normal;
    fragUV0 = uv0;
    fragInstanceID = instanceID;

    gl_Position = camera.vp * worldPos;   // vp 已是 camera-relative
}
