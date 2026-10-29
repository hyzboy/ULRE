// ─────────────────────────────────────────────────────────────────────────────
// ⚠ 手工维护（暂不参与代码生成）
//   源：CMMath/inc/hgl/graph/CameraInfo.h → graph::CameraInfo
//   布局：bda=1 access=readonly align=64
//   说明：相机行表（世界私有），行号 = 相机槽 × 帧槽总数 + 帧槽。
//   为什么暂不生成：CameraInfo 位于 CMMath 模块（不属 ubo/ ssbo/ 两个独占目录），
//   待其迁入该目录并加 HGL_UBO_BDA 标注后再并入生成器。
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_UBO_CAMERA_INFO_GLSL
#define HGL_SHADERLIB_UBO_CAMERA_INFO_GLSL

struct CameraInfoData
{
    mat4  projection;
    mat4  inverse_projection;
    mat4  view;
    mat4  inverse_view;
    mat4  vp;
    mat4  inverse_vp;
    vec4  frustum_planes[6];
    mat4  sky;
    vec3  pos;
    float _pad_pos;
    vec3  view_line;
    float _pad_vl;
    vec3  world_up;
    float _pad_wu;
    vec3  camera_facing_up;
    float _pad_cfu;
    vec3  camera_facing_right;
    float _pad_cfr;
    float znear;
    float zfar;
    uint  use_reversed_z;
    float _pad_ci0;
    vec3  camera_world_pos;
    float _pad_cwp;
};

layout(buffer_reference, scalar, buffer_reference_align=64) readonly buffer CameraInfoBufferRef
{
    CameraInfoData cameras[];
};

#endif // HGL_SHADERLIB_UBO_CAMERA_INFO_GLSL
