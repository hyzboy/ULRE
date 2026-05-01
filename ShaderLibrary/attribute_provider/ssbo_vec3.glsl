// attribute_provider/ssbo_vec3.glsl
//
// Attribute source: storage buffer – reads a vec3 array by vertex index.
// Typical uses: Normal, Tangent.
//
// NOTE: under std430 layout, vec3 elements are stride-padded to 16 bytes.
// Upload data as { float x, y, z, _pad; } (16 bytes per vertex) on the CPU side.
//
// This file is designed for repeated inclusion, one per unique ATTRIB_TAG value.
// The emitter must define the following macros before including this file:
//
//   ATTRIB_SET     – Vulkan descriptor set index   (uint literal)
//   ATTRIB_BINDING – binding index within that set  (uint literal)
//   ATTRIB_TAG     – identifier suffix, e.g. Normal
//                    Expands to unique buffer and function names:
//                      buffer instance : u_AttribVec3_<ATTRIB_TAG>
//                      reader function : ReadAttrib_<ATTRIB_TAG>(uint i) → vec3
//
// MANIFEST: {
//   "attrib_provider": "SSBO_Vec3",
//   "byte_stride": 16,
//   "ssbo": [{
//     "set":     "ATTRIB_SET",
//     "binding": "ATTRIB_BINDING",
//     "name":    "AttribVec3Block_<ATTRIB_TAG>",
//     "members": ["vec3 data[]"]
//   }],
//   "ubo": [], "samplers": []
// }

#define _ATTRIB_V3_CAT2(a,b)  a##b
#define _ATTRIB_V3_CAT(a,b)   _ATTRIB_V3_CAT2(a,b)

layout(set=ATTRIB_SET, binding=ATTRIB_BINDING)
    readonly buffer _ATTRIB_V3_CAT(AttribVec3Block_, ATTRIB_TAG)
    { vec3 data[]; }
    _ATTRIB_V3_CAT(u_AttribVec3_, ATTRIB_TAG);

vec3 _ATTRIB_V3_CAT(ReadAttrib_, ATTRIB_TAG)(uint i)
{
    return _ATTRIB_V3_CAT(u_AttribVec3_, ATTRIB_TAG).data[i];
}

#undef _ATTRIB_V3_CAT
#undef _ATTRIB_V3_CAT2
