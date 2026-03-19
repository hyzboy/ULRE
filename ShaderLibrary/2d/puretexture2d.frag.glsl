// PureTexture2D / RectTexture2D fragment shader

layout(location=0) in vec2 fragTexCoord;

layout(location=0) out vec4 FragColor;

void main()
{
    FragColor = texture(TextureBaseColor, fragTexCoord);
}
