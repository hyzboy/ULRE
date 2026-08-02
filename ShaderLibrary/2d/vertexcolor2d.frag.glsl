// VertexColor2D fragment shader

layout(location=0) in vec4 fragVertexColor;

layout(location=0) out vec4 FragColor;

void main()
{
    FragColor = fragVertexColor;
}
