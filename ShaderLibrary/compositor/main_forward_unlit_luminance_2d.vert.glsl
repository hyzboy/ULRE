#version 450

// === Compositor Template: Forward Unlit VS (Luminance, vec2 Position) ===
// 顶点亮度材质 — 用于 VertexLuminance3D (vec2)
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene    set=0 : camera=0, viewport=1
//   Transform set=1 : l2w=0
//   Material set=2 : mtl=0

// Scene UBO
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO(0, 0);

// L2W SSBO
layout(set=1, binding=0) readonly buffer LocalToWorldData { mat4 mats[]; } l2w;

// Vertex attributes: Position(vec2) + Luminance + TransformID + MaterialInstanceID
layout(location=0) in vec2 Position;
layout(location=1) in float Luminance;
layout(location=2) in uint TransformID;
layout(location=3) in uint MaterialInstanceID;

// Output to FS
layout(location=0) flat out uint fragMaterialInstanceID;
layout(location=1)      out float fragLuminance;

void main()
{
    mat4 l2w_mat = l2w.mats[TransformID];
    vec4 worldPos = l2w_mat * vec4(Position, 0.0, 1.0);

    fragMaterialInstanceID = MaterialInstanceID;
    fragLuminance = Luminance;

    gl_Position = camera.vp * worldPos;
}
