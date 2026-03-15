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
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO(0, 0);
SCENE_VIEWPORT_UBO(0, 1);

// L2W SSBO
layout(set=1, binding=0) readonly buffer LocalToWorldData { mat4 mats[]; } l2w;

// MI SSBO (Material set, VS only)
// Resort() 字母序: TextureBaseColor=0, mtl=1
struct MaterialInstance {
    uvec2 BillboardSize;
};
layout(scalar, set=2, binding=1) readonly buffer MaterialInstanceData {
    MaterialInstance items[];
} mtl;

// Vertex attributes: Position + TransformID + MaterialInstanceID
layout(location=0) in vec3  Position;
layout(location=1) in uint  TransformID;
layout(location=2) in uint  MaterialInstanceID;

// Output to FS
layout(location=0) out vec2 fragTexCoord;

MaterialInstance GetMI() { return mtl.items[MaterialInstanceID]; }

void main()
{
    MaterialInstance mi = GetMI();

    vec2 psize = vec2(mi.BillboardSize) / vec2(viewport.canvas_resolution);
    vec4 center_clip = camera.vp * l2w.mats[TransformID] * vec4(0.0, 0.0, 0.0, 1.0);
    vec2 center_ndc = center_clip.xy / center_clip.w;
    vec2 ndc = center_ndc + Position.xy * psize;

    fragTexCoord = vec2(Position.x + 0.5, Position.y + 0.5);

    gl_Position = vec4(ndc * center_clip.w, center_clip.z, center_clip.w);
}
