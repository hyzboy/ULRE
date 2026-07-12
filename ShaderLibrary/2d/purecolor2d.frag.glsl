// PureColor2D fragment shader

#include "common/material_instance_ssbo.glsl"

struct MaterialInstance {
    vec4 Color;
};

MI_SSBO;

layout(location=0) flat in uint fragDataIndexID;

layout(location=0) out vec4 FragColor;

void main()
{
    FragColor = mtl.mi[fragDataIndexID].Color;
}
