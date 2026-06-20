#ifndef MATERIAL_INSTANCE_SSBO_GLSL
#define MATERIAL_INSTANCE_SSBO_GLSL

// @require SSBO(MaterialBindingInstanceID)

// Fallback schema for legacy/bridge paths that forgot to inject a schema header.
// Keep this guarded so schema-owned MaterialBindingInstance definitions remain authoritative.
#if !defined(ULRE_SHADER_SCHEMA_COLOR4F_GLSL) \
 && !defined(ULRE_SHADER_SCHEMA_TEXT_COLOR_GLSL) \
 && !defined(ULRE_SHADER_SCHEMA_BILLBOARD_SIZE_GLSL) \
 && !defined(ULRE_SHADER_SCHEMA_PBR_COLOR_PARAMS_GLSL) \
 && !defined(ULRE_SHADER_SCHEMA_STANDARD_PARAMS_GLSL) \
 && !defined(ULRE_SHADER_SCHEMA_TEXTURE_ARRAY_ID_GLSL)
struct MaterialBindingInstance {
    uvec2 BillboardSize;
};
#define ULRE_SHADER_SCHEMA_BILLBOARD_SIZE_GLSL
#endif

#if defined(MBI_ID_BINDING)
layout(std430, set=PERMATERIAL_SET, binding=MBI_ID_BINDING) readonly buffer MaterialInstanceIDData {
    uint ids[];
} mbi_id;
#endif

uint GetMaterialInstanceID()
{
#ifdef MATERIAL_INSTANCE_ID_OVERRIDE
    return MATERIAL_INSTANCE_ID_OVERRIDE;
#elif defined(MBI_ID_BINDING)
    return mbi_id.ids[gl_InstanceIndex];
#else
    return 0u;
#endif
}

#ifndef MATERIAL_INSTANCE_ID_ONLY

// @require SSBO(MaterialBindingInstance)

#ifndef PERMATERIAL_SET
#define PERMATERIAL_SET 0
#endif
#ifndef MBI_DATA_BINDING
#define MBI_DATA_BINDING 1
#endif

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
