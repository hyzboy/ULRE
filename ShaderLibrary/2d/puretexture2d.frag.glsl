
#define HAS_TEXCOORD
#include "common/varying_fs.glsl"
#ifdef HAS_MI
#define MATERIAL_INSTANCE_ID_OVERRIDE fragMaterialInstanceID
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
#endif

layout(location=0) out vec4 FragColor;

void main()
{    FragColor = GetSamplerBaseColor(fragUV0);
}
