#ifndef GET_NORMAL_TANGNET_COMPRESSED_GLSL
#define GET_NORMAL_TANGNET_COMPRESSED_GLSL

// Compressed-friendly path: consume tangent xyz(+sign in w) if provided,
// otherwise fall back to a normal-only orthonormal basis.

vec3 _ULRE_SelectFallbackAxis(vec3 n)
{
    return (abs(n.z) < 0.999) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
}

vec3 _ULRE_BuildTangentFromNormal(vec3 n)
{
    vec3 axis = _ULRE_SelectFallbackAxis(n);
    vec3 bitangent = normalize(cross(axis, n));
    return normalize(cross(n, bitangent));
}

void GetNormalTangnet(in vec3 worldPos,
                      in vec2 uv,
                      in vec3 worldNormal,
                      out vec3 outNormal,
                      out vec3 outTangent)
{
    outNormal = normalize(worldNormal);

#ifdef HAS_WORLD_TANGENT
    vec3 t = normalize(fragWorldTangent.xyz);
    t = normalize(t - outNormal * dot(outNormal, t));

    if (dot(t, t) <= 1e-8)
        t = _ULRE_BuildTangentFromNormal(outNormal);

    // Preserve tangent handedness when packed sign is provided in .w.
    if (fragWorldTangent.w < 0.0)
        t = -t;

    outTangent = t;
#else
    outTangent = _ULRE_BuildTangentFromNormal(outNormal);
#endif
}

void GetNormalTangent(in vec3 worldPos,
                      in vec2 uv,
                      in vec3 worldNormal,
                      out vec3 outNormal,
                      out vec3 outTangent)
{
    GetNormalTangnet(worldPos, uv, worldNormal, outNormal, outTangent);
}

#endif