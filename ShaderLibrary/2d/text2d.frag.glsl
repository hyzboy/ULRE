// @ulre begin
// @ulre name text2d
// @ulre kind FragmentShader
// @ulre priority 0
// @ulre require Resource MaterialData
// @ulre require ProducedSemantic MaterialData
// @ulre require ProducedSemantic UV0
// @ulre end
// Text2D fragment shader
#include "common/alpha_compositor.glsl"

layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2D TextureText;

layout(location=0) flat in uint fragDataIndexID;
layout(location=1) in vec2 fragUV0;

layout(location=0) out vec4 FragColor;

void main()
{
    TransmissionSurfaceData material_data = MTL_DATA.data[fragDataIndexID];
    vec4 TextColor = unpackUnorm4x8(material_data.TextColor);
    float lum = texture(TextureText, fragUV0).r;
    FragColor = HGLComposeColor(vec4(TextColor.rgb * lum, TextColor.a));
}
