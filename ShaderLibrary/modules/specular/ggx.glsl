// GGX Specular Model
// Physically-based specular using GGX/Trowbridge-Reitz distribution
// Interface: GetSpecularColor()
// Requires: Input.Normal, Input.Position, sky, camera
// Dependencies: GetRoughness, GetMetallic, DistributionGGX, GeometrySmith, fresnelSchlick

vec3 GetSpecularColor()
{
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(sky.sun_direction.xyz);
    vec3 viewDir = normalize(camera.pos - Position.xyz);
    vec3 halfDir = normalize(lightDir + viewDir);
    
    float roughness = GetRoughness();
    float metallic = GetMetallic();
    
    float NdotL = max(dot(normal, lightDir), 0.0);
    float NdotV = max(dot(normal, viewDir), 0.0);
    float NdotH = max(dot(normal, halfDir), 0.0);
    float VdotH = max(dot(viewDir, halfDir), 0.0);
    
    // Calculate F0 based on metallic
    vec3 F0 = mix(vec3(0.04), GetAlbedo(), metallic);
    
    // Cook-Torrance BRDF
    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    vec3 F = fresnelSchlick(VdotH, F0);
    
    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.001;
    vec3 specular = numerator / denominator;
    
    vec3 lightColor = sky.sun_color.rgb * sky.sun_intensity;
    
    return specular * NdotL * lightColor;
}
