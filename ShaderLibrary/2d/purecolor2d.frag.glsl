// PureColor2D fragment shader

layout(location=0) flat in uint fragDataIndexID;

layout(location=0) out vec4 FragColor;

void main()
{
    FragColor = mtl.mi[fragDataIndexID].color;
}
