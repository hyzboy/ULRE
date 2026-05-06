#pragma once

#include<hgl/shadergen/MaterialDescriptorDB.h>
#include<hgl/shadergen/ShaderStageMap.h>
#include<hgl/shadergen/MaterialBuilderBlocks.h>
#include<hgl/mtl/ShaderDataSchema.h>

namespace hgl::graph::mtl
{
class MaterialInstanceConfigurator
{
public:
    static bool ConfigureMaterialInstance(MaterialDescriptorDB &descriptor_db,MaterialInstanceBlock &material_instance,const uint32_t ssbo_range,const uint32_t data_bytes,const uint32_t shader_stage_flag_bits);
    static bool ConfigureMaterialInstance(MaterialDescriptorDB &descriptor_db,MaterialInstanceBlock &material_instance,const uint32_t ssbo_range,const ShaderDataSchema schema,const ShaderDataSchemaInfo &schema_info,const uint32_t shader_stage_flag_bits);
    static bool ConfigureLocalToWorld(MaterialDescriptorDB &descriptor_db,ShaderStageMap &shader_map,LocalToWorldBlock &local_to_world,const uint32_t ssbo_range,const uint32_t shader_stage_flag_bits);
};
}//namespace hgl::graph::mtl
