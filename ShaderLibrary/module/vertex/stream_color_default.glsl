// @ulre begin
// @ulre name stream_color_default
// @ulre kind Utility
// @ulre priority 2
// @ulre provide Color
// @ulre end
// Stage 1: default vertex-color provider for meshes without a color attribute.
// White is the multiplicative identity used by every mesh master.
#ifndef S1_COLOR_DEFAULT_GLSL
#define S1_COLOR_DEFAULT_GLSL

vec4 GetVertexColor() { return vec4(1.0); }

#endif // S1_COLOR_DEFAULT_GLSL
