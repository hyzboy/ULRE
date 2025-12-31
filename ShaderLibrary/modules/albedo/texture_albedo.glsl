// Texture Albedo
// Get base color from texture map
// Interface: GetAlbedo()
// Requires: Input.TexCoord, albedoMap
// Dependencies: none

vec3 GetAlbedo()
{
    return texture(albedoMap, TexCoord).rgb;
}
