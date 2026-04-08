#include "instance_data/Color4f.glsl"

#include "common/ssbo_material_instance.glsl"

layout(location=0) out vec4 fragColor;

void main()
{
    fragColor = GetMaterialInstance().color;
    gl_Position = GetPosition2D();
}
