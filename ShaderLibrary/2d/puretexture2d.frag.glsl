
layout(location=0) in vec2 fragTexCoord;

#ifdef HAS_MI
layout(location=1) flat in uint fragMaterialInstanceID;
#define MATERIAL_INSTANCE_ID_OVERRIDE fragMaterialInstanceID
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
#endif

layout(location=0) out vec4 FragColor;

void main()
{
#ifdef TEXTURE_ARRAY_MODE
    _ULRE_InitTextureLayerIndices(MATERIAL_INSTANCE_ID_OVERRIDE);
#endif
    FragColor = GetSamplerBaseColor(fragTexCoord);
}
