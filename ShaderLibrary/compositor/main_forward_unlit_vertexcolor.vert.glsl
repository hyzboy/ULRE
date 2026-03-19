#version 450

// === Compositor Template: Forward Unlit VS (Vertex Color) ===
// 无 MI 的顶点色材质 — 用于 VertexColor3D
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene    set=0 : camera=0, viewport=1
//   Transform set=1 : l2w=0
//   （无 Material set）

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

// Vertex attributes: Position + Color + TransformID（无 MaterialInstanceID）
layout(location=POSITION_LOCATION) in vec3 Position;
layout(location=COLOR_LOCATION) in vec4 Color;

// Output to FS
layout(location=0) out vec4 fragVertexColor;

void main()
{
    mat4 l2w_mat = l2w.mats[GET_TRANSFORM_ID()];
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);

    fragVertexColor = Color;

    gl_Position = camera.vp * worldPos;
}
