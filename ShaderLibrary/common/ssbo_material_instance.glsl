#ifndef MATERIAL_INSTANCE_SSBO_GLSL
#define MATERIAL_INSTANCE_SSBO_GLSL

layout(set=PERMATERIAL_SET, binding=MID_BINDING) readonly buffer MaterialInstanceIDData {
    uint ids[];
} mid;

uint GetMaterialInstanceID()
{
#ifdef MATERIAL_INSTANCE_ID_OVERRIDE
    return MATERIAL_INSTANCE_ID_OVERRIDE;
#else
    return mid.ids[gl_InstanceIndex];
#endif
}

#ifndef MATERIAL_INSTANCE_ID_ONLY

#ifdef MATERIAL_INSTANCE_SSBO_SCALAR
layout(scalar, set=PERMATERIAL_SET, binding=MI_BINDING) readonly buffer MaterialInstanceData {
    MaterialInstance mi[];
} mtl;
#else
layout(set=PERMATERIAL_SET, binding=MI_BINDING) readonly buffer MaterialInstanceData {
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