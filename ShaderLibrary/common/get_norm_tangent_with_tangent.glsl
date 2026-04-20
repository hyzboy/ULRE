#ifndef ULRE_GET_NORM_TANGENT_WITH_TANGENT_GLSL
#define ULRE_GET_NORM_TANGENT_WITH_TANGENT_GLSL

#include "common/surface_normal.glsl"

vec3 ULRE_GetGeometryNormal(SurfaceInput si)
{
    return normalize(si.worldNormal);
}

vec3 ULRE_GetNormalFromTS(SurfaceInput si, vec3 normal_ts)
{
    return ULRE_ApplyNormalMap(si.worldPos,
                               si.uv0,
                               si.worldNormal,
                               si.worldTangent,
                               normal_ts);
}

#endif
