// Text2D fragment shader

#include "common/material_instance_ssbo.glsl"

struct MaterialInstance {
    uint TextColor;
};

layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2D TextureText;

MI_SSBO;

layout(location=0) flat in uint fragDataIndexID;
layout(location=1) in vec2 fragUV0;

layout(location=0) out vec4 FragColor;

void main()
{
    MaterialInstance mi = mtl.mi[fragDataIndexID];
    vec4 TextColor = unpackUnorm4x8(mi.TextColor);
    float lum = texture(TextureText, fragUV0).r;
    FragColor = vec4(TextColor.rgb * lum, TextColor.a);
}
