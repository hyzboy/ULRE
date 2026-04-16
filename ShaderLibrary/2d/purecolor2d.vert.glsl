#include "common/ssbo_material_instance.glsl"

layout(location=0) out vec4 fragColor;

void main()
{
    fragColor = GetMaterialInstance().Color;
    gl_Position = GetPosition2D();
}
