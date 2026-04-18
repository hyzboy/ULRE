layout(location=TEXCOORD_LOCATION) in vec2 TexCoord;

#include "common/ssbo_material_instance.glsl"

layout(location=0) out vec4 fragTextColor;
layout(location=1) out vec2 fragTexCoord;

void main()
{
    fragTextColor = unpackUnorm4x8(GetMaterialBindingInstance().TextColor);
    fragTexCoord = TexCoord;
    gl_Position = GetPosition2D();
}
