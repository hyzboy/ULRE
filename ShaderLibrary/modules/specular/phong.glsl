// Phong Specular Model
// Classic specular highlights using reflection vector
// Interface: GetSpecularColor()
// Requires: Input.Normal, Input.Position, sky, camera
// Dependencies: none

vec3 GetSpecularColor()
{
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(sky.sun_direction.xyz);
    vec3 viewDir = normalize(camera.pos - Position.xyz);
    vec3 reflectDir = reflect(-lightDir, normal);
    
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = vec3(0.5) * spec;
    
    vec3 lightColor = sky.sun_color.rgb * sky.sun_intensity;
    
    return specular * lightColor;
}
