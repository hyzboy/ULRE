// Constant Color Albedo
// Use a fixed color as base color
// Interface: GetAlbedo()
// Requires: materialInstance
// Dependencies: none

vec3 GetAlbedo()
{
    return materialInstance.baseColor.rgb;
}
