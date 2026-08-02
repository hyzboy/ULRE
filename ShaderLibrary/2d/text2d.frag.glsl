// Text2D fragment shader

struct TransmissionSurfaceData {
    uint TextColor;
};

layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2D TextureText;

layout(set=MI_SET, binding=MI_BINDING) readonly buffer TransmissionSurfaceBuffer {
    TransmissionSurfaceData mi[];
} mtl;

layout(location=0) flat in uint fragDataIndexID;
layout(location=1) in vec2 fragUV0;

layout(location=0) out vec4 FragColor;

void main()
{
    TransmissionSurfaceData mi = mtl.mi[fragDataIndexID];
    vec4 TextColor = unpackUnorm4x8(mi.TextColor);
    float lum = texture(TextureText, fragUV0).r;
    FragColor = vec4(TextColor.rgb * lum, TextColor.a);
}
