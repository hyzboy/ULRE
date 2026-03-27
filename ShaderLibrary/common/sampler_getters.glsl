// ---- sampler_getters.glsl ----
// Auto-generated sampler getter functions.
// Controlled by HAS_SAMPLER_xxx / SAMPLER_xxx_ARRAY / SAMPLER_xxx_GRAYSCALE defines.

#ifdef HAS_SAMPLER_BASECOLOR
vec4 GetSamplerBaseColor(vec2 uv)
{
#ifdef SAMPLER_BASECOLOR_ARRAY
  #ifdef SAMPLER_BASECOLOR_GRAYSCALE
    float _r = texture(Sampler_BaseColor, vec3(uv, float(_tex_layer_BaseColor))).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_BaseColor, vec3(uv, float(_tex_layer_BaseColor)));
  #endif
#else
  #ifdef SAMPLER_BASECOLOR_GRAYSCALE
    float _r = texture(Sampler_BaseColor, uv).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_BaseColor, uv);
  #endif
#endif
}
#endif

#ifdef HAS_SAMPLER_NORMAL
vec4 GetSamplerNormal(vec2 uv)
{
#ifdef SAMPLER_NORMAL_ARRAY
  #ifdef SAMPLER_NORMAL_GRAYSCALE
    float _r = texture(Sampler_Normal, vec3(uv, float(_tex_layer_Normal))).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_Normal, vec3(uv, float(_tex_layer_Normal)));
  #endif
#else
  #ifdef SAMPLER_NORMAL_GRAYSCALE
    float _r = texture(Sampler_Normal, uv).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_Normal, uv);
  #endif
#endif
}
#endif

#ifdef HAS_SAMPLER_TANGENT
vec4 GetSamplerTangent(vec2 uv)
{
#ifdef SAMPLER_TANGENT_ARRAY
  #ifdef SAMPLER_TANGENT_GRAYSCALE
    float _r = texture(Sampler_Tangent, vec3(uv, float(_tex_layer_Tangent))).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_Tangent, vec3(uv, float(_tex_layer_Tangent)));
  #endif
#else
  #ifdef SAMPLER_TANGENT_GRAYSCALE
    float _r = texture(Sampler_Tangent, uv).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_Tangent, uv);
  #endif
#endif
}
#endif

#ifdef HAS_SAMPLER_METALLIC
vec4 GetSamplerMetallic(vec2 uv)
{
#ifdef SAMPLER_METALLIC_ARRAY
  #ifdef SAMPLER_METALLIC_GRAYSCALE
    float _r = texture(Sampler_Metallic, vec3(uv, float(_tex_layer_Metallic))).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_Metallic, vec3(uv, float(_tex_layer_Metallic)));
  #endif
#else
  #ifdef SAMPLER_METALLIC_GRAYSCALE
    float _r = texture(Sampler_Metallic, uv).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_Metallic, uv);
  #endif
#endif
}
#endif

#ifdef HAS_SAMPLER_ROUGHNESS
vec4 GetSamplerRoughness(vec2 uv)
{
#ifdef SAMPLER_ROUGHNESS_ARRAY
  #ifdef SAMPLER_ROUGHNESS_GRAYSCALE
    float _r = texture(Sampler_Roughness, vec3(uv, float(_tex_layer_Roughness))).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_Roughness, vec3(uv, float(_tex_layer_Roughness)));
  #endif
#else
  #ifdef SAMPLER_ROUGHNESS_GRAYSCALE
    float _r = texture(Sampler_Roughness, uv).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_Roughness, uv);
  #endif
#endif
}
#endif

#ifdef HAS_SAMPLER_HEIGHT
vec4 GetSamplerHeight(vec2 uv)
{
#ifdef SAMPLER_HEIGHT_ARRAY
  #ifdef SAMPLER_HEIGHT_GRAYSCALE
    float _r = texture(Sampler_Height, vec3(uv, float(_tex_layer_Height))).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_Height, vec3(uv, float(_tex_layer_Height)));
  #endif
#else
  #ifdef SAMPLER_HEIGHT_GRAYSCALE
    float _r = texture(Sampler_Height, uv).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_Height, uv);
  #endif
#endif
}
#endif

#ifdef HAS_SAMPLER_OPACITY
vec4 GetSamplerOpacity(vec2 uv)
{
#ifdef SAMPLER_OPACITY_ARRAY
  #ifdef SAMPLER_OPACITY_GRAYSCALE
    float _r = texture(Sampler_Opacity, vec3(uv, float(_tex_layer_Opacity))).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_Opacity, vec3(uv, float(_tex_layer_Opacity)));
  #endif
#else
  #ifdef SAMPLER_OPACITY_GRAYSCALE
    float _r = texture(Sampler_Opacity, uv).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_Opacity, uv);
  #endif
#endif
}
#endif

#ifdef HAS_SAMPLER_TEXT
vec4 GetSamplerText(vec2 uv)
{
#ifdef SAMPLER_TEXT_ARRAY
  #ifdef SAMPLER_TEXT_GRAYSCALE
    float _r = texture(Sampler_Text, vec3(uv, float(_tex_layer_Text))).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_Text, vec3(uv, float(_tex_layer_Text)));
  #endif
#else
  #ifdef SAMPLER_TEXT_GRAYSCALE
    float _r = texture(Sampler_Text, uv).r;
    return vec4(_r, _r, _r, _r);
  #else
    return texture(Sampler_Text, uv);
  #endif
#endif
}
#endif
