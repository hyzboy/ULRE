// Blinn-Phong Lighting Model
// Classic diffuse lighting with improved specular calculation
// Interface: GetDiffuseColor()
// Requires: Input.Normal, Input.Position, sky, camera
// Dependencies: GetAlbedo

vec3 GetDiffuseColor()
{
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(sky.sun_direction.xyz);
    
    float NdotL = max(dot(normal, lightDir), 0.0);
    
    vec3 baseColor = GetAlbedo();
    vec3 lightColor = sky.sun_color.rgb * sky.sun_intensity;
    
    return baseColor * NdotL * lightColor;
}
