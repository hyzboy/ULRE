#ifndef L2W_SSBO_GLSL
#define L2W_SSBO_GLSL

// @require SSBO(TransformID)
// @require SSBO(LocalToWorld)

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