// --------------------------------------------------------------------------
// vert_forward_main.glsl - Unified forward vertex main template.
//
// Control defines (set before #including this file):
//
//   Varying flags (must match the fragment shader's HAS_* defines):
//     HAS_WORLD_POS      output fragWorldPos (vec3)
//     HAS_WORLD_NORMAL   input  inNormal  ? output fragWorldNormal (vec3)
//     HAS_UV0            input  inUV0     ? output fragUV0 (vec2)
//     HAS_VERTEX_COLOR   input  inColor   ? output fragVertexColor (vec4)
//     HAS_LUMINANCE      input  inLuminance ? output fragLuminance (float)
//     HAS_DIRECTION      output fragDirection = normalize(position) (sky)
//
//   Geometry source:
//     GEOMETRY_FETCH_SSBO  (injected by C++)  use vertex_fetch_ssbo.glsl
//                          instead of vertex attributes
//
//   Position mode:
//     VERT_INPUT_2D      position attribute is vec2 (padded to vec3 with z=0)
//     (default)          position attribute is vec3
// --------------------------------------------------------------------------

// --- Vertex inputs ---
#if GEOMETRY_FETCH_SSBO
    #include "common/vertex_fetch_ssbo.glsl"
#else
    #ifdef VERT_INPUT_2D
        layout(location=POSITION_LOCATION) in vec2 inPosition;
    #else
        layout(location=POSITION_LOCATION) in vec3 inPosition;
    #endif

    #ifdef HAS_WORLD_NORMAL
        layout(location=NORMAL_LOCATION) in vec3 inNormal;
    #endif

    #ifdef HAS_UV0
        layout(location=TEXCOORD_LOCATION) in vec2 inUV0;
    #endif

    #ifdef HAS_VERTEX_COLOR
        layout(location=COLOR_LOCATION) in vec4 inColor;
    #endif

    #ifdef HAS_LUMINANCE
        layout(location=LUMINANCE_LOCATION) in float inLuminance;
    #endif
#endif

// --- Varying interface ---
#define VARYING_STAGE_VERT
#include "common/varying_interface.glsl"

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    mat4 l2w_mat = GetTransform();

    // Position
#if GEOMETRY_FETCH_SSBO
    vec3 pos3 = FetchPosition(gl_VertexIndex);
#elif defined(VERT_INPUT_2D)
    vec3 pos3 = vec3(inPosition, 0.0);
#else
    vec3 pos3 = inPosition;
#endif

    vec4 worldPos = l2w_mat * vec4(pos3, 1.0);

#ifdef HAS_WORLD_POS
    fragWorldPos = worldPos.xyz;
#endif

    // Normal
#ifdef HAS_WORLD_NORMAL
  #if GEOMETRY_FETCH_SSBO
    vec3 rawNormal = FetchNormal(gl_VertexIndex);
  #else
    vec3 rawNormal = inNormal;
  #endif
    fragWorldNormal = normalize(mat3(l2w_mat) * rawNormal);
#endif

    // UV0
#ifdef HAS_UV0
  #if GEOMETRY_FETCH_SSBO
    fragUV0 = FetchUV0(gl_VertexIndex);
  #else
    fragUV0 = inUV0;
  #endif
#endif

    // Vertex color
#ifdef HAS_VERTEX_COLOR
    fragVertexColor = inColor;
#endif

    // Luminance
#ifdef HAS_LUMINANCE
    fragLuminance = inLuminance;
#endif

    // Direction (sky)
#ifdef HAS_DIRECTION
    fragDirection = normalize(pos3);
#endif

    gl_Position = camera.vp * worldPos;
}
