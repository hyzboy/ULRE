#ifndef ULRE_SURFACE_NORMAL_GLSL
#define ULRE_SURFACE_NORMAL_GLSL

bool ULRE_HasValidWorldTangent(vec4 tangent_ws)
{
    return dot(tangent_ws.xyz, tangent_ws.xyz) > 1e-8;
}

vec3 ULRE_BuildBitangent(vec3 n, vec3 t, float tangent_w)
{
    float handedness = (tangent_w >= 0.0) ? 1.0 : -1.0;
    return normalize(cross(n, t)) * handedness;
}

vec3 ULRE_ApplyNormalMap(vec3 world_pos, vec2 uv, vec3 n_geom, vec4 tangent_ws, vec3 normal_ts)
{
    vec3 n = normalize(n_geom);

#ifdef HAS_WORLD_TANGENT
    vec3 t = normalize(tangent_ws.xyz - n * dot(n, tangent_ws.xyz));
    vec3 b = ULRE_BuildBitangent(n, t, tangent_ws.w);
    return normalize(mat3(t, b, n) * normal_ts);
#else
    // No derivative fallback: no-tangent materials must route to dedicated
    // *_no_tangent_surface variants that use geometric normals directly.
    return n;
#endif
}

#endif