// PureColor surface fragment program shared by every transform graph.
#version 450

layout(location=0) flat in uint fragDataIndexID;
#include "common/alpha_compositor.glsl"

layout(location=0) out vec4 outColor;

void main()
{
    outColor = MTL_DATA.data[fragDataIndexID].color;
}
