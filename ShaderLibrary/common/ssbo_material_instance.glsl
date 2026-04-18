#ifndef MATERIAL_INSTANCE_SSBO_GLSL
#define MATERIAL_INSTANCE_SSBO_GLSL

// @require SSBO(MaterialBindingInstanceID)

layout(std430, set=PERMATERIAL_SET, binding=MBI_ID_BINDING) readonly buffer MaterialInstanceIDData {
    uint ids[];
} mbi_id;

uint GetMaterialInstanceID()
{
#ifdef MATERIAL_INSTANCE_ID_OVERRIDE
    return MATERIAL_INSTANCE_ID_OVERRIDE;
#else
    return mbi_id.ids[gl_InstanceIndex];
#endif
}

#ifndef MATERIAL_INSTANCE_ID_ONLY

// @require SSBO(MaterialBindingInstance)

layout(scalar, set=PERMATERIAL_SET, binding=MBI_DATA_BINDING) readonly buffer MaterialBindingInstanceData {
    MaterialBindingInstance datas[];
} mbi_data;

MaterialBindingInstance GetMaterialBindingInstance(uint id)
{
    return mbi_data.datas[id];
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
