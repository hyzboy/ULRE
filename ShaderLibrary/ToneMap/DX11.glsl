//--------------------------------------------------------------------------------------
// The tone mapper used in HDRToneMappingCS11
//--------------------------------------------------------------------------------------
vec3 ToneMapping(vec3 color)
{
    float  MIDDLE_GRAY = 0.72f;
    float  LUM_WHITE = 1.5f;

    // Tone mapping
    color.rgb *= MIDDLE_GRAY;
    color.rgb *= (1.0f + color/LUM_WHITE);
    color.rgb /= (1.0f + color);
    
    return color;
}
