// control/varying_locations.glsl — Canonical layout location numbers for VS/FS varyings
//
// These constants document the binding slots used in varying_vs.glsl / varying_fs.glsl.
// Include this only if you need to reference locations by name (e.g. in custom billboard VS).

#ifndef ULRE_CONTROL_VARYING_LOCATIONS_GLSL
#define ULRE_CONTROL_VARYING_LOCATIONS_GLSL

#define VARYING_LOC_MATERIAL_INSTANCE_ID  0
#define VARYING_LOC_WORLD_POS             1
#define VARYING_LOC_WORLD_NORMAL          2
#define VARYING_LOC_UV0                   3
#define VARYING_LOC_VERTEX_COLOR          4
#define VARYING_LOC_BILLBOARD_TEXCOORD    5
#define VARYING_LOC_DIRECTION             6
#define VARYING_LOC_LUMINANCE             7
#define VARYING_LOC_CLIP_POS              8
#define VARYING_LOC_WORLD_TANGENT         9

#endif // ULRE_CONTROL_VARYING_LOCATIONS_GLSL
