layout(location=TEXCOORD_LOCATION) in vec2 TexCoord;

#define HAS_TEXCOORD
#ifdef HAS_MI
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
#endif
#include "common/varying_vs.glsl"

void main()
{
#ifdef HAS_MI
    fragMaterialInstanceID = GetMaterialInstanceID();
#endif
    fragUV0 = TexCoord;
    gl_Position = GetPosition2D();
}
