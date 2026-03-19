// RectTexture2DArray vertex shader
#include "2d/common/vertex_prefix_2d.glsl"

layout(location=TEXCOORD_LOCATION) in vec2 TexCoord;

layout(location=0) flat out uint fragMIID;
layout(location=1) out vec2 fragTexCoord;

void main()
{
    fragMIID = GET_MATERIAL_INSTANCE_ID();
    fragTexCoord = TexCoord;
    gl_Position = GetPosition2D();
}
