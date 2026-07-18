// === Surface Function: Billboard Texture ===
// Billboard 共用 — Bindless 采样 TextureBaseColor，无光照
// Dynamic/Fixed 共用此 surface function

#include "common/descriptor_macros.glsl"
#include "common/instance_rows_ssbo.glsl"
TEXTURE_LAYER_ROWS_SSBO;
#include "common/bindless_textures.glsl"

SurfaceOutput EvalSurface(SurfaceInput si, uint materialInstanceID)
{
    const uint iid = si.textureLayerID;
    vec4 texColor = SAMPLE_BINDLESS_SLOT_2D(iid, TEXTURE_SLOT_BASE_COLOR, si.uv0);

    SurfaceOutput so;
    so.baseColor = texColor.rgb;
    so.alpha     = texColor.a;
    so.normal    = si.worldNormal;
    so.metallic  = 0.0;
    so.roughness = 1.0;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    return so;
}

float EvalAlpha(SurfaceInput si, uint materialInstanceID)
{
    return SAMPLE_BINDLESS_SLOT_2D(si.textureLayerID, TEXTURE_SLOT_BASE_COLOR, si.uv0).a;
}
