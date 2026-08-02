// RectTexture2DArray fragment shader

struct TextureRectArraySurfaceData {
    uvec4 id;
};

layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2DArray TextureBaseColor;

layout(set=MI_SET, binding=MI_BINDING) readonly buffer TextureRectArraySurfaceBuffer {
    TextureRectArraySurfaceData mi[];
} mtl;

layout(location=0) flat in uint fragDataIndexID;
layout(location=1) flat in uint fragTextureLayerID;
layout(location=2) in vec2 fragUV0;

layout(location=0) out vec4 FragColor;

void main()
{
    TextureRectArraySurfaceData mi = mtl.mi[fragDataIndexID];
    uint layer = fragTextureLayerID;
    if (layer == 0u && mi.id.x != 0u)
        layer = mi.id.x;
FragColor = texture(TextureBaseColor, vec3(fragUV0, float(layer)));
}
