// Normal Map
// Calculate normal from normal map texture
// Interface: GetNormal()
// Requires: Input.Normal, Input.Tangent, Input.TexCoord, normalMap
// Dependencies: none

vec3 GetNormal()
{
    vec3 tangentNormal = texture(normalMap, TexCoord).xyz * 2.0 - 1.0;
    
    vec3 N = normalize(Normal);
    vec3 T = normalize(Tangent.xyz);
    vec3 B = cross(N, T) * Tangent.w;
    mat3 TBN = mat3(T, B, N);
    
    return normalize(TBN * tangentNormal);
}
