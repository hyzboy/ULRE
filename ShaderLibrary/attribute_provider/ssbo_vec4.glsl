// attribute_provider/ssbo_vec4.glsl
//
// Attribute source: storage buffer – reads a tightly-packed vec4 array by vertex index.
// Typical uses: Color (RGBA float), Joints (ivec4 reinterpret), Weights (vec4).
//
// This file is designed for repeated inclusion, one per unique ATTRIB_TAG value.
// The emitter must define the following macros before including this file:
//
//   ATTRIB_SET     – Vulkan descriptor set index   (uint literal)
//   ATTRIB_BINDING – binding index within that set  (uint literal)
//   ATTRIB_TAG     – identifier suffix, e.g. Color
//                    Expands to unique buffer and function names:
//                      buffer instance : u_AttribVec4_<ATTRIB_TAG>
//                      reader function : ReadAttrib_<ATTRIB_TAG>(uint i) → vec4
//
// MANIFEST: {
//   "attrib_provider": "SSBO_Vec4",
//   "byte_stride": 16,
//   "ssbo": [{
//     "set":     "ATTRIB_SET",
//     "binding": "ATTRIB_BINDING",
//     "name":    "AttribVec4Block_<ATTRIB_TAG>",
//     "members": ["vec4 data[]"]
//   }],
//   "ubo": [], "samplers": []
// }

#define _ATTRIB_V4_CAT2(a,b)  a##b
#define _ATTRIB_V4_CAT(a,b)   _ATTRIB_V4_CAT2(a,b)

layout(set=ATTRIB_SET, binding=ATTRIB_BINDING)
    readonly buffer _ATTRIB_V4_CAT(AttribVec4Block_, ATTRIB_TAG)
    { vec4 data[]; }
    _ATTRIB_V4_CAT(u_AttribVec4_, ATTRIB_TAG);

vec4 _ATTRIB_V4_CAT(ReadAttrib_, ATTRIB_TAG)(uint i)
{
    return _ATTRIB_V4_CAT(u_AttribVec4_, ATTRIB_TAG).data[i];
}

#undef _ATTRIB_V4_CAT
#undef _ATTRIB_V4_CAT2
