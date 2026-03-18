// transform_id_buffer.glsl — TransformID descriptor-backed accessor
//
// Requires descriptor_macros.glsl and these preamble macros:
//   TRANSFORM_ID_FROM_DESCRIPTOR : 0/1
//   TRANSFORM_ID_DESCRIPTOR_UBO  : 0/1
//   TRANSFORM_ID_UBO_MAX         : max element count for UBO mode

#ifndef TRANSFORM_ID_BUFFER_GLSL
#define TRANSFORM_ID_BUFFER_GLSL

#ifndef TRANSFORM_ID_FROM_DESCRIPTOR
#define TRANSFORM_ID_FROM_DESCRIPTOR 0
#endif

#ifndef TRANSFORM_ID_DESCRIPTOR_UBO
#define TRANSFORM_ID_DESCRIPTOR_UBO 0
#endif

#ifndef TRANSFORM_ID_UBO_MAX
#define TRANSFORM_ID_UBO_MAX 65536
#endif

#if TRANSFORM_ID_FROM_DESCRIPTOR

#if TRANSFORM_ID_DESCRIPTOR_UBO
#define TRANSFORM_ID_BUFFER \
    layout(set=TID_SET, binding=TID_BINDING) uniform TransformIDData { \
        uint ids[TRANSFORM_ID_UBO_MAX]; \
    } tid
#else
#define TRANSFORM_ID_BUFFER \
    layout(set=TID_SET, binding=TID_BINDING) readonly buffer TransformIDData { \
        uint ids[]; \
    } tid
#endif

#define FetchTransformID() (tid.ids[gl_InstanceIndex])

#else

#define TRANSFORM_ID_BUFFER
#define FetchTransformID() (0u)

#endif

#endif // TRANSFORM_ID_BUFFER_GLSL
