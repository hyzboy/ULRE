// PureColor2D vertex shader
#include "2d/common/vertex_prefix_2d.glsl"

layout(location=0) flat out uint fragMIID;

void main()
{
    fragMIID = GET_MATERIAL_INSTANCE_ID();
    gl_Position = GetPosition2D();
}
