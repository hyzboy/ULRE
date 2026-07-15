// Text2D vertex shader
#include "2d/common/vertex_prefix_2d.glsl"

layout(location=NEXT_LOC) in vec2 TexCoord;

layout(location=0) flat out uint fragDataIndexID;
layout(location=1) out vec2 fragTexCoord;

void main()
{
    fragDataIndexID = GetDataIndexID2D();
    fragTexCoord = TexCoord;
    gl_Position = GetPosition2D();
}
