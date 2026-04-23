// util/color_space.glsl — Linear ↔ sRGB conversion and luminance helpers
//
// No UBO/SSBO dependencies.
// ToneMap files that call linearTosRGB must #include this first.

#ifndef ULRE_UTIL_COLOR_SPACE_GLSL
#define ULRE_UTIL_COLOR_SPACE_GLSL

// Linear HDR → gamma-corrected sRGB (IEC 61966-2-1)
vec3 linearTosRGB(vec3 c)
{
    vec3 lo = 12.92 * c;
    vec3 hi = 1.055 * pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.4)) - 0.055;
    return mix(hi, lo, step(c, vec3(0.0031308)));
}

// sRGB → linear (inverse of above)
vec3 sRGBToLinear(vec3 c)
{
    vec3 lo = c / 12.92;
    vec3 hi = pow((c + 0.055) / 1.055, vec3(2.4));
    return mix(hi, lo, step(c, vec3(0.04045)));
}

// Perceptual luminance (BT.709)
float Luminance(vec3 c)
{
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

#endif // ULRE_UTIL_COLOR_SPACE_GLSL
