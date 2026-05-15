#ifndef L2W_SSBO_GLSL
#define L2W_SSBO_GLSL

// @require SSBO(TransformID)
// @require SSBO(LocalToWorld)

#ifndef PEROBJECT_SET
#define PEROBJECT_SET 0
#endif
#ifndef TRANSFORM_ID_BINDING
#define TRANSFORM_ID_BINDING 0
#endif
#ifndef TRANSFORM_DATA_BINDING
#define TRANSFORM_DATA_BINDING 1
#endif

layout(set=PEROBJECT_SET, binding=TRANSFORM_ID_BINDING) readonly buffer TransformIDData {
    uint ids[];
} transform_id;

layout(set=PEROBJECT_SET, binding=TRANSFORM_DATA_BINDING) readonly buffer LocalToWorldData {
    mat4 mats[];
} transform_data;

uint GetTransformID()
{
    return transform_id.ids[gl_InstanceIndex];
}

mat4 GetTransform()
{
    return transform_data.mats[GetTransformID()];
}

#endif 
