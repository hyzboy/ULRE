// @ulre begin
// @ulre name pure_color
// @ulre kind FragmentShader
// @ulre priority 0
// @ulre require Resource MaterialData
// @ulre require ProducedSemantic MaterialData
// @ulre uses alpha_compositor
// @ulre end
// PureColor built-in fallback fragment — single-color MTL_DATA surface.
// Shared by every transform graph. Used as missing material replacement.
#ifndef PURE_COLOR_FRAG_GLSL
#define PURE_COLOR_FRAG_GLSL
#version 450

layout(location=0) flat in uint fragDataIndexID;
#include "common/alpha_compositor.glsl"

layout(location=0) out vec4 outColor;

void main()
{
    outColor = MTL_DATA.data[fragDataIndexID].color;
}

#endif // PURE_COLOR_FRAG_GLSL
