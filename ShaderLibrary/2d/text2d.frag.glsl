// Text2D fragment shader

#include "common/material_instance_ssbo.glsl"

struct MaterialInstance {
    uint TextColor;
};

MI_SSBO;

layout(location=0) flat in uint fragMIID;
layout(location=1) in vec2 fragTexCoord;

layout(location=0) out vec4 FragColor;

void main()
{
    MaterialInstance mi = mtl.mi[fragMIID];
    vec4 TextColor = unpackUnorm4x8(mi.TextColor);
    float lum = GetSamplerText(fragTexCoord).r;
    FragColor = vec4(TextColor.rgb * lum, TextColor.a);
}
