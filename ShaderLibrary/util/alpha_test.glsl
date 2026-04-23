// util/alpha_test.glsl — Alpha discard helpers for ALPHA_MODE_MASKED / DITHER
//
// Depends on: util/dither.glsl (for DitherDiscard)
// Usage (in frag_forward_main or any fragment shader):
//   #ifdef ALPHA_MODE_MASKED
//   #include "util/alpha_test.glsl"
//   AlphaTestDiscard(so.alpha);
//   #endif
//
//   #ifdef ALPHA_MODE_DITHER
//   #include "util/alpha_test.glsl"
//   DitherDiscard(so.alpha, gl_FragCoord.xy);
//   #endif

#ifndef ULRE_UTIL_ALPHA_TEST_GLSL
#define ULRE_UTIL_ALPHA_TEST_GLSL

// Masked: hard clip at ALPHA_THRESHOLD (default 0.5, caller may override).
#ifndef ALPHA_THRESHOLD
#define ALPHA_THRESHOLD 0.5
#endif

void AlphaTestDiscard(float alpha)
{
    if (alpha < ALPHA_THRESHOLD) discard;
}

// Ordered dither: discard based on Bayer 4×4 threshold.
#include "util/dither.glsl"

void DitherDiscard(float alpha, vec2 screenPos)
{
    if (alpha < BayerDither4x4(ivec2(screenPos))) discard;
}

#endif // ULRE_UTIL_ALPHA_TEST_GLSL
