// @ulre begin
// @ulre name checkerboard_3d_source
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require ProducedSemantic WorldPosition
// @ulre uses material_source_interface
// @ulre end

#ifndef CHECKERBOARD_3D_SOURCE_GLSL
#define CHECKERBOARD_3D_SOURCE_GLSL

#include "common/material_source_interface.glsl"

vec3 ResolveCheckerboardNormal(const SurfaceInput surfaceInput)
{
#ifdef HGL_MATERIAL_HAS_VERTEX_NORMAL
    return normalize(surfaceInput.worldNormal);
#else
    // dFdx/dFdy are the two triangle-edge derivatives in world space.
    // Their cross product is the same face normal as cross(v1-v0, v2-v0).
    vec3 faceNormal = cross(
        dFdx(surfaceInput.worldPos),
        dFdy(surfaceInput.worldPos));
    const float length_squared = dot(faceNormal, faceNormal);
    if (length_squared <= 1e-12)
        return vec3(0.0, 0.0, 1.0);
    faceNormal *= inversesqrt(length_squared);
    return gl_FrontFacing ? faceNormal : -faceNormal;
#endif
}

vec2 ResolveCheckerboardCoordinates(
    const SurfaceInput surfaceInput,
    const vec3 normal)
{
    const vec3 absolute_normal = abs(normal);
    if (absolute_normal.z >= absolute_normal.x
     && absolute_normal.z >= absolute_normal.y)
        return surfaceInput.worldPos.xy;
    if (absolute_normal.x >= absolute_normal.y)
        return surfaceInput.worldPos.zy;
    return surfaceInput.worldPos.xz;
}

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput sourceInput)
{
    const vec3 normal = ResolveCheckerboardNormal(sourceInput.surface);
    const vec2 checker_position =
        ResolveCheckerboardCoordinates(sourceInput.surface, normal);
    const ivec2 cell = ivec2(floor(checker_position / 0.5));
    const int parity = (cell.x + cell.y) & 1;
    const vec3 checkerColor = mix(
        vec3(0.0),
        vec3(0.35),
        float(parity));

    const vec3 lightDirection = normalize(vec3(0.45, 0.55, 0.70));
    const float diffuse = 0.25
        + 0.75 * max(dot(normal, lightDirection), 0.0);

    MaterialSourceOutput materialResult;
    materialResult.baseColor = checkerColor * diffuse;
    materialResult.metallic = 0.0;
    materialResult.roughness = 1.0;
    materialResult.fresnel = 0.0;
    materialResult.normalScale = 1.0;
    materialResult.ao = 1.0;
    materialResult.emissive = vec3(0.0);
    materialResult.alpha = 1.0;
    return materialResult;
}

float EvalMaterialAlpha(MaterialSourceInput sourceInput)
{
    return 1.0;
}

#endif // CHECKERBOARD_3D_SOURCE_GLSL
