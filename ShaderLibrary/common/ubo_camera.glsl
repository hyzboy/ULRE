#ifndef UBO_CAMERA_GLSL
#define UBO_CAMERA_GLSL

layout(set=PERFRAME_SET, binding=CAMERA_BINDING) uniform CameraInfo
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
    vec3 billboard_up;
    vec3 billboard_right;
    float znear, zfar;
    uint use_reversed_z;
    float _pad_ci0;
    vec3 camera_world_pos;
} camera;

#endif