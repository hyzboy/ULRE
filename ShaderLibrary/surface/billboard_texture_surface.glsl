// === Surface Function: Billboard Texture ===
// Billboard 共用 — 采样 TextureBaseColor，无光照
// Dynamic/Fixed 共用此 surface function

layout(set=MATERIAL_SET, binding=TEXTUREBASECOLOR_BINDING) uniform sampler2D TextureBaseColor;

SurfaceOutput EvalSurface(SurfaceInput si, uint materialInstanceID)
{
    vec4 texColor = texture(TextureBaseColor, si.uv0);

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
    return texture(TextureBaseColor, si.uv0).a;
}
