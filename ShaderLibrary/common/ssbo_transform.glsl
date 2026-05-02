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

#if !defined(ULRE_INSTANCE_INDEX)
#if defined(ULRE_MESH_SHADER_STAGE)
#define ULRE_INSTANCE_INDEX 0u
#else
#define ULRE_INSTANCE_INDEX gl_InstanceIndex
#endif
#endif

uint GetTransformID()
{
    return transform_id.ids[ULRE_INSTANCE_INDEX];
}

mat4 GetTransform()
{
    return transform_data.mats[GetTransformID()];
}

#endif 