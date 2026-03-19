#version 450

// === Compositor Template: Forward Unlit VS (with Normal) ===
// Unlit 材质 + Normal 属性 — 用于 Gizmo3D 等需要法线做自定义光照的材质
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene    set=0 : camera=0, viewport=1
//   Transform set=1 : l2w=0
//   Material set=2 : mtl=0

// Scene UBO
#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

// L2W SSBO
#include "common/l2w_ssbo.glsl"
L2W_SSBO;

#if TRANSFORM_ID_FROM_DESCRIPTOR
    #include "common/transform_id_buffer.glsl"
    TRANSFORM_ID_BUFFER;
    #define GET_TRANSFORM_ID() FetchTransformID()
#else
    layout(location=TRANSFORM_ID_LOCATION) in uint TransformID;
    #define GET_TRANSFORM_ID() TransformID
#endif

#if MATERIAL_INSTANCE_ID_FROM_DESCRIPTOR
    #include "common/material_instance_id_buffer.glsl"
    MATERIAL_INSTANCE_ID_BUFFER;
    #define GET_MATERIAL_INSTANCE_ID() FetchMaterialInstanceID()
#else
    layout(location=MATERIAL_INSTANCE_ID_LOCATION) in uint MaterialInstanceID;
    #define GET_MATERIAL_INSTANCE_ID() MaterialInstanceID
#endif

// Vertex attributes: Position + Normal + TransformID + MaterialInstanceID
layout(location=POSITION_LOCATION) in vec3 Position;
layout(location=NORMAL_LOCATION) in vec3 Normal;

// Outputs to FS
layout(location=0) flat out uint fragMaterialInstanceID;
layout(location=1) out vec3 fragWorldPos;
layout(location=2) out vec3 fragWorldNormal;

void main()
{
    mat4 l2w_mat = l2w.mats[GET_TRANSFORM_ID()];
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);
    vec3 worldNormal = normalize(mat3(l2w_mat) * Normal);

    fragMaterialInstanceID = GET_MATERIAL_INSTANCE_ID();
    fragWorldPos   = worldPos.xyz;
    fragWorldNormal = worldNormal;

    gl_Position = camera.vp * worldPos;
}
