#ifndef MATERIAL_INSTANCE_SSBO_GLSL
#define MATERIAL_INSTANCE_SSBO_GLSL

#include "common/descriptor_macros.glsl"

#ifndef MI_SET
#define MI_SET MATERIAL_SET
#endif

#ifndef MI_BINDING
#define MI_BINDING 0
#endif

layout(set=MID_SET, binding=MID_BINDING) readonly buffer MaterialInstanceIDData {
    uint ids[];
} mid;

uint GetMaterialInstanceID()
{
    return mid.ids[gl_InstanceIndex];
}

#ifndef MATERIAL_INSTANCE_ID_ONLY

#ifdef MATERIAL_INSTANCE_SSBO_SCALAR
layout(scalar, set=MI_SET, binding=MI_BINDING) readonly buffer MaterialInstanceData {
    MaterialInstance mi[];
} mtl;
#else
layout(set=MI_SET, binding=MI_BINDING) readonly buffer MaterialInstanceData {
    MaterialInstance mi[];
} mtl;
#endif

MaterialInstance GetMaterialInstance(uint miID)
{
    return mtl.mi[miID];
}

#ifndef MATERIAL_INSTANCE_CURRENT_ID
#define MATERIAL_INSTANCE_CURRENT_ID GetMaterialInstanceID()
#endif

MaterialInstance GetMaterialInstance()
{
    return GetMaterialInstance(MATERIAL_INSTANCE_CURRENT_ID);
}

#endif 
#endif 