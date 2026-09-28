// @ulre begin
// @ulre name scene_ubo
// @ulre kind Utility
// @ulre priority 0
// @ulre provide Camera
// @ulre provide SkyLight
// @ulre provide Viewport
// @ulre end
// 全局统一基础数据声明（**无绑定无集**）
//
// 本文件里的 UBO 声明已全部退场：CameraInfo / ColorPalette / GlobalAddresses 表本体 / sky /
// shadow / viewport 一律走 BDA —— 地址进 global_addresses 表（表基址经
// pc_root.addr_global_addresses 下发），按下标解引用。宏名与成员名保持不变
// ⇒ shader 正文与生成侧发射字符串零改动。

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

// viewport（S2：地址进表 —— 单份 buffer，内容按 pass/RT 覆盖写、地址恒定 ⇒ 全帧槽同址）
// 退出 Scene 集绑定；宏名与成员名不变 ⇒ `viewport.*` 读点零改动。
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer ViewportInfoRef
{
    mat4 ortho_matrix;
    uvec2 canvas_resolution;
    uvec2 viewport_resolution;
    vec2 inv_viewport_resolution;
};

#define viewport ViewportInfoRef(global_addresses.addr_viewport)

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
    uint64_t addr_mesh_draw_params_pool;   // 池基址；本批那一块在 pc_root.addr_batch_mesh_draw_params
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
    uint64_t addr_viewport;
    uint64_t addr_shadow;
};

#define global_addresses GlobalAddressesRef(pc_root.addr_global_addresses)

// 世界地址表（**无绑定无集**）：世界私有 SSBO 的地址表，基址经 pc_root.addr_world_addresses 下发。
// 为什么独立成表：表内都是**世界私有** buffer（相机行表 / 4-ID 渲染项表 / DrawItemID 表，
// 后续 C2 迁入 sky / shadow / env）。一个设备上可同时存在多个世界（主世界 + OffscreenWorld），
// 放进全局表只能表达"最后一个世界"的地址 ⇒ 多世界同帧互踩。
// 表本体与 GlobalAddresses 同形：HGL_FRAME_SLOT_TOTAL 份 × kWorldAddressesSlotStride，
// pc_root 指向"本帧那一槽"。
//（定稿见 doc/world-addresses-and-camera-model-plan.md §1）
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer WorldAddressesRef
{
    uint64_t addr_camera_info;          // 相机行表（行号 = 相机槽 × 帧槽总数 + 帧槽）
    uint64_t addr_global_render_items;  // 4-ID 渲染项表（世界私有）
    uint64_t addr_draw_item_ids;        // DrawItemID 压缩索引表（世界私有）
};

#define world_addresses WorldAddressesRef(pc_root.addr_world_addresses)

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

// shadow（S2：地址进表，**按帧槽各一份** —— ring[i] 对应帧槽 i）退出 Scene 集绑定。
// 宏名与成员名不变 ⇒ `shadow.*` 读点零改动。
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer ShadowInfoRef
{
    mat4 shadow_vp;
    vec4 shadow_params;
    vec2 shadow_map_size;
    vec2 inv_shadow_map_size;
    uvec4 shadow_tex;
    uvec4 csm_params;
    ShadowCascadeInfo cascades[4];
};

#define shadow ShadowInfoRef(global_addresses.addr_shadow)

#define camera CameraInfoBufferRef(global_addresses.addr_camera_info).cameras[pc_root.camera_row]

#endif // HGL_SCENE_UBO_GLSL
