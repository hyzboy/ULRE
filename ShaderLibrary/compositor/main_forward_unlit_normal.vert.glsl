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
layout(set=L2W_SET, binding=L2W_BINDING) readonly buffer LocalToWorldData { mat4 mats[]; } l2w;

// Vertex attributes: Position + Normal + TransformID + MaterialInstanceID
layout(location=0) in vec3 Position;
layout(location=1) in vec3 Normal;
layout(location=2) in uint TransformID;
layout(location=3) in uint MaterialInstanceID;

// Outputs to FS
layout(location=0) flat out uint fragMaterialInstanceID;
layout(location=1) out vec3 fragWorldPos;
layout(location=2) out vec3 fragWorldNormal;

void main()
{
    mat4 l2w_mat = l2w.mats[TransformID];
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);
    vec3 worldNormal = normalize(mat3(l2w_mat) * Normal);

    fragMaterialInstanceID = MaterialInstanceID;
    fragWorldPos   = worldPos.xyz;
    fragWorldNormal = worldNormal;

    gl_Position = camera.vp * worldPos;
}
