// attribute_provider/ssbo_packed_normal_oct.glsl
//
// Attribute source: storage buffer – reads a packed R16G16_SNORM uint encoding an
// octahedral normal and reconstructs a unit-length vec3.
// Typical use: compressed vertex Normal attribute (4 bytes vs 12/16 bytes).
//
// Octahedral encoding:
//   (u,v) in [-1,1]²; reconstruct z from |u|+|v|<=1, then L2-normalize.
//   See Cigolle et al. "Survey of Efficient Representations for Independent Unit Vectors"
//
// This file is designed for repeated inclusion, one per unique ATTRIB_TAG value.
// The emitter must define the following macros before including this file:
//
//   ATTRIB_SET     – Vulkan descriptor set index   (uint literal)
//   ATTRIB_BINDING – binding index within that set  (uint literal)
//   ATTRIB_TAG     – identifier suffix, e.g. Normal
//                    Expands to unique buffer and function names:
//                      buffer instance : u_AttribOctN_<ATTRIB_TAG>
//                      reader function : ReadAttrib_<ATTRIB_TAG>(uint i) → vec3
//
// MANIFEST: {
//   "attrib_provider": "SSBO_PackedNormal_Oct",
//   "byte_stride": 4,
//   "ssbo": [{
//     "set":     "ATTRIB_SET",
//     "binding": "ATTRIB_BINDING",
//     "name":    "AttribOctNBlock_<ATTRIB_TAG>",
//     "members": ["uint data[]"]
//   }],
//   "ubo": [], "samplers": []
// }

#define _ATTRIB_OCTN_CAT2(a,b)  a##b
#define _ATTRIB_OCTN_CAT(a,b)   _ATTRIB_OCTN_CAT2(a,b)

layout(set=ATTRIB_SET, binding=ATTRIB_BINDING)
    readonly buffer _ATTRIB_OCTN_CAT(AttribOctNBlock_, ATTRIB_TAG)
    { uint data[]; }
    _ATTRIB_OCTN_CAT(u_AttribOctN_, ATTRIB_TAG);

vec3 _ATTRIB_OCTN_CAT(ReadAttrib_, ATTRIB_TAG)(uint i)
{
    uint packed = _ATTRIB_OCTN_CAT(u_AttribOctN_, ATTRIB_TAG).data[i];
    // Unpack two signed 16-bit values to [-1, 1]
    float u = float(int(packed        & 0xFFFFu) << 16) / float(0x7FFFFFFF);
    float v = float(int((packed >> 16) & 0xFFFFu) << 16) / float(0x7FFFFFFF);
    vec3 n = vec3(u, v, 1.0 - abs(u) - abs(v));
    // Remap lower hemisphere (fold corners)
    if (n.z < 0.0)
    {
        float ox = n.x;
        n.x = (1.0 - abs(n.y)) * (ox >= 0.0 ? 1.0 : -1.0);
        n.y = (1.0 - abs(ox))  * (n.y >= 0.0 ? 1.0 : -1.0);
    }
    return normalize(n);
}

#undef _ATTRIB_OCTN_CAT
#undef _ATTRIB_OCTN_CAT2
