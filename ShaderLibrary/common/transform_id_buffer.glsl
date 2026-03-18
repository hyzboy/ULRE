// transform_id_buffer.glsl — TransformID descriptor-backed accessor (SSBO only)
//
// Requires descriptor_macros.glsl and this preamble macro:
//   TRANSFORM_ID_FROM_DESCRIPTOR : 0/1

#ifndef TRANSFORM_ID_BUFFER_GLSL
#define TRANSFORM_ID_BUFFER_GLSL

#ifndef TRANSFORM_ID_FROM_DESCRIPTOR
#define TRANSFORM_ID_FROM_DESCRIPTOR 0
#endif

#if TRANSFORM_ID_FROM_DESCRIPTOR

#define TRANSFORM_ID_BUFFER \
    layout(set=TID_SET, binding=TID_BINDING) readonly buffer TransformIDData { \
        uint ids[]; \
    } tid

#define FetchTransformID() (tid.ids[gl_InstanceIndex])

#else

#define TRANSFORM_ID_BUFFER
#define FetchTransformID() (0u)

#endif

#endif // TRANSFORM_ID_BUFFER_GLSL
