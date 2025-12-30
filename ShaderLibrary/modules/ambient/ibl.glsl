// Full IBL Ambient Lighting
// Advanced Image-Based Lighting with diffuse and specular convolution
// Interface: GetAmbientColor()
// Requires: Input.Normal, Input.Position, camera, irradianceMap, prefilterMap, brdfLUT
// Dependencies: GetAlbedo, GetRoughness, GetMetallic, fresnelSchlickRoughness

vec3 GetAmbientColor()
{
    vec3 normal = normalize(Normal);
    vec3 viewDir = normalize(camera.pos - Position.xyz);
    vec3 reflectDir = reflect(-viewDir, normal);
    
    vec3 albedo = GetAlbedo();
    float roughness = GetRoughness();
    float metallic = GetMetallic();
    
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlickRoughness(max(dot(normal, viewDir), 0.0), F0, roughness);
    
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    
    // Diffuse IBL
    vec3 irradiance = texture(irradianceMap, normal).rgb;
    vec3 diffuse = kD * albedo * irradiance;
    
    // Specular IBL
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, reflectDir, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = texture(brdfLUT, vec2(max(dot(normal, viewDir), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    
    return diffuse + specular;
}
