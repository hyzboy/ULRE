// Half-Lambert Lighting Model
// Improved diffuse lighting with better contrast in shadows
// Interface: GetDiffuseColor()
// Requires: Input.Normal, sky
// Dependencies: GetAlbedo

vec3 GetDiffuseColor()
{
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(sky.sun_direction.xyz);
    
    float NdotL = dot(normal, lightDir);
    // Half-Lambert: remap [-1, 1] to [0, 1]
    float halfLambert = NdotL * 0.5 + 0.5;
    
    vec3 baseColor = GetAlbedo();
    vec3 lightColor = sky.sun_color.rgb * sky.sun_intensity;
    
    return baseColor * halfLambert * lightColor;
}
