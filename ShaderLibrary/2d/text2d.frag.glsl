// Text2D fragment shader

struct MaterialInstance {
    uint TextColor;
};

layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2D TextureText;

layout(set=MI_SET, binding=MI_BINDING) readonly buffer MaterialInstanceData {
    MaterialInstance mi[];
} mtl;

layout(location=0) flat in uint fragMIID;
layout(location=1) in vec2 fragTexCoord;

layout(location=0) out vec4 FragColor;

void main()
{
    MaterialInstance mi = mtl.mi[fragMIID];
    vec4 TextColor = unpackUnorm4x8(mi.TextColor);
    float lum = texture(TextureText, fragTexCoord).r;
    FragColor = vec4(TextColor.rgb * lum, TextColor.a);
}
