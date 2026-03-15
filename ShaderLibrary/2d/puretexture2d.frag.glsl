// PureTexture2D / RectTexture2D fragment shader

layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2D TextureBaseColor;

layout(location=0) in vec2 fragTexCoord;

layout(location=0) out vec4 FragColor;

void main()
{
    FragColor = texture(TextureBaseColor, fragTexCoord);
}
