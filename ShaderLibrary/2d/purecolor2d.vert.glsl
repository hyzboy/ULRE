#include "2d/common/vertex_prefix_2d.glsl"

struct MaterialInstance {
    vec4 Color;
};

#include "common/ssbo_material_instance.glsl"

layout(location=0) out vec4 fragColor;

void main()
{
    fragColor = GetMaterialInstance().Color;
    gl_Position = GetPosition2D();
}
