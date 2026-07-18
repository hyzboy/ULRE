#version 450

// === Compositor Template: Billboard Fixed Size VS ===
// 固定像素尺寸 Billboard — 屏幕空间展开，面积不变
//
// Descriptor binding 约定（固定布局）：
//   Scene     set=0 : camera=0, viewport=2
//   Transform set=1 : l2w=0
//   Material  set=2 : mtl=0（MI SSBO, VS only）, TextureBaseColor=3（FS only）

#extension GL_EXT_scalar_block_layout : require

// Scene UBOs
#define VIEWPORT_BINDING 2
#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;
SCENE_VIEWPORT_UBO;

// L2W SSBO
#include "common/l2w_ssbo.glsl"
L2W_SSBO;
#include "common/instance_rows_ssbo.glsl"
L2W_INDEX_ROWS_SSBO;

// MI SSBO (Material set, VS only)
// 固定 Material 绑定：mtl=0；若同 set 含经典纹理采样器，则从 binding 3 起分配
#include "common/material_instance_ssbo.glsl"
struct MaterialInstance {
    uvec2 BillboardSize;
};
MI_SSBO_SCALAR;
DATA_INDEX_ROWS_SSBO;
TEXTURE_LAYER_ROWS_SSBO;

// Vertex attributes: Position
layout(location=0) in vec3  Position;

// Output to FS
layout(location=0) out vec2 fragTexCoord;

MaterialInstance GetMI() { return mtl.mi[ResolveDataIndexID(gl_InstanceIndex)]; }

void main()
{
    MaterialInstance mi = GetMI();

    vec2 psize = vec2(mi.BillboardSize) / vec2(viewport.canvas_resolution);
    vec4 center_clip = camera.vp * l2w.mats[ResolveTransformID(gl_InstanceIndex)] * vec4(0.0, 0.0, 0.0, 1.0);
    vec2 center_ndc = center_clip.xy / center_clip.w;
    vec2 ndc = center_ndc + Position.xy * psize;

    fragTexCoord = vec2(Position.x + 0.5, Position.y + 0.5);

    gl_Position = vec4(ndc * center_clip.w, center_clip.z, center_clip.w);
}
