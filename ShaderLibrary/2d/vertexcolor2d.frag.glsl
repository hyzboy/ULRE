// @ulre begin
// @ulre name vertexcolor2d
// @ulre kind FragmentShader
// @ulre priority 0
// @ulre require ProducedSemantic Color
// @ulre end
// VertexColor2D fragment shader

layout(location=0) in vec4 fragVertexColor;

layout(location=0) out vec4 FragColor;

void main()
{
    FragColor = fragVertexColor;
}
