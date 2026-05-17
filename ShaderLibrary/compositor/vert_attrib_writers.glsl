// compositor/vert_attrib_writers.glsl
//
// Write per-vertex varyings from the resolved world-space data and raw VBO inputs.
//
// This file contains ONLY varying assignment — no position transform, no matrix math.
// It is included AFTER ApplyVertexTransform() has run and set `worldPos`.
//
// Context requirements (guaranteed by CompositorAssembler emit order):
//   - vec4 worldPos          – computed by ApplyVertexTransform() in the policy include
//   - HAS_* defines          – set by CompositorAssembler based on attrib feature bits
//   - GEOMETRY_FETCH_SSBO    – defined when geometry comes from SSBO instead of VBO
//   - varying out-vars from common/varying_vs.glsl already declared
//   - mat4 transform_mat     – must be declared by callers who need normal/tangent transforms
//
// NOTE: This file intentionally does NOT declare transform_mat itself. For policies that
// produce a meaningful world transform (e.g. Mesh3D), the policy file provides it via
// a local variable; this writer receives it as an ambient variable in GLSL scope.

#ifndef ULRE_COMPOSITOR_VERT_ATTRIB_WRITERS_GLSL
#define ULRE_COMPOSITOR_VERT_ATTRIB_WRITERS_GLSL

    // --- World position varying ---
#ifdef HAS_POSITION
    fragWorldPos = worldPos.xyz;
#endif

    // --- Normal ---
#if defined(HAS_NORMAL) && defined(POLICY_HAS_TRANSFORM_MAT)
  #if GEOMETRY_FETCH_SSBO
    vec3 _rawNormal = FetchNormal(gl_VertexIndex);
  #else
    vec3 _rawNormal = inNormal;
  #endif
    fragWorldNormal = normalize(mat3(transform_mat) * _rawNormal);
#endif

    // --- Tangent (xyz world-space + w handedness) ---
#if defined(HAS_TANGENT) && defined(POLICY_HAS_TRANSFORM_MAT)
  #if GEOMETRY_FETCH_SSBO
    vec3  _rawTangent = vec3(1.0, 0.0, 0.0);
    float _tangentW   = 1.0;
  #elif defined(TANGENT_LOCATION)
    vec3  _rawTangent = inTangent.xyz;
    float _tangentW   = inTangent.w;
  #else
    vec3  _rawTangent = vec3(1.0, 0.0, 0.0);
    float _tangentW   = 1.0;
  #endif
    fragWorldTangent = vec4(normalize(mat3(transform_mat) * _rawTangent), _tangentW);
#endif

    // --- UV0 ---
#ifdef HAS_TEXCOORD
  #if GEOMETRY_FETCH_SSBO
    fragUV0 = FetchUV0(gl_VertexIndex);
  #else
    fragUV0 = inUV0;
  #endif
#endif

    // --- Vertex color ---
#ifdef HAS_COLOR
    fragVertexColor = inColor;
#endif

    // --- Luminance ---
#ifdef HAS_LUMINANCE
    fragLuminance = inLuminance;
#endif

    // --- Direction (sky / atmosphere) ---
#ifdef HAS_DIRECTION
    fragDirection = normalize(worldPos.xyz);
#endif

#endif // ULRE_COMPOSITOR_VERT_ATTRIB_WRITERS_GLSL
