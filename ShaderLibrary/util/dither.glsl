// util/dither.glsl — Ordered dither helpers
//
// No UBO/SSBO dependencies.  Include anywhere a dither threshold is needed.
// Usage:
//   float t = BayerDither4x4(ivec2(gl_FragCoord.xy));
//   if (alpha < t) discard;

#ifndef ULRE_UTIL_DITHER_GLSL
#define ULRE_UTIL_DITHER_GLSL

// Bayer 4×4 ordered dither matrix, returns threshold in [0, 1).
float BayerDither4x4(ivec2 p)
{
    const float bayer[16] = float[16](
         0.0/16.0,  8.0/16.0,  2.0/16.0, 10.0/16.0,
        12.0/16.0,  4.0/16.0, 14.0/16.0,  6.0/16.0,
         3.0/16.0, 11.0/16.0,  1.0/16.0,  9.0/16.0,
        15.0/16.0,  7.0/16.0, 13.0/16.0,  5.0/16.0
    );
    return bayer[(p.y & 3) * 4 + (p.x & 3)];
}

#endif // ULRE_UTIL_DITHER_GLSL
