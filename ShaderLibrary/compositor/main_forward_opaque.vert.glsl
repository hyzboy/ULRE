#version 450

// === Compositor Template: Forward Opaque VS ===
// 自动生成 — 不要手动编辑此文件
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene    set=0 : camera=0, sky=1, viewport=2
//   Transform set=1 : l2w=0
//   Material set=2 : mtl=0

// Scene UBO declarations (shared with all shaders)
#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

// L2W SSBO — camera-relative local-to-world matrices, indexed by TransformID
#include "common/l2w_ssbo.glsl"
L2W_SSBO;

#if GEOMETRY_FETCH_SSBO
    // SSBO 顶点获取
    #include "common/vertex_fetch_ssbo.glsl"
#else
    // VBO 顶点获取
    layout(location=POSITION_LOCATION) in vec3 inPosition;
    layout(location=NORMAL_LOCATION) in vec3 inNormal;
    layout(location=TEXCOORD_LOCATION) in vec2 inUV0;
#endif

// ECS instance-rate attributes (dual ID)
#if GEOMETRY_FETCH_SSBO
    // SSBO 平台: TransformID = gl_InstanceIndex, MaterialInstanceID = gl_InstanceIndex
    #define GET_TRANSFORM_ID()          gl_InstanceIndex
    #define GET_MATERIAL_INSTANCE_ID()  gl_InstanceIndex
#else
    #include "common/transform_id_buffer.glsl"
    TRANSFORM_ID_BUFFER;
    // Descriptor path: per-instance TransformID comes from tid.ids[gl_InstanceIndex].
    #define GET_TRANSFORM_ID()          FetchTransformID()

    #include "common/material_instance_id_buffer.glsl"
    MATERIAL_INSTANCE_ID_BUFFER;
    #define GET_MATERIAL_INSTANCE_ID()  FetchMaterialInstanceID()
#endif

layout(location=0) out vec3 fragWorldPos;
layout(location=1) out vec3 fragWorldNormal;
layout(location=2) out vec2 fragUV0;
layout(location=3) flat out uint fragMaterialInstanceID;

void main()
{
    uint transformID = GET_TRANSFORM_ID();
    mat4 l2w_mat = l2w.mats[transformID];

#if GEOMETRY_FETCH_SSBO
    vec3 pos = FetchPosition(gl_VertexIndex);
    vec3 normal = FetchNormal(gl_VertexIndex);
    vec2 uv0 = FetchUV0(gl_VertexIndex);
#else
    vec3 pos = inPosition;
    vec3 normal = inNormal;
    vec2 uv0 = inUV0;
#endif

    vec4 worldPos = l2w_mat * vec4(pos, 1.0);   // camera-relative world position
    fragWorldPos = worldPos.xyz;
    fragWorldNormal = mat3(l2w_mat) * normal;
    fragUV0 = uv0;
    fragMaterialInstanceID = GET_MATERIAL_INSTANCE_ID();

    gl_Position = camera.vp * worldPos;   // vp 已是 camera-relative
}
