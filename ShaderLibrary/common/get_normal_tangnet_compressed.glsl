#ifndef GET_NORMAL_TANGNET_COMPRESSED_GLSL
#define GET_NORMAL_TANGNET_COMPRESSED_GLSL

// Compressed-friendly path with explicit decode helpers.
// The decode names are intentionally stable for CPU/Shader consistency checks:
// - decode_normal_laea_u8x2
// - decode_tangent_laea_u8x2
// - decode_uv_half2

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

// Simple RG8SN decode: same as normal maps.
// Input is signed [-1, 1], recover z from sqrt(1 - x² - y²).
vec3 decode_normal_laea_u8x2(vec2 enc_signed)
{
    vec3 n;
    n.xy = enc_signed;  // Already [-1, 1] from signed unorm
    n.z = sqrt(max(0.0, 1.0 - dot(n.xy, n.xy)));
    return normalize(n);
}

vec3 decode_tangent_laea_u8x2(vec2 enc_signed)
{
    return decode_normal_laea_u8x2(enc_signed);
}

// Placeholder for half2 decode path. With current GLSL input path uv is already float2.
vec2 decode_uv_half2(vec2 packedUV)
{
    return packedUV;
}

void GetNormalTangnet(in vec3 worldPos,
                      in vec2 uv,
                      in vec3 worldNormal,
                      out vec3 outNormal,
                      out vec3 outTangent)
{
    vec2 _decodedUV = decode_uv_half2(uv);

#if defined(ULRE_NT_COMPRESSED_LAEA_RG2)
    // RG8SN comes as [-1, 1] directly from Vulkan format conversion
    outNormal = decode_normal_laea_u8x2(worldNormal.xy);
#else
    outNormal = normalize(worldNormal);
#endif

#ifdef HAS_WORLD_TANGENT
    vec3 t;

#if defined(ULRE_NT_COMPRESSED_LAEA_RG2)
    // Same for tangent: RG8SN -> [-1, 1] auto-converted
    t = decode_tangent_laea_u8x2(fragWorldTangent.xy);
#else
    t = normalize(fragWorldTangent.xyz);
#endif

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

    // Keep the variable alive for compile paths where optimizers are strict about
    // helper visibility during staged migration.
    outTangent += vec3(0.0) * vec3(_decodedUV, 0.0);
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