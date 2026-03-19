#version 450

// === Compositor Template: Forward Unlit VS (Palette Color) ===
// 调色板索引材质 — 用于 VertexPattleColor3D
// VS 从 color_pattle UBO 查表得到 vec4，传给 FS
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene     set=0 : camera=0, viewport=1
//   Transform set=1 : l2w=0
//   Material  set=2 : color_pattle=0

#extension GL_EXT_scalar_block_layout : require

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
    layout(location=TRANSFORM_ID_LOCATION) in uint  TransformID;
    #define GET_TRANSFORM_ID() TransformID
#endif

// Color palette UBO (Material set, VS only)
layout(scalar, set=MATERIAL_SET, binding=COLOR_PATTLE_BINDING) uniform ColorPattle { vec4 color[256]; } color_pattle;

// Vertex attributes: Position + ColorIndex(uint) + TransformID
layout(location=POSITION_LOCATION) in vec3  Position;
layout(location=COLOR_LOCATION) in uint  ColorIndex;

// Output to FS
layout(location=0) out vec4 fragVertexColor;

void main()
{
    mat4 l2w_mat = l2w.mats[GET_TRANSFORM_ID()];
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);

    fragVertexColor = color_pattle.color[ColorIndex];

    gl_Position = camera.vp * worldPos;
}
