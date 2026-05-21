#ifndef ULRE_POS_SSBO_PACKED_GLSL
#define ULRE_POS_SSBO_PACKED_GLSL

// position_provider/ssbo_packed.glsl
//
// Position source: storage buffer – reads a packed vec3 array by gl_VertexIndex.
// No vertex attribute buffer is used.
//
// MANIFEST: {
//   "vab_count": 0,
//   "position_space": "local",
//   "ssbo": [{
//     "set":     "POSITION_SSBO_SET",
//     "binding": "POSITION_SSBO_BINDING",
//     "name":    "PositionData",
//     "members": ["vec3 positions[]"]
//   }],
//   "ubo": [], "samplers": []
// }
//
// Prerequisites injected by emitter:
//   POSITION_SSBO_SET     – descriptor set index
//   POSITION_SSBO_BINDING – binding index within that set

layout(set=POSITION_SSBO_SET, binding=POSITION_SSBO_BINDING) readonly buffer PositionData
{
    vec3 positions[];
} u_PositionData;

vec4 GetPosition()
{
    return vec4(u_PositionData.positions[gl_VertexIndex], 1.0);
}

#endif // ULRE_POS_SSBO_PACKED_GLSL
