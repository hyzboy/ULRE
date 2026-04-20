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
    vec3 t;
    vec3 b;

#ifdef HAS_WORLD_TANGENT
    t = normalize(tangent_ws.xyz - n * dot(n, tangent_ws.xyz));
    b = ULRE_BuildBitangent(n, t, tangent_ws.w);
#else
    vec3 dp1 = dFdx(world_pos);
    vec3 dp2 = dFdy(world_pos);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    if (abs(det) <= 1e-8)
        return n;

    float inv_det = 1.0 / det;
    t = normalize((dp1 * duv2.y - dp2 * duv1.y) * inv_det);
    b = normalize((dp2 * duv1.x - dp1 * duv2.x) * inv_det);
#endif

    mat3 tbn = mat3(t, b, n);
    return normalize(tbn * normal_ts);
}

#endif