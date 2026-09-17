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

layout(set=SCENE_SET, binding=CAMERA_BINDING) uniform CameraInfo
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
    vec3 view_line;
    vec3 world_up;
    vec3 camera_facing_up;
    vec3 camera_facing_right;
    float znear, zfar;
    uint use_reversed_z;
    float _pad_ci0;
    vec3 camera_world_pos;
} camera;

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

#endif // HGL_SCENE_UBO_GLSL
