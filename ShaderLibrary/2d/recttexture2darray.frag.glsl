// @ulre begin
// @ulre name recttexture2darray
// @ulre kind FragmentShader
// @ulre priority 0
// @ulre require Resource MaterialData
// @ulre require ProducedSemantic MaterialData
// @ulre require ProducedSemantic UV0
// @ulre end
// Texture2DArray fragment shader
#include "common/alpha_compositor.glsl"

layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2DArray TextureBaseColor;

layout(location=0) flat in uint fragDataIndexID;
layout(location=1) in vec2 fragUV0;

layout(location=0) out vec4 FragColor;

void main()
{
    TextureRectArraySurfaceData material_data = MTL_DATA.data[fragDataIndexID];
    // layer 直接取 material_data.id.x：0-based 真层索引。
    uint layer = material_data.id.x;
    FragColor = HGLComposeColor(texture(TextureBaseColor, vec3(fragUV0, float(layer))));
}
