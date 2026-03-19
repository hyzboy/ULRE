#version 450

// === Compositor Template: Forward Unlit VS (Luminance, vec2 Position) ===
// 顶点亮度材质 — 用于 VertexLuminance3D (vec2)
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

// Vertex attributes: Position(vec2) + Luminance + TransformID + MaterialInstanceID
layout(location=POSITION_LOCATION) in vec2 Position;
layout(location=LUMINANCE_LOCATION) in float Luminance;

// Output to FS
layout(location=0) flat out uint fragMaterialInstanceID;
layout(location=1)      out float fragLuminance;

void main()
{
    mat4 l2w_mat = l2w.mats[GET_TRANSFORM_ID()];
    vec4 worldPos = l2w_mat * vec4(Position, 0.0, 1.0);

    fragMaterialInstanceID = GET_MATERIAL_INSTANCE_ID();
    fragLuminance = Luminance;

    gl_Position = camera.vp * worldPos;
}
