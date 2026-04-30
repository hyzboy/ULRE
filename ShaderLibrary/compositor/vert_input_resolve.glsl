// compositor/vert_input_resolve.glsl — Fetch raw vertex data into local variables.
//
// Context requirements:
//   - common/vertex_input_position.glsl (GetPositionLocal()) already in scope.
//   - common/ssbo_transform.glsl (GetTransform()) must be in scope.
//   - GEOMETRY_FETCH_SSBO / HAS_* defines set by CompositorAssembler.
//
// Provides after include:
//   mat4  transform_mat   — LocalToWorld matrix
//   vec3  pos3            — object-space position (always vec3)
//   vec4  worldPos        — homogeneous world position

#ifndef ULRE_COMPOSITOR_VERT_INPUT_RESOLVE_GLSL
#define ULRE_COMPOSITOR_VERT_INPUT_RESOLVE_GLSL

    mat4 transform_mat = GetTransform();

    // --- Position ---
#if GEOMETRY_FETCH_SSBO
    vec3 pos3 = FetchPosition(gl_VertexIndex);
#else
    vec3 pos3 = GetPositionLocal();
#endif

    vec4 worldPos = transform_mat * vec4(pos3, 1.0);

    // --- Normal ---
#ifdef HAS_NORMAL
  #if GEOMETRY_FETCH_SSBO
    vec3 _rawNormal = FetchNormal(gl_VertexIndex);
  #else
    vec3 _rawNormal = inNormal;
  #endif
    fragWorldNormal = normalize(mat3(transform_mat) * _rawNormal);
#endif

    // --- Tangent (xyz world + w handedness) ---
#ifdef HAS_TANGENT
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

    // --- World position varying + direction (sky) ---
#ifdef HAS_POSITION
    fragWorldPos = worldPos.xyz;
#endif

#ifdef HAS_DIRECTION
    fragDirection = normalize(pos3);
#endif

#endif // ULRE_COMPOSITOR_VERT_INPUT_RESOLVE_GLSL
