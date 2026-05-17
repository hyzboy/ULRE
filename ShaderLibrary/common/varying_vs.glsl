// common/varying_vs.glsl — Vertex shader output varyings (stage = out)
//
// *** AUTHORITATIVE LAYOUT: inc/hgl/shadergen/InterstageVaryingLayout.h ***
// Location numbers here must stay in sync with kVaryingTable in
// src/ShaderGen/InterstageVaryingLayout.cpp.
//
// Include in vertex shaders AFTER defining HAS_* attrib macros.
//
// Usage:
//   #define HAS_POSITION
//   #define HAS_NORMAL
//   #include "common/varying_vs.glsl"

#ifndef ULRE_COMMON_VARYING_VS_GLSL
#define ULRE_COMMON_VARYING_VS_GLSL

// location 0: Material Instance ID (always present)
layout(location=0) flat out uint fragMaterialInstanceID;

// location 1: World Position
#ifdef HAS_POSITION
layout(location=1) out vec3 fragWorldPos;
#endif

// location 2: World Normal
#ifdef HAS_NORMAL
layout(location=2) out vec3 fragWorldNormal;
#endif

// location 3: UV0
#ifdef HAS_TEXCOORD
layout(location=3) out vec2 fragUV0;
#endif

// location 4: Vertex Color
#ifdef HAS_COLOR
layout(location=4) out vec4 fragVertexColor;
#endif

// location 6: Direction (sky / atmosphere)
#ifdef HAS_DIRECTION
layout(location=6) out vec3 fragDirection;
#endif

// location 7: Luminance
#ifdef HAS_LUMINANCE
layout(location=7) out float fragLuminance;
#endif

// location 8: Clip Position (terrain edge-fade)
#ifdef HAS_CLIP_POS
layout(location=8) out vec4 fragClipPos;
#endif

// location 9: World Tangent (xyz + handedness w)
#ifdef HAS_TANGENT
layout(location=9) out vec4 fragWorldTangent;
#endif

#endif // ULRE_COMMON_VARYING_VS_GLSL
