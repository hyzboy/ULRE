layout(location=COLOR_LOCATION) in vec4 Color;

layout(location=0) out vec4 fragColor;

void main()
{
    fragColor = Color;
    gl_Position = GetPosition2D();
}
