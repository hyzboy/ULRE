#version 450

// === Compositor Template: Forward Unlit VS ===
// Unlit 材质专用 — 仅需 Position，无 Normal/UV
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene    set=0 : camera=0, viewport=1   (viewport 可选，不一定使用)
//   Transform set=1 : l2w=0
//   Material set=2 : mtl=0

// Scene UBO
#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

// L2W SSBO — camera-relative local-to-world matrices, indexed by TransformID
#include "common/l2w_ssbo.glsl"
L2W_SSBO;

// Vertex attributes — minimal: Position + dual instance-rate IDs
layout(location=0) in vec3 Position;
layout(location=1) in uint TransformID;
layout(location=2) in uint MaterialInstanceID;

// Outputs to FS
layout(location=0) flat out uint fragMaterialInstanceID;

void main()
{
    mat4 l2w_mat = l2w.mats[TransformID];
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);
    fragMaterialInstanceID = MaterialInstanceID;
    gl_Position = camera.vp * worldPos;
}
