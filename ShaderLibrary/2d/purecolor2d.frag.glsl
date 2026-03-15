// PureColor2D fragment shader

struct MaterialInstance {
    vec4 Color;
};

layout(set=MI_SET, binding=MI_BINDING) readonly buffer MaterialInstanceData {
    MaterialInstance mi[];
} mtl;

layout(location=0) flat in uint fragMIID;

layout(location=0) out vec4 FragColor;

void main()
{
    FragColor = mtl.mi[fragMIID].Color;
}
