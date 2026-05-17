#ifndef VARYING_INTERFACE_GLSL
#define VARYING_INTERFACE_GLSL

// Unified inter-stage varying declarations.
//
// Usage (vertex shader):
//   #define VARYING_STAGE_VERT
//   #define HAS_POSITION
//   #define HAS_NORMAL
//   #define HAS_TEXCOORD
//   #include "common/varying_interface.glsl"
//
// Usage (fragment shader):
//   #define HAS_POSITION
//   #define HAS_NORMAL
//   #define HAS_TEXCOORD
//   #include "common/varying_interface.glsl"
//   // MATERIAL_INSTANCE_ID_OVERRIDE is auto-defined in fragment stage

#ifdef VARYING_STAGE_VERT
  #define _VARYING_DIR out
#else
  #define _VARYING_DIR in
#endif

// --- location 0: Material Instance ID (always present) ---
layout(location=0) flat _VARYING_DIR uint fragMaterialInstanceID;

// --- location 1: World Position ---
#ifdef HAS_POSITION
layout(location=1) _VARYING_DIR vec3 fragWorldPos;
#endif

// --- location 2: World Normal ---
#ifdef HAS_NORMAL
layout(location=2) _VARYING_DIR vec3 fragWorldNormal;
#endif

// --- location 3: UV0 ---
#ifdef HAS_TEXCOORD
layout(location=3) _VARYING_DIR vec2 fragUV0;
#endif

// --- location 4: Vertex Color ---
#ifdef HAS_COLOR
layout(location=4) _VARYING_DIR vec4 fragVertexColor;
#endif

// --- location 6: Direction (sky) ---
#ifdef HAS_DIRECTION
layout(location=6) _VARYING_DIR vec3 fragDirection;
#endif

// --- location 7: Luminance ---
#ifdef HAS_LUMINANCE
layout(location=7) _VARYING_DIR float fragLuminance;
#endif

// --- location 8: Clip Position (terrain) ---
#ifdef HAS_CLIP_POS
layout(location=8) _VARYING_DIR vec4 fragClipPos;
#endif

// --- location 9: World Tangent (xyz + handedness w) ---
#ifdef HAS_TANGENT
layout(location=9) _VARYING_DIR vec4 fragWorldTangent;
#endif

#undef _VARYING_DIR

// In fragment stage, auto-define the MI ID override so that
// ssbo_material_instance.glsl uses the flat varying instead of gl_InstanceIndex.
#ifndef VARYING_STAGE_VERT
  #ifndef MATERIAL_INSTANCE_ID_OVERRIDE
    #define MATERIAL_INSTANCE_ID_OVERRIDE fragMaterialInstanceID
  #endif
#endif

#endif
