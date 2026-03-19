// material_instance_id_buffer.glsl — MaterialInstanceID descriptor-backed accessor (SSBO only)
//
// Requires descriptor_macros.glsl.

#ifndef MATERIAL_INSTANCE_ID_BUFFER_GLSL
#define MATERIAL_INSTANCE_ID_BUFFER_GLSL

#define MATERIAL_INSTANCE_ID_BUFFER \
    layout(set=MID_SET, binding=MID_BINDING) readonly buffer MaterialInstanceIDData { \
        uint ids[]; \
    } mid

#define FetchMaterialInstanceID() (mid.ids[gl_InstanceIndex])

#endif // MATERIAL_INSTANCE_ID_BUFFER_GLSL
