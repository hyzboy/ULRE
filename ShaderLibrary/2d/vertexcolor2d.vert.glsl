// VertexColor2D vertex shader
#include "2d/common/vertex_prefix_2d.glsl"

layout(location=COLOR_LOCATION) in vec4 Color;

layout(location=0) out vec4 fragColor;

void main()
{
    fragColor = Color;
    gl_Position = GetPosition2D();
}
