#version 450

// === Compositor Template: Billboard Fixed Size VS ===
// 固定像素尺寸 Billboard — 屏幕空间展开，面积不变
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene     set=0 : camera=0, viewport=1
//   Transform set=1 : l2w=0
//   Material  set=2 : mtl=0（MI SSBO, VS only）, TextureBaseColor=1（FS only）

#extension GL_EXT_scalar_block_layout : require

// Scene UBOs
#define VIEWPORT_BINDING 1
#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;
SCENE_VIEWPORT_UBO;

// L2W SSBO
#include "common/l2w_ssbo.glsl"
L2W_SSBO;

#if TRANSFORM_ID_FROM_DESCRIPTOR
    #include "common/transform_id_buffer.glsl"
    TRANSFORM_ID_BUFFER;
    #define GET_TRANSFORM_ID() FetchTransformID()
#else
    layout(location=1) in uint  TransformID;
    #define GET_TRANSFORM_ID() TransformID
#endif

// MI SSBO (Material set, VS only)
// Resort() 字母序: TextureBaseColor=0, mtl=1
#define MI_BINDING 1
#include "common/material_instance_ssbo.glsl"
struct MaterialInstance {
    uvec2 BillboardSize;
};
MI_SSBO_SCALAR;

// Vertex attributes: Position + TransformID + MaterialInstanceID
layout(location=0) in vec3  Position;
#if TRANSFORM_ID_FROM_DESCRIPTOR
layout(location=1) in uint  MaterialInstanceID;
#else
layout(location=2) in uint  MaterialInstanceID;
#endif

// Output to FS
layout(location=0) out vec2 fragTexCoord;

MaterialInstance GetMI() { return mtl.mi[MaterialInstanceID]; }

void main()
{
    MaterialInstance mi = GetMI();

    vec2 psize = vec2(mi.BillboardSize) / vec2(viewport.canvas_resolution);
    vec4 center_clip = camera.vp * l2w.mats[GET_TRANSFORM_ID()] * vec4(0.0, 0.0, 0.0, 1.0);
    vec2 center_ndc = center_clip.xy / center_clip.w;
    vec2 ndc = center_ndc + Position.xy * psize;

    fragTexCoord = vec2(Position.x + 0.5, Position.y + 0.5);

    gl_Position = vec4(ndc * center_clip.w, center_clip.z, center_clip.w);
}
