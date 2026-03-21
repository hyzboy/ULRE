
layout(location=0) in vec2 fragTexCoord;

#ifdef HAS_MI
layout(location=1) flat in uint fragMaterialInstanceID;
#define MATERIAL_INSTANCE_ID_OVERRIDE fragMaterialInstanceID
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
#endif

#if defined(TEXTURE_ARRAY_MODE) || defined(TEX_BASECOLOR_ARRAY)
#include "common/ssbo_material_instance_texture.glsl"
#endif

layout(location=0) out vec4 FragColor;

void main()
{
#if defined(TEXTURE_ARRAY_MODE) || defined(TEX_BASECOLOR_ARRAY)
    _ULRE_InitTextureLayerIndices();
#endif
    FragColor = GetSamplerBaseColor(fragTexCoord);
}
