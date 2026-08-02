// PureColor2D fragment shader

struct EmissiveSurfaceData {
    vec4 color;
};

layout(set=MI_SET, binding=MI_BINDING) readonly buffer EmissiveSurfaceBuffer {
    EmissiveSurfaceData mi[];
} mtl;

layout(location=0) flat in uint fragDataIndexID;

layout(location=0) out vec4 FragColor;

void main()
{
    FragColor = mtl.mi[fragDataIndexID].color;
}
