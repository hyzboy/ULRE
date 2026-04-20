#ifndef ULRE_GET_NORM_TANGENT_WITHOUT_TANGENT_GLSL
#define ULRE_GET_NORM_TANGENT_WITHOUT_TANGENT_GLSL

vec3 ULRE_GetGeometryNormal(SurfaceInput si)
{
    return normalize(si.worldNormal);
}

vec3 ULRE_GetNormalFromTS(SurfaceInput si, vec3 normal_ts)
{
    // Keep parameter in signature to match tangent-available implementation.
    // In no-tangent path normal_ts is intentionally ignored.
    // No tangent basis is available in this path. Keep geometric normal and let
    // material fallback / authoring decide whether normal-map contribution is used.
    return normalize(si.worldNormal);
}

#endif
