// @ulre begin
// @ulre name puretexture2d
// @ulre kind FragmentShader
// @ulre priority 0
// @ulre require ProducedSemantic UV0
// @ulre end
// PureTexture2D / RectTexture2D fragment shader

layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2D TextureBaseColor;

layout(location=0) in vec2 fragUV0;

layout(location=0) out vec4 FragColor;

void main()
{
    FragColor = texture(TextureBaseColor, fragUV0);
}
