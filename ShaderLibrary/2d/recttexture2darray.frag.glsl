// @ulre begin
// @ulre name recttexture2darray
// @ulre kind FragmentShader
// @ulre priority 0
// @ulre require Resource MaterialData
// @ulre require ProducedSemantic MaterialData
// @ulre require ProducedSemantic UV0
// @ulre end
// RectTexture2DArray fragment shader

layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2DArray TextureBaseColor;

layout(location=0) flat in uint fragDataIndexID;
layout(location=1) in vec2 fragUV0;

layout(location=0) out vec4 FragColor;

void main()
{
    TextureRectArraySurfaceData mi = mtl.mi[fragDataIndexID];
    // layer 直接取 mi.id.x：0-based 真层索引（example 中 mi.id.x = 层号）
    uint layer = mi.id.x;
FragColor = texture(TextureBaseColor, vec3(fragUV0, float(layer)));
}
