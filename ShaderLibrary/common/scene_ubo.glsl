// scene_ubo.glsl — Scene 描述符集 UBO 共享定义
// 对应 C++: UBOCommon.h 中的 SBS_ViewportInfo / SBS_CameraInfo / SBS_SkyInfo
//
// 结构体名与实例名与 ShaderGen 一致：
//   uniform ViewportInfo { ... } viewport;
//   uniform CameraInfo   { ... } camera;
//   uniform SkyInfo      { ... } sky;
//
// 使用方式：
//   #include "common/descriptor_macros.glsl"
//   #include "common/scene_ubo.glsl"
//   SCENE_CAMERA_UBO;
//   SCENE_SKY_UBO;
//   SCENE_VIEWPORT_UBO;

#ifndef SCENE_UBO_GLSL
#define SCENE_UBO_GLSL

// ---- ViewportInfo (C++: SBS_ViewportInfo, instance "viewport") ----
#define SCENE_VIEWPORT_UBO \
    layout(set=SCENE_SET, binding=VIEWPORT_BINDING) uniform ViewportInfo \
    { \
        mat4 ortho_matrix; \
        uvec2 canvas_resolution; \
        uvec2 viewport_resolution; \
        vec2 inv_viewport_resolution; \
    } viewport

// ---- CameraInfo (C++: SBS_CameraInfo, instance "camera") ----
#define SCENE_CAMERA_UBO \
    layout(set=SCENE_SET, binding=CAMERA_BINDING) uniform CameraInfo \
    { \
        mat4 projection; \
        mat4 inverse_projection; \
        mat4 view; \
        mat4 inverse_view; \
        mat4 vp; \
        mat4 inverse_vp; \
        vec4 frustum_planes[6]; \
        mat4 sky; \
        vec3 pos; \
        vec3 view_line; \
        vec3 world_up; \
        vec3 billboard_up; \
        vec3 billboard_right; \
        float znear, zfar; \
        uint use_reversed_z; \
        float _pad_ci0; \
        vec3 camera_world_pos; \
    } camera

// ---- SkyInfo (C++: SBS_SkyInfo, instance "sky") ----
#define SCENE_SKY_UBO \
    layout(set=SCENE_SET, binding=SKY_BINDING) uniform SkyInfo \
    { \
        vec4 base_sky_color; \
        vec4 sun_direction; \
        vec4 sun_color; \
        vec4 halo_color; \
        vec4 moon_color; \
        float sun_ang_deg; \
        float sun_intensity; \
        float moon_intensity; \
        float halo_intensity; \
    } sky

#endif // SCENE_UBO_GLSL
