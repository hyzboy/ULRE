// @ulre begin
// @ulre name puretexture2d
// @ulre kind FragmentShader
// @ulre priority 0
// @ulre require ProducedSemantic UV0
// @ulre end
// UnlitTexture fragment shader
#include "common/alpha_compositor.glsl"

layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2D TextureBaseColor;

layout(location=0) in vec2 fragUV0;

layout(location=0) out vec4 FragColor;

void main()
{
    FragColor = HGLComposeColor(texture(TextureBaseColor, fragUV0));
}
