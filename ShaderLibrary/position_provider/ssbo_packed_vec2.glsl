#ifndef ULRE_POS_SSBO_PACKED_VEC2_GLSL
#define ULRE_POS_SSBO_PACKED_VEC2_GLSL

// position_provider/ssbo_packed_vec2.glsl
//
// Position source: storage buffer – reads packed vec2 by gl_VertexIndex.
// Returns vec3(x, y, 0) in local space for 2D pipelines.
//
// MANIFEST: {
//   "vab_count": 0,
//   "position_space": "local",
//   "ssbo": [{
//     "set":     "POSITION_SSBO_SET",
//     "binding": "POSITION_SSBO_BINDING",
//     "name":    "PositionData",
//     "members": ["vec2 positions[]"]
//   }],
//   "ubo": [], "samplers": []
// }

layout(scalar, set=POSITION_SSBO_SET, binding=POSITION_SSBO_BINDING) readonly buffer PositionData
{
    vec2 positions[];
} u_PositionData;

#if !defined(ULRE_POSITION_FETCH_INDEX)
#if defined(ULRE_MESH_SHADER_STAGE)
#define ULRE_POSITION_FETCH_INDEX 0u
#else
#define ULRE_POSITION_FETCH_INDEX gl_VertexIndex
#endif
#endif

vec3 GetPositionLocal()
{
    return vec3(u_PositionData.positions[ULRE_POSITION_FETCH_INDEX], 0.0);
}

#endif // ULRE_POS_SSBO_PACKED_VEC2_GLSL
