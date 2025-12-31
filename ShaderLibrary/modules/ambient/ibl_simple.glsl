// Simple IBL Ambient Lighting
// Basic Image-Based Lighting using environment map
// Interface: GetAmbientColor()
// Requires: Input.Normal, envMap
// Dependencies: GetAlbedo

vec3 GetAmbientColor()
{
    vec3 normal = normalize(Normal);
    vec3 baseColor = GetAlbedo();
    
    // Sample environment map
    vec3 envColor = texture(envMap, normal).rgb;
    
    return baseColor * envColor * 0.5;
}
