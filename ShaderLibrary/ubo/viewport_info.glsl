// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ubo/ViewportInfo.h → ViewportInfo
//   布局：bda=1 access=readonly align=16
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_UBO_VIEWPORT_INFO_GLSL
#define HGL_SHADERLIB_UBO_VIEWPORT_INFO_GLSL

struct ViewportInfoData
{
    mat4  ortho_matrix;
    uvec2 canvas_resolution;
    uvec2 viewport_resolution;
    vec2  inv_viewport_resolution;
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer ViewportInfoRef
{
    mat4  ortho_matrix;
    uvec2 canvas_resolution;
    uvec2 viewport_resolution;
    vec2  inv_viewport_resolution;
};

#endif // HGL_SHADERLIB_UBO_VIEWPORT_INFO_GLSL
