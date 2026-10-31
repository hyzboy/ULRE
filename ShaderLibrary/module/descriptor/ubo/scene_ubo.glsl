// @ulre begin
// @ulre name scene_ubo
// @ulre kind Utility
// @ulre priority 0
// @ulre provide Camera
// @ulre provide SkyLight
// @ulre provide Viewport
// @ulre uses descriptor_macros
// @ulre end
// 全局统一基础数据声明（**无绑定无集**）
//
// 本文件是**聚合入口**：每个结构一个独立文件，便于单独重新生成。
//
//   结构本体（生成物，唯一真源 = inc/hgl/graph/ubo/*.h）
//     module/descriptor/ubo/camera_info.glsl        graph::CameraInfo        （手工维护，源在 CMMath）
//     module/descriptor/ubo/ambient_env_info.glsl   AmbientEnvInfo
//     module/descriptor/ubo/viewport_info.glsl      ViewportInfo
//     module/descriptor/ubo/color_palette.glsl      ColorPalette
//     module/descriptor/ssbo/global_addresses.glsl   GlobalAddresses
//     module/descriptor/ssbo/world_addresses.glsl    WorldAddresses
//     module/descriptor/ssbo/light_row.glsl          LightRow
//     module/descriptor/ubo/shadow_info.glsl        ShadowCascadeInfo / ShadowInfo
//
//   寻址胶水（手工维护）
//     module/descriptor/ubo/scene_ubo_bindings.glsl  #define global_addresses / world_addresses /
//                                  ambient_env / viewport / color_palette /
//                                  light_rows / main_light / shadow_info / camera
//
// 本文件里的 UBO 声明已全部退场：CameraInfo / ColorPalette / GlobalAddresses 表本体 / sky /
// shadow / viewport 一律走 BDA —— 地址进地址表，按下标解引用。
// 宏名与成员名保持不变 ⇒ shader 正文与生成侧发射字符串零改动。

#ifndef HGL_SCENE_UBO_GLSL
#define HGL_SCENE_UBO_GLSL

#include "contract/descriptor_macros.glsl"

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

// ⚠ #extension 必须在**任何非预处理 token 之前**出现 ⇒ 含 64 位地址的结构
// （global_addresses / world_addresses）由本聚合入口统一启用扩展，子文件不自带扩展。

#include "module/descriptor/ubo/camera_info.glsl"
#include "module/descriptor/ubo/ambient_env_info.glsl"
#include "module/descriptor/ubo/viewport_info.glsl"
#include "module/descriptor/ubo/color_palette.glsl"
#include "module/descriptor/ssbo/global_addresses.glsl"
#include "module/descriptor/ssbo/world_addresses.glsl"
#include "module/descriptor/ssbo/light_row.glsl"
#include "module/descriptor/ubo/shadow_info.glsl"

#include "module/descriptor/ubo/scene_ubo_bindings.glsl"

#endif // HGL_SCENE_UBO_GLSL
