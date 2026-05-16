// common/varying_fs.glsl — Fragment shader input varyings (stage = in)
//
// *** AUTHORITATIVE LAYOUT: inc/hgl/shadergen/InterstageVaryingLayout.h ***
// Location numbers here must stay in sync with kVaryingTable in
// src/ShaderGen/InterstageVaryingLayout.cpp.
//
// Include in fragment shaders AFTER defining HAS_* attrib macros.
//
// Also auto-defines MATERIAL_INSTANCE_ID_OVERRIDE so that
// ssbo_material_instance.glsl uses the flat varying instead of gl_InstanceIndex.
//
// Usage:
//   #define HAS_POSITION
//   #define HAS_NORMAL
//   #include "common/varying_fs.glsl"

#ifndef ULRE_COMMON_VARYING_FS_GLSL
#define ULRE_COMMON_VARYING_FS_GLSL

// location 0: Material Instance ID (always present)
#ifndef ULRE_HAS_FRAG_MATERIAL_INSTANCE_ID
layout(location=0) flat in uint fragMaterialInstanceID;
#define ULRE_HAS_FRAG_MATERIAL_INSTANCE_ID
#endif

// location 1: World Position
#ifdef HAS_POSITION
layout(location=1) in vec3 fragWorldPos;
#endif

// location 2: World Normal
#ifdef HAS_NORMAL
layout(location=2) in vec3 fragWorldNormal;
#endif

// location 3: UV0
#ifdef HAS_TEXCOORD
layout(location=3) in vec2 fragUV0;
#endif

// location 4: Vertex Color
#ifdef HAS_COLOR
layout(location=4) in vec4 fragVertexColor;
#endif

// location 5: Billboard Tex Coord
#ifdef HAS_BILLBOARD_TEXCOORD
layout(location=5) in vec2 fragTexCoord;
#endif

// location 6: Direction (sky / atmosphere)
#ifdef HAS_DIRECTION
layout(location=6) in vec3 fragDirection;
#endif

// location 7: Luminance
#ifdef HAS_LUMINANCE
layout(location=7) in float fragLuminance;
#endif

// location 8: Clip Position (terrain edge-fade)
#ifdef HAS_CLIP_POS
layout(location=8) in vec4 fragClipPos;
#endif

// location 9: World Tangent (xyz + handedness w)
#ifdef HAS_TANGENT
layout(location=9) in vec4 fragWorldTangent;
#endif

// Auto-define MI ID override for ssbo_material_instance.glsl
#ifndef MATERIAL_INSTANCE_ID_OVERRIDE
#define MATERIAL_INSTANCE_ID_OVERRIDE fragMaterialInstanceID
#endif

#endif // ULRE_COMMON_VARYING_FS_GLSL
