// RectTexture2DArray fragment shader

struct MaterialInstance {
    uvec4 id;
};

layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2DArray TextureBaseColor;

layout(set=MI_SET, binding=MI_BINDING) readonly buffer MaterialInstanceData {
    MaterialInstance mi[];
} mtl;

layout(location=0) flat in uint fragMIID;
layout(location=1) in vec2 fragTexCoord;

layout(location=0) out vec4 FragColor;

void main()
{
    MaterialInstance mi = mtl.mi[fragMIID];
    FragColor = texture(TextureBaseColor, vec3(fragTexCoord, mi.id.x));
}
