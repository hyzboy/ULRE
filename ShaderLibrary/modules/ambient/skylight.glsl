// Skylight Ambient Lighting
// Simple ambient lighting from sky color
// Interface: GetAmbientColor()
// Requires: sky
// Dependencies: GetAlbedo

vec3 GetAmbientColor()
{
    vec3 baseColor = GetAlbedo();
    vec3 skyColor = sky.base_sky_color.rgb;
    
    return baseColor * skyColor * 0.3;
}
