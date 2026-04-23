// util/normal_mapping.glsl — Tangent-space normal map helper
//
// No UBO/SSBO dependencies.
// Requires: worldPos, uv, geometric normal, tangent (world-space, w = handedness),
//           and the sampled normal in tangent-space (range [0,1] from texture).

#ifndef ULRE_UTIL_NORMAL_MAPPING_GLSL
#define ULRE_UTIL_NORMAL_MAPPING_GLSL

// Transform a tangent-space normal into world space via TBN.
// n_geom    : geometric world-space normal (need not be normalized)
// tangent_ws: world-space tangent, .w = handedness sign (+1/-1)
// normal_ts : tangent-space normal, already decoded from [0,1] to [-1,1]
vec3 PBR_ApplyNormalMapTBN(vec3 n_geom, vec4 tangent_ws, vec3 normal_ts)
{
    vec3 n = normalize(n_geom);
    vec3 t = normalize(tangent_ws.xyz - n * dot(n, tangent_ws.xyz));
    float handedness = (tangent_ws.w >= 0.0) ? 1.0 : -1.0;
    vec3 b = normalize(cross(n, t)) * handedness;
    return normalize(mat3(t, b, n) * normal_ts);
}

#endif // ULRE_UTIL_NORMAL_MAPPING_GLSL
