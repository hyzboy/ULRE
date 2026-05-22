#ifndef ULRE_SURFACE_UNLIT_TEXTURE3D_SURFACE_GLSL
#define ULRE_SURFACE_UNLIT_TEXTURE3D_SURFACE_GLSL

// @sfm:surface_type    Unlit
// @sfm:supports_phase  ForwardOpaque ForwardMasked ForwardTransparent
// @sfm:require va      TexCoord
// @sfm:require tex     BaseColor
// @sfm:require ubo     camera
// @sfm:require sky     false

SurfaceOutput EvalSurface
{
    vec4 texColor = GetSamplerBaseColor(GetMaterialInstanceID(), si.uv0);

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

float EvalAlpha(SurfaceInput si)
{
    return GetSamplerBaseColor(GetMaterialInstanceID(), si.uv0).a;
}

#endif // ULRE_SURFACE_UNLIT_TEXTURE3D_SURFACE_GLSL
