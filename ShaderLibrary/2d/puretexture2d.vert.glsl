// PureTexture2D / RectTexture2D vertex shader
#include "2d/common/vertex_prefix_2d.glsl"

layout(location=TEXCOORD_LOCATION) in vec2 TexCoord;

layout(location=0) out vec2 fragTexCoord;

void main()
{
    fragTexCoord = TexCoord;
    gl_Position = GetPosition2D();
}
