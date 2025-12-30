// PBR Standard Lighting Model
// Physically-Based Rendering with energy conservation
// Interface: GetDiffuseColor()
// Requires: Input.Normal, Input.Position, sky, camera
// Dependencies: GetAlbedo, GetRoughness, GetMetallic, fresnelSchlick, DistributionGGX

vec3 GetDiffuseColor()
{
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(sky.sun_direction.xyz);
    vec3 viewDir = normalize(camera.pos - Position.xyz);
    vec3 halfDir = normalize(lightDir + viewDir);
    
    float NdotL = max(dot(normal, lightDir), 0.0);
    float NdotV = max(dot(normal, viewDir), 0.0);
    float NdotH = max(dot(normal, halfDir), 0.0);
    
    vec3 albedo = GetAlbedo();
    float roughness = GetRoughness();
    float metallic = GetMetallic();
    
    // Calculate F0 for dielectrics and metals
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(NdotH, roughness);
    vec3 F = fresnelSchlick(max(dot(halfDir, viewDir), 0.0), F0);
    
    vec3 kD = vec3(1.0) - F;
    kD *= 1.0 - metallic;
    
    vec3 diffuse = kD * albedo / 3.14159265359;
    
    vec3 lightColor = sky.sun_color.rgb * sky.sun_intensity;
    
    return diffuse * NdotL * lightColor;
}
