#include "2d/common/vertex_prefix_2d.glsl"

layout(location=TEXCOORD_LOCATION) in vec2 TexCoord;

struct MaterialInstance {
    uvec4 id;
};

#include "common/ssbo_material_instance.glsl"

layout(location=0) flat out uint fragLayer;
layout(location=1) out vec2 fragTexCoord;

void main()
{
    fragLayer = GetMaterialInstance().id.x;
    fragTexCoord = TexCoord;
    gl_Position = GetPosition2D();
}
