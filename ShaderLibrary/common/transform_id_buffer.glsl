// transform_id_buffer.glsl — TransformID descriptor-backed accessor (SSBO only)
//
// Requires descriptor_macros.glsl.

#ifndef TRANSFORM_ID_BUFFER_GLSL
#define TRANSFORM_ID_BUFFER_GLSL

#define TRANSFORM_ID_BUFFER \
    layout(set=TID_SET, binding=TID_BINDING) readonly buffer TransformIDData { \
        uint ids[]; \
    } tid

#define FetchTransformID() (tid.ids[gl_InstanceIndex])

#endif // TRANSFORM_ID_BUFFER_GLSL
