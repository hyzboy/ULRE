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
//   binding 0: CameraInfo camera
//   binding 1: SkyInfo sky
//   binding 2: ViewportInfo viewport
//   binding 3: ColorPalette color_palette
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

layout(set=SCENE_SET, binding=SKY_BINDING) uniform SkyInfo
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
} sky;

layout(set=SCENE_SET, binding=VIEWPORT_BINDING) uniform ViewportInfo
{
    mat4 ortho_matrix;
    uvec2 canvas_resolution;
    uvec2 viewport_resolution;
    vec2 inv_viewport_resolution;
} viewport;

layout(scalar, set=SCENE_SET, binding=COLOR_PALETTE_BINDING) uniform ColorPalette
{
    uint color[256];
} color_palette;

layout(set=SCENE_SET, binding=GLOBAL_ADDRESSES_BINDING) uniform GlobalAddressesInfo
{
    uint64_t addr_mesh_draw_params;
    uint64_t addr_pbr_surface;
    uint64_t addr_emissive_surface;
    uint64_t addr_transmission_surface;
    uint64_t addr_global_render_items;
    uint64_t addr_draw_item_ids;
    uint64_t addr_camera_info;
} global_addresses;

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
