// Spherical Harmonics Ambient Lighting
// Efficient ambient lighting using spherical harmonics coefficients
// Interface: GetAmbientColor()
// Requires: Input.Normal, shCoefficients
// Dependencies: GetAlbedo

vec3 GetAmbientColor()
{
    vec3 normal = normalize(Normal);
    vec3 baseColor = GetAlbedo();
    
    // Evaluate SH (simplified for L2)
    vec3 result = shCoefficients[0];
    result += shCoefficients[1] * normal.y;
    result += shCoefficients[2] * normal.z;
    result += shCoefficients[3] * normal.x;
    
    return baseColor * max(result, vec3(0.0));
}
