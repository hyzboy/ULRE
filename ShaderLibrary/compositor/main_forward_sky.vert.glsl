#version 450

// === Compositor Template: Forward Sky VS ===
// Sky dome 专用 — Position(0) + TransformID(1), 输出 Direction
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene     set=0 : camera=0, sky=1, viewport=2
//   Transform set=1 : l2w=0

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
    layout(location=1) in uint TransformID;
    #define GET_TRANSFORM_ID() TransformID
#endif

// Vertex attributes: Position + TransformID
layout(location=0) in vec3 Position;

// Output to FS: sky direction
layout(location=0) out vec3 fragDirection;

void main()
{
    fragDirection = normalize(Position);

    mat4 l2w_mat = l2w.mats[GET_TRANSFORM_ID()];
    gl_Position = camera.vp * l2w_mat * vec4(Position, 1.0);
}
