// attribute_provider/ssbo_vec2.glsl
//
// Attribute source: storage buffer – reads a tightly-packed vec2 array by vertex index.
// Typical uses: TexCoord0, TexCoord1.
//
// This file is designed for repeated inclusion, one per unique ATTRIB_TAG value.
// The emitter must define the following macros before including this file:
//
//   ATTRIB_SET     – Vulkan descriptor set index   (uint literal)
//   ATTRIB_BINDING – binding index within that set  (uint literal)
//   ATTRIB_TAG     – identifier suffix, e.g. TexCoord0
//                    Expands to unique buffer and function names:
//                      buffer instance : u_AttribVec2_<ATTRIB_TAG>
//                      reader function : ReadAttrib_<ATTRIB_TAG>(uint i) → vec2
//
// MANIFEST: {
//   "attrib_provider": "SSBO_Vec2",
//   "byte_stride": 8,
//   "ssbo": [{
//     "set":     "ATTRIB_SET",
//     "binding": "ATTRIB_BINDING",
//     "name":    "AttribVec2Block_<ATTRIB_TAG>",
//     "members": ["vec2 data[]"]
//   }],
//   "ubo": [], "samplers": []
// }

#define _ATTRIB_V2_CAT2(a,b)  a##b
#define _ATTRIB_V2_CAT(a,b)   _ATTRIB_V2_CAT2(a,b)

layout(set=ATTRIB_SET, binding=ATTRIB_BINDING)
    readonly buffer _ATTRIB_V2_CAT(AttribVec2Block_, ATTRIB_TAG)
    { vec2 data[]; }
    _ATTRIB_V2_CAT(u_AttribVec2_, ATTRIB_TAG);

vec2 _ATTRIB_V2_CAT(ReadAttrib_, ATTRIB_TAG)(uint i)
{
    return _ATTRIB_V2_CAT(u_AttribVec2_, ATTRIB_TAG).data[i];
}

#undef _ATTRIB_V2_CAT
#undef _ATTRIB_V2_CAT2
