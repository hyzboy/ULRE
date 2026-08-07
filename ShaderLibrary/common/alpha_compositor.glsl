// Shared final alpha handling for compositor output.
#ifndef HGL_ALPHA_COMPOSITOR_GLSL
#define HGL_ALPHA_COMPOSITOR_GLSL

float HGLDitherThreshold(vec2 pixel)
{
    return fract(sin(dot(pixel, vec2(12.9898, 78.233))) * 43758.5453);
}

void HGLApplyAlpha(float alpha)
{
#ifdef HGL_ALPHA_TEST
    if (alpha < HGL_ALPHA_CUTOFF)
        discard;
#endif

#ifdef HGL_ALPHA_DITHER
    if (alpha < HGLDitherThreshold(gl_FragCoord.xy))
        discard;
#endif
}

vec4 HGLComposeColor(vec4 color)
{
    HGLApplyAlpha(color.a);
    return color;
}

#endif
