
layout(location=0) in vec4 fragTextColor;
layout(location=1) in vec2 fragTexCoord;

layout(location=0) out vec4 FragColor;

void main()
{
    float lum = GetSamplerText(GetMaterialInstanceID(), fragTexCoord).r;
    FragColor = vec4(fragTextColor.rgb * lum, fragTextColor.a);
}
