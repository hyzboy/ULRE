// @ulre begin
// @ulre name scene_ubo
// @ulre kind Utility
// @ulre priority 0
// @ulre provide Camera
// @ulre provide SkyLight
// @ulre provide Viewport
// @ulre end
// Scene 集（Set 0）全局统一基础 UBO 声明
//
// 对应 SceneBinding 枚举（固定 ABI）：
//   （binding 0 的 CameraInfo UBO 已删，相机数据走 BDA）
//   binding 0: SkyInfo sky
//   binding 1: ViewportInfo viewport
//   （原 binding 2 的 ColorPalette UBO 已删：调色板构造期写入、长期有效，
//     地址经 global_addresses.addr_color_palette 以 buffer_reference 读取）
//   （原 GlobalAddressesInfo UBO 已删：表本体改为 SSBO，基址经
//     pc_root.addr_global_addresses 下发，宏名不变）
//
// 由 C++ 全局绑定，一次性声明，未使用的 block 在 SPIR-V 编译期自动剔除。

#ifndef HGL_SCENE_UBO_GLSL
#define HGL_SCENE_UBO_GLSL

#include "common/descriptor_macros.glsl"

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

struct CameraInfoData
{
    mat4 projection;
    mat4 inverse_projection;
    mat4 view;
    mat4 inverse_view;
    mat4 vp;
    mat4 inverse_vp;
    vec4 frustum_planes[6];
    mat4 sky;
    vec3 pos;
    float _pad_pos;
    vec3 view_line;
    float _pad_vl;
    vec3 world_up;
    float _pad_wu;
    vec3 camera_facing_up;
    float _pad_cfu;
    vec3 camera_facing_right;
    float _pad_cfr;
    float znear, zfar;
    uint use_reversed_z;
    float _pad_ci0;
    vec3 camera_world_pos;
    float _pad_cwp;
};

layout(buffer_reference, scalar, buffer_reference_align=64) readonly buffer CameraInfoBufferRef
{
    CameraInfoData cameras[];
};

// sky（S2：地址进表，退出 Scene 集绑定）：sky 是单份 buffer ⇒ 所有帧槽同址。
// 宏名与成员名均不变 ⇒ 正文与生成侧的 `sky.*` 读点零改动。
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer SkyInfoRef
{
    vec4 base_sky_color;
    vec4 sun_direction;
    vec4 sun_color;
    vec4 halo_color;
    vec4 moon_color;
    float sun_ang_deg;
    float sun_intensity;
    float moon_intensity;
    float halo_intensity;
    uvec4 env_tex;
};

#define sky SkyInfoRef(global_addresses.addr_sky)

layout(set=SCENE_SET, binding=VIEWPORT_BINDING) uniform ViewportInfo
{
    mat4 ortho_matrix;
    uvec2 canvas_resolution;
    uvec2 viewport_resolution;
    vec2 inv_viewport_resolution;
} viewport;

// 调色板（256 项 RGBA8 打包，unpackUnorm4x8 解码）：构造期一次写入、长期有效
// ⇒ 地址随地址表下发（不进 pc_root —— 后者只承载每 pass / 每帧变化的地址）。
// 宏名保持不变：shader 正文与生成侧发射的
//   unpackUnorm4x8(color_palette.color[ColorIndex]) 零改动。
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer ColorPaletteRef
{
    uint color[256];
};

#define color_palette ColorPaletteRef(global_addresses.addr_color_palette)

// 全局地址表（**无绑定无集**）：表本体是 SSBO，基址经 pc_root.addr_global_addresses 下发。
// 宏名不变 ⇒ 所有 `global_addresses.addr_*` 读点（含生成侧发射的 MTL_ROW / camera 宏）
// 与 shader 正文零改动。
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer GlobalAddressesRef
{
    uint64_t addr_mesh_draw_params;
    uint64_t addr_pbr_surface;
    uint64_t addr_emissive_surface;
    uint64_t addr_transmission_surface;
    uint64_t addr_global_render_items;
    uint64_t addr_draw_item_ids;
    uint64_t addr_camera_info;
    uint64_t addr_color_palette;
    // 每帧槽字段：表按 HGL_FRAME_SLOT_TOTAL 多份，pc_root.addr_global_addresses
    // 指向「本帧那一槽」，所以这几个地址总是当前帧的数据。
    uint64_t addr_sky;
};

#define global_addresses GlobalAddressesRef(pc_root.addr_global_addresses)

struct ShadowCascadeInfo
{
    mat4 shadow_vp;
    vec4 shadow_params;
    vec2 shadow_map_size;
    vec2 inv_shadow_map_size;
    uvec4 shadow_tex;
    vec4 cascade_params;
    vec4 cache_origin;
    uvec4 cache_offset;
    uvec4 cache_valid_rect;
};

layout(set=SCENE_SET, binding=SHADOW_BINDING) uniform ShadowInfo
{
    mat4 shadow_vp;
    vec4 shadow_params;
    vec2 shadow_map_size;
    vec2 inv_shadow_map_size;
    uvec4 shadow_tex;
    uvec4 csm_params;
    ShadowCascadeInfo cascades[4];
} shadow;

#define camera CameraInfoBufferRef(global_addresses.addr_camera_info).cameras[pc_root.camera_id]

#endif // HGL_SCENE_UBO_GLSL
