#include "2d/common/vertex_prefix_2d.glsl"

layout(location=TEXCOORD_LOCATION) in vec2 TexCoord;

layout(location=0) out vec2 fragTexCoord;

#ifdef HAS_MI
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
layout(location=1) flat out uint fragMaterialInstanceID;
#endif

void main()
{
    fragTexCoord = TexCoord;
#ifdef HAS_MI
    fragMaterialInstanceID = GetMaterialInstanceID();
#endif
    gl_Position = GetPosition2D();
}
