// attribute_provider/ssbo_packed_rgba8.glsl
//
// Attribute source: storage buffer – reads a packed R8G8B8A8_UNORM uint array
// and unpacks it to a normalized vec4 in [0, 1].
// Typical use: vertex Color attribute stored as 4-byte RGBA.
//
// This file is designed for repeated inclusion, one per unique ATTRIB_TAG value.
// The emitter must define the following macros before including this file:
//
//   ATTRIB_SET     – Vulkan descriptor set index   (uint literal)
//   ATTRIB_BINDING – binding index within that set  (uint literal)
//   ATTRIB_TAG     – identifier suffix, e.g. Color
//                    Expands to unique buffer and function names:
//                      buffer instance : u_AttribRGBA8_<ATTRIB_TAG>
//                      reader function : ReadAttrib_<ATTRIB_TAG>(uint i) → vec4
//
// MANIFEST: {
//   "attrib_provider": "SSBO_PackedRGBA8",
//   "byte_stride": 4,
//   "ssbo": [{
//     "set":     "ATTRIB_SET",
//     "binding": "ATTRIB_BINDING",
//     "name":    "AttribRGBA8Block_<ATTRIB_TAG>",
//     "members": ["uint data[]"]
//   }],
//   "ubo": [], "samplers": []
// }

#define _ATTRIB_RGBA8_CAT2(a,b)  a##b
#define _ATTRIB_RGBA8_CAT(a,b)   _ATTRIB_RGBA8_CAT2(a,b)

layout(set=ATTRIB_SET, binding=ATTRIB_BINDING)
    readonly buffer _ATTRIB_RGBA8_CAT(AttribRGBA8Block_, ATTRIB_TAG)
    { uint data[]; }
    _ATTRIB_RGBA8_CAT(u_AttribRGBA8_, ATTRIB_TAG);

vec4 _ATTRIB_RGBA8_CAT(ReadAttrib_, ATTRIB_TAG)(uint i)
{
    uint packed = _ATTRIB_RGBA8_CAT(u_AttribRGBA8_, ATTRIB_TAG).data[i];
    return vec4(
        float((packed      ) & 0xFFu) / 255.0,
        float((packed >>  8) & 0xFFu) / 255.0,
        float((packed >> 16) & 0xFFu) / 255.0,
        float((packed >> 24) & 0xFFu) / 255.0
    );
}

#undef _ATTRIB_RGBA8_CAT
#undef _ATTRIB_RGBA8_CAT2
