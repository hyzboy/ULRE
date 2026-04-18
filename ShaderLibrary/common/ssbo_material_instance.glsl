#ifndef MATERIAL_INSTANCE_SSBO_GLSL
#define MATERIAL_INSTANCE_SSBO_GLSL

// @require SSBO(MaterialBindingInstanceID)

layout(std430, set=PERMATERIAL_SET, binding=MBI_ID_BINDING) readonly buffer MaterialInstanceIDData {
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

// @require SSBO(MaterialBindingInstance)

layout(scalar, set=PERMATERIAL_SET, binding=MBI_DATA_BINDING) readonly buffer MaterialBindingInstanceData {
    MaterialBindingInstance mi[];
} mtl;

MaterialBindingInstance GetMaterialBindingInstance(uint miID)
{
    return mtl.mi[miID];
}

#ifndef MATERIAL_INSTANCE_CURRENT_ID
#define MATERIAL_INSTANCE_CURRENT_ID GetMaterialInstanceID()
#endif

MaterialBindingInstance GetMaterialBindingInstance()
{
    return GetMaterialBindingInstance(MATERIAL_INSTANCE_CURRENT_ID);
}

#endif 
#endif 