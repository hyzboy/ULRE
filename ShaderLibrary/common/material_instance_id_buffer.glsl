// material_instance_id_buffer.glsl — MaterialInstanceID descriptor-backed accessor (SSBO only)
//
// Requires descriptor_macros.glsl and this preamble macro:
//   MATERIAL_INSTANCE_ID_FROM_DESCRIPTOR : 0/1

#ifndef MATERIAL_INSTANCE_ID_BUFFER_GLSL
#define MATERIAL_INSTANCE_ID_BUFFER_GLSL

#ifndef MATERIAL_INSTANCE_ID_FROM_DESCRIPTOR
#define MATERIAL_INSTANCE_ID_FROM_DESCRIPTOR 0
#endif

#if MATERIAL_INSTANCE_ID_FROM_DESCRIPTOR

#define MATERIAL_INSTANCE_ID_BUFFER \
    layout(set=MID_SET, binding=MID_BINDING) readonly buffer MaterialInstanceIDData { \
        uint ids[]; \
    } mid

#define FetchMaterialInstanceID() (mid.ids[gl_InstanceIndex])

#else

#define MATERIAL_INSTANCE_ID_BUFFER
#define FetchMaterialInstanceID() (0u)

#endif

#endif // MATERIAL_INSTANCE_ID_BUFFER_GLSL
