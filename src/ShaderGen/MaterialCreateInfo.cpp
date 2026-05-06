#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/BindingContractBuilder.h>
#include<hgl/shadergen/DescriptorLayoutBuilder.h>
#include<hgl/shadergen/MaterialDescriptorStageBinder.h>
#include<hgl/shadergen/MaterialInstanceConfigurator.h>
#include<hgl/shadergen/ShaderSetCompiler.h>
#include<hgl/shadergen/ShaderStageBuildSet.h>
#include<hgl/shadergen/ShaderStageIO.h>
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/shadergen/device/DeviceProfile.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/math/Matrix.h>
#include<string>
#include<limits>

using namespace hgl;
using namespace hgl::graph;

namespace hgl::graph::mtl{

template<typename SemanticT>
static bool ResolveSemanticMeta(
    const SemanticT semantic,
    const DescriptorSemanticMeta *&meta)
{
    if(!RangeCheck(semantic))
        return false;

    const DescriptorSemanticMeta &candidate = GetDescriptorSemanticMeta(semantic);

    if(!candidate.struct_name || !*candidate.struct_name)
        return false;

    if(!candidate.name || !*candidate.name)
        return false;

    meta = &candidate;
    return true;
}

static bool ResolveUBOSemanticMeta(
    const UBODescriptorSemantic semantic,
    const DescriptorSemanticMeta *&meta)
{
    return ResolveSemanticMeta(semantic, meta);
}

static bool ResolveSSBOSemanticMeta(
    const SSBODescriptorSemantic semantic,
    const DescriptorSemanticMeta *&meta)
{
    return ResolveSemanticMeta(semantic, meta);
}


MaterialCreateInfo::MaterialCreateInfo(const MaterialCreateConfig *mc)
    : config(*mc)
{
    ShaderStageBuildSet shader_stage_set(shader_map);

    if(HasVertex    ())shader_stage_set.Add(new ShaderCreateInfoVertex(&descriptor_db));
    if(HasFragment  ())shader_stage_set.Add(new ShaderCreateInfo(new FragmentShaderStageIO(),&descriptor_db));

    ubo_range=0;
    ssbo_range=0;

    material_instance = MaterialInstanceBlock{};
    local_to_world = LocalToWorldBlock{0, 0, config.local_to_world};
}

MaterialCreateInfo::~MaterialCreateInfo()
{
    ShaderStageBuildSet(shader_map).DeleteAllShaders();
}

bool MaterialCreateInfo::AddResolvedUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const UBODescriptorSemantic semantic,const std::string &struct_name,const std::string &name)
{
    return MaterialDescriptorStageBinder::AddResolvedUBO(descriptor_db,shader_map,flag_bits,set_type,semantic,struct_name,name);
}

bool MaterialCreateInfo::AddUBOStruct(const uint32_t flag_bits,const UBODescriptorSemantic semantic)
{
    const DescriptorSemanticMeta *meta = nullptr;

    if(!ResolveUBOSemanticMeta(semantic,meta))
        return false;

    if(!descriptor_db.AddUBOStruct(semantic))
        return false;

    return AddResolvedUBO(flag_bits,meta->set_type,semantic,meta->struct_name,meta->name);
}

bool MaterialCreateInfo::AddResolvedSSBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const SSBODescriptorSemantic semantic,const std::string &struct_name,const std::string &name)
{
    return MaterialDescriptorStageBinder::AddResolvedSSBO(descriptor_db,shader_map,flag_bits,set_type,semantic,struct_name,name);
}

bool MaterialCreateInfo::AddSSBOStruct(const uint32_t flag_bits,const SSBODescriptorSemantic semantic)
{
    const DescriptorSemanticMeta *meta = nullptr;

    if(!ResolveSSBOSemanticMeta(semantic,meta))
        return false;

    if(!descriptor_db.AddSSBOStruct(semantic))
        return false;

    return AddResolvedSSBO(flag_bits,meta->set_type,semantic,meta->struct_name,meta->name);
}

bool MaterialCreateInfo::AddTexture(const ShaderStage flag_bit,const TextureType &tt,const SamplerSlot slot)
{
    return MaterialDescriptorStageBinder::AddTexture(descriptor_db,flag_bit,tt,slot);
}

bool MaterialCreateInfo::AddTextureSampler(const ShaderStage flag_bit,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint)
{
    return MaterialDescriptorStageBinder::AddTextureSampler(descriptor_db,flag_bit,st,slot,channel_hint);
}

bool MaterialCreateInfo::AddTextureSampler(const uint32_t flag_bits,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint)
{
    return MaterialDescriptorStageBinder::AddTextureSampler(descriptor_db,shader_map,flag_bits,st,slot,channel_hint);
}

/**
* 设置材质实例数据长度
* @param data_bytes     单个材质实例数据长度
* @param shader_stage_flag_bits   具体使用材质实例的shader
* @return 是否设置成功
*/
bool MaterialCreateInfo::SetMaterialInstance(const uint32_t data_bytes,const uint32_t shader_stage_flag_bits)
{
    return MaterialInstanceConfigurator::ConfigureMaterialInstance(descriptor_db,
                                                                   material_instance,
                                                                   ssbo_range,
                                                                   data_bytes,
                                                                   shader_stage_flag_bits);
}

bool MaterialCreateInfo::SetMaterialInstance(const ShaderDataSchema schema,
                                             const ShaderDataSchemaInfo &schema_info,
                                             const uint32_t shader_stage_flag_bits)
{
    return MaterialInstanceConfigurator::ConfigureMaterialInstance(descriptor_db,
                                                                   material_instance,
                                                                   ssbo_range,
                                                                   schema,
                                                                   schema_info,
                                                                   shader_stage_flag_bits);
}

void MaterialCreateInfo::BuildBindingContract()
{
    BindingContractBuilder::Build(descriptor_db,binding_contract);
}

bool MaterialCreateInfo::SetLocalToWorld(const uint32_t shader_stage_flag_bits)
{
    return MaterialInstanceConfigurator::ConfigureLocalToWorld(descriptor_db,
                                                               shader_map,
                                                               local_to_world,
                                                               ssbo_range,
                                                               shader_stage_flag_bits);
}
//
void MaterialCreateInfo::SetDevice(const contract::PhysicalDeviceProfileLite *profile)
{
    if(!profile)
    {
        ubo_range=0;
        ssbo_range=0;
        return;
    }

    const uint64_t max_u32=std::numeric_limits<uint32_t>::max();
    const uint64_t profile_ubo=profile->limits.max_uniform_buffer_range;
    const uint64_t profile_ssbo=profile->limits.max_storage_buffer_range;

    ubo_range=static_cast<uint32_t>((profile_ubo>max_u32)?max_u32:profile_ubo);
    ssbo_range=static_cast<uint32_t>((profile_ssbo>max_u32)?max_u32:profile_ssbo);
}

bool MaterialCreateInfo::CompilePreparedShaderSources()
{
    ShaderStageBuildSet shader_stage_set(shader_map);

    DescriptorLayoutBuilder::Finalize(descriptor_db,binding_contract);
    return ShaderSetCompiler::Compile(shader_stage_set.GetMap());
}

bool MaterialCreateInfo::CompileShaderStagesToSPV()
{
    return CompilePreparedShaderSources();
}
}//namespace hgl::graph::mtl
