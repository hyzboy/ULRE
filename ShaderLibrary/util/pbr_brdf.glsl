// util/pbr_brdf.glsl — Physically-based BRDF micro-facet terms
//
// Canonical implementations shared by all PBR surface and lighting files.
// No UBO/SSBO dependencies.
//
// Convention:
//   roughness  = linear perceptual roughness (r)
//   alpha      = r²    (GGX roughness)
//   alpha2     = r⁴    (GGX roughness²)   ← pass to PBR_D_GGX

#ifndef ULRE_UTIL_PBR_BRDF_GLSL
#define ULRE_UTIL_PBR_BRDF_GLSL

#define PBR_PI 3.14159265359

// --- Normal Distribution Function (GGX / Trowbridge-Reitz) -----------------
// alpha2 = (roughness²)²  — pre-compute once per shading call.
float PBR_D_GGX(float NdotH, float alpha2)
{
    float d = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / max(PBR_PI * d * d, 1e-7);
}

// --- Geometry function (Smith single term) ----------------------------------
float PBR_G1_Smith(float NdotX, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / max(NdotX * (1.0 - k) + k, 1e-7);
}

// --- Geometry function (Smith combined: G = G1(V) * G1(L)) -----------------
float PBR_G_Smith(float NdotV, float NdotL, float roughness)
{
    return PBR_G1_Smith(NdotV, roughness) * PBR_G1_Smith(NdotL, roughness);
}

// --- Fresnel-Schlick approximation ------------------------------------------
vec3 PBR_F_Schlick(float VdotH, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

#endif // ULRE_UTIL_PBR_BRDF_GLSL
