#version 450

// === Compositor Template: Forward Unlit VS (Vertex Color) ===
// 无 MI 的顶点色材质 — 用于 VertexColor3D
//
// Descriptor binding 约定（固定布局）：
//   Scene    set=0 : camera=0, viewport=2
//   Transform set=1 : l2w=0
//   （无 Material set）

// Scene UBO
#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

// L2W SSBO
#include "common/l2w_ssbo.glsl"
L2W_SSBO;
#include "common/instance_rows_ssbo.glsl"
L2W_INDEX_ROWS_SSBO;

// Vertex attributes: Position + Color
layout(location=0) in vec3 Position;
layout(location=1) in vec4 Color;

// Output to FS
layout(location=0) out vec4 fragVertexColor;

void main()
{
    mat4 l2w_mat = l2w.mats[ResolveTransformID(gl_InstanceIndex)];
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);

    fragVertexColor = Color;

    gl_Position = camera.vp * worldPos;
}
