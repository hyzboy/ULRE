#include<hgl/shadergen/MaterialInstanceConfigurator.h>
#include<hgl/shadergen/MaterialDescriptorStageBinder.h>
#include<hgl/mtl/DescriptorSemanticRegistry.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/math/Matrix.h>
#include<limits>

namespace hgl::graph::mtl
{
bool MaterialInstanceConfigurator::ConfigureMaterialInstance(MaterialDescriptorDB &descriptor_db,MaterialInstanceBlock &material_instance,const uint32_t ssbo_range,const uint32_t data_bytes,const uint32_t shader_stage_flag_bits)
{
    if(material_instance.stride>0)return(false);

    if(shader_stage_flag_bits==0)return(false);

    if(data_bytes==0)return(false);

    material_instance.stride=data_bytes;

    if(!descriptor_db.AddSSBOStruct(SSBODescriptorSemantic::MaterialBindingInstanceData))
        return false;

    material_instance.max_count=std::min<uint32_t>(ssbo_range/data_bytes,HGL_U16_MAX);

    SSBODescriptor *mi_ssbo=CreateSSBODescriptor(SSBODescriptorSemantic::MaterialBindingInstanceData,shader_stage_flag_bits);

    descriptor_db.AddSSBO(shader_stage_flag_bits,
                          GetDescriptorSemanticMeta(SSBODescriptorSemantic::MaterialBindingInstanceData).set_type,
                          mi_ssbo);

    material_instance.stage_bits=shader_stage_flag_bits;

    return(true);
}

bool MaterialInstanceConfigurator::ConfigureMaterialInstance(MaterialDescriptorDB &descriptor_db,MaterialInstanceBlock &material_instance,const uint32_t ssbo_range,const ShaderDataSchema schema,const ShaderDataSchemaInfo &schema_info,const uint32_t shader_stage_flag_bits)
{
    if(schema==ShaderDataSchema::None)
        return false;

    if(schema_info.byte_size==0)
        return false;

    if(!ConfigureMaterialInstance(descriptor_db,material_instance,ssbo_range,schema_info.byte_size,shader_stage_flag_bits))
        return false;

    material_instance.schema=schema;
    material_instance.schema_file=schema_info.glsl_schema_file ? schema_info.glsl_schema_file : "";

    return true;
}

bool MaterialInstanceConfigurator::ConfigureLocalToWorld(MaterialDescriptorDB &descriptor_db,ShaderStageMap &shader_map,LocalToWorldBlock &local_to_world,const uint32_t ssbo_range,const uint32_t shader_stage_flag_bits)
{
    if(shader_stage_flag_bits==0)return(false);

    local_to_world.max_count=std::min<uint32_t>(ssbo_range/sizeof(math::Matrix4f),HGL_U16_MAX);

    if(!descriptor_db.AddSSBOStruct(SSBODescriptorSemantic::TransformData))
        return false;

    const auto &meta=GetDescriptorSemanticMeta(SSBODescriptorSemantic::TransformData);

    if(!MaterialDescriptorStageBinder::AddResolvedSSBO(descriptor_db,
                                                       shader_map,
                                                       shader_stage_flag_bits,
                                                       meta.set_type,
                                                       SSBODescriptorSemantic::TransformData,
                                                       meta.struct_name?meta.struct_name:"",
                                                       meta.name?meta.name:""))
        return false;

    local_to_world.stage_bits=shader_stage_flag_bits;
    local_to_world.enabled=true;

    return(true);
}
}//namespace hgl::graph::mtl
