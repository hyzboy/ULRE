#ifndef GET_NORMAL_TANGNET_FROM_NORMAL_ONLY_GLSL
#define GET_NORMAL_TANGNET_FROM_NORMAL_ONLY_GLSL

// Build an orthonormal tangent from normal only.
// This path is robust for assets that do not provide tangent attributes.

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
    outTangent = _ULRE_BuildTangentFromNormal(outNormal);
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