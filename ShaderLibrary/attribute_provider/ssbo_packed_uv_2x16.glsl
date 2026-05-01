// attribute_provider/ssbo_packed_uv_2x16.glsl
//
// Attribute source: storage buffer – reads a packed R16G16_UNORM uint encoding
// two UV coordinates and unpacks them to a vec2 in [0, 1].
// Typical use: compressed TexCoord0/TexCoord1 attribute (4 bytes vs 8 bytes).
//
// This file is designed for repeated inclusion, one per unique ATTRIB_TAG value.
// The emitter must define the following macros before including this file:
//
//   ATTRIB_SET     – Vulkan descriptor set index   (uint literal)
//   ATTRIB_BINDING – binding index within that set  (uint literal)
//   ATTRIB_TAG     – identifier suffix, e.g. TexCoord0
//                    Expands to unique buffer and function names:
//                      buffer instance : u_AttribUV16_<ATTRIB_TAG>
//                      reader function : ReadAttrib_<ATTRIB_TAG>(uint i) → vec2
//
// MANIFEST: {
//   "attrib_provider": "SSBO_PackedUV_2x16",
//   "byte_stride": 4,
//   "ssbo": [{
//     "set":     "ATTRIB_SET",
//     "binding": "ATTRIB_BINDING",
//     "name":    "AttribUV16Block_<ATTRIB_TAG>",
//     "members": ["uint data[]"]
//   }],
//   "ubo": [], "samplers": []
// }

#define _ATTRIB_UV16_CAT2(a,b)  a##b
#define _ATTRIB_UV16_CAT(a,b)   _ATTRIB_UV16_CAT2(a,b)

layout(set=ATTRIB_SET, binding=ATTRIB_BINDING)
    readonly buffer _ATTRIB_UV16_CAT(AttribUV16Block_, ATTRIB_TAG)
    { uint data[]; }
    _ATTRIB_UV16_CAT(u_AttribUV16_, ATTRIB_TAG);

vec2 _ATTRIB_UV16_CAT(ReadAttrib_, ATTRIB_TAG)(uint i)
{
    uint packed = _ATTRIB_UV16_CAT(u_AttribUV16_, ATTRIB_TAG).data[i];
    return vec2(
        float(packed        & 0xFFFFu) / 65535.0,
        float((packed >> 16) & 0xFFFFu) / 65535.0
    );
}

#undef _ATTRIB_UV16_CAT
#undef _ATTRIB_UV16_CAT2
