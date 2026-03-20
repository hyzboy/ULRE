#include "2d/common/vertex_prefix_2d.glsl"

layout(location=TEXCOORD_LOCATION) in vec2 TexCoord;

struct MaterialInstance {
    uint TextColor;
};

#include "common/material_instance_ssbo.glsl"

layout(location=0) out vec4 fragTextColor;
layout(location=1) out vec2 fragTexCoord;

void main()
{
    fragTextColor = unpackUnorm4x8(GetMaterialInstance().TextColor);
    fragTexCoord = TexCoord;
    gl_Position = GetPosition2D();
}
