// common/vertex_fetch_ssbo.glsl
//
// Provides FetchPosition / FetchNormal / FetchTangent / FetchUV0 functions
// for the GEOMETRY_FETCH_SSBO vertex-pulling path.
//
// Included by compositor/vert_forward_main.glsl when GEOMETRY_FETCH_SSBO is
// defined.  The CompositorAssembler emits per-attribute binding macros BEFORE
// including this file:
//
//   Position from SSBO:
//     POSITION_SSBO_SET      — Vulkan descriptor set index  (= 4)
//     POSITION_SSBO_BINDING  — binding index within that set (= AttributeSemantic::BuiltinCount = 8)
//
//   Per-semantic attribute SSBOs (binding = AttributeSemantic ordinal):
//     FETCH_NORMAL_SSBO_BINDING           (binding = 0)
//     FETCH_TANGENT_SSBO_BINDING          (binding = 1)
//     FETCH_COLOR_SSBO_BINDING            (binding = 2)
//     FETCH_TEXCOORD0_SSBO_BINDING        (binding = 3)
//     FETCH_TEXCOORD1_SSBO_BINDING        (binding = 4)
//
// Any macro not emitted → the corresponding Fetch*() returns a safe default
// (position delegates to GetPositionLocal() which reads from the VBO).

#ifndef ULRE_COMMON_VERTEX_FETCH_SSBO_GLSL
#define ULRE_COMMON_VERTEX_FETCH_SSBO_GLSL

#ifndef VERTEXSTREAMS_SET
#define VERTEXSTREAMS_SET 4
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Position
// ─────────────────────────────────────────────────────────────────────────────
#ifdef POSITION_SSBO_BINDING
    // Full SSBO pulling: position is in a packed vec3 storage buffer.
    // ssbo_packed.glsl uses POSITION_SSBO_SET / POSITION_SSBO_BINDING macros
    // and defines u_PositionData + GetPositionLocal().
    #include "position_provider/ssbo_packed.glsl"
    vec3 FetchPosition(uint i) { return u_PositionData.positions[i]; }
#else
    // Hybrid mode: position stays in the vertex buffer (inPosition declared by
    // vertex_input_position.glsl); SSBO pulling is used only for other attributes.
    vec3 FetchPosition(uint i) { return GetPositionLocal(); }
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Normal  (AttributeSemantic::Normal = 0)
// ─────────────────────────────────────────────────────────────────────────────
#ifdef FETCH_NORMAL_SSBO_BINDING
    #define ATTRIB_SET     VERTEXSTREAMS_SET
    #define ATTRIB_BINDING FETCH_NORMAL_SSBO_BINDING
    #define ATTRIB_TAG     Normal
    #include "attribute_provider/ssbo_vec3.glsl"
    #undef ATTRIB_TAG
    #undef ATTRIB_BINDING
    #undef ATTRIB_SET
    vec3 FetchNormal(uint i) { return ReadAttrib_Normal(i); }
#else
    vec3 FetchNormal(uint i) { return vec3(0.0, 1.0, 0.0); }
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Tangent  (AttributeSemantic::Tangent = 1)
// ─────────────────────────────────────────────────────────────────────────────
#ifdef FETCH_TANGENT_SSBO_BINDING
    #define ATTRIB_SET     VERTEXSTREAMS_SET
    #define ATTRIB_BINDING FETCH_TANGENT_SSBO_BINDING
    #define ATTRIB_TAG     Tangent
    #include "attribute_provider/ssbo_vec3.glsl"
    #undef ATTRIB_TAG
    #undef ATTRIB_BINDING
    #undef ATTRIB_SET
    vec3 FetchTangent(uint i) { return ReadAttrib_Tangent(i); }
#else
    vec3 FetchTangent(uint i) { return vec3(1.0, 0.0, 0.0); }
#endif

// ─────────────────────────────────────────────────────────────────────────────
// TexCoord0  (AttributeSemantic::TexCoord0 = 3)
// ─────────────────────────────────────────────────────────────────────────────
#ifdef FETCH_TEXCOORD0_SSBO_BINDING
    #define ATTRIB_SET     VERTEXSTREAMS_SET
    #define ATTRIB_BINDING FETCH_TEXCOORD0_SSBO_BINDING
    #define ATTRIB_TAG     TexCoord0
    #include "attribute_provider/ssbo_vec2.glsl"
    #undef ATTRIB_TAG
    #undef ATTRIB_BINDING
    #undef ATTRIB_SET
    vec2 FetchUV0(uint i) { return ReadAttrib_TexCoord0(i); }
#else
    vec2 FetchUV0(uint i) { return vec2(0.0); }
#endif

// ─────────────────────────────────────────────────────────────────────────────
// TexCoord1  (AttributeSemantic::TexCoord1 = 4)
// ─────────────────────────────────────────────────────────────────────────────
#ifdef FETCH_TEXCOORD1_SSBO_BINDING
    #define ATTRIB_SET     VERTEXSTREAMS_SET
    #define ATTRIB_BINDING FETCH_TEXCOORD1_SSBO_BINDING
    #define ATTRIB_TAG     TexCoord1
    #include "attribute_provider/ssbo_vec2.glsl"
    #undef ATTRIB_TAG
    #undef ATTRIB_BINDING
    #undef ATTRIB_SET
    vec2 FetchUV1(uint i) { return ReadAttrib_TexCoord1(i); }
#else
    vec2 FetchUV1(uint i) { return vec2(0.0); }
#endif

#endif // ULRE_COMMON_VERTEX_FETCH_SSBO_GLSL
