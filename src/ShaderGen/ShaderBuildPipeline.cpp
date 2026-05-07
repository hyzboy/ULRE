#include<hgl/shadergen/ShaderBuildPipeline.h>
#include<hgl/shadergen/DescriptorLayoutBuilder.h>
#include<hgl/shadergen/MaterialDescriptorDB.h>
#include<hgl/shadergen/MaterialDescriptorStageBinder.h>
#include<hgl/mtl/UBOCommon.h>

namespace
{
static bool HasStageBit(const uint32_t bits,const hgl::graph::ShaderStage stage)
{
    return (bits&uint32_t(stage))!=0;
}

static hgl::graph::ShaderStage ResolveTextureBindStage(const uint32_t stage_bits)
{
    if(HasStageBit(stage_bits,hgl::graph::ShaderStage::Fragment))
        return hgl::graph::ShaderStage::Fragment;

    return hgl::graph::ShaderStage::Vertex;
}

static bool ApplyTextureSamplerOverrides(const hgl::graph::mtl::MaterialCreateConfig &config,
                                         hgl::graph::MaterialDescriptorDB &descriptor_db)
{
    if(config.sampler_feature_bits_override==0)
        return true;

    const hgl::graph::ShaderStage bind_stage=ResolveTextureBindStage(config.shader_stage_flag_bit);

    for(size_t i=0;i<hgl::graph::mtl::SamplerSlotCount;++i)
    {
        const uint32_t bit=(1u<<i);

        if((config.sampler_feature_bits_override&bit)==0)
            continue;

        const auto slot=static_cast<hgl::graph::mtl::SamplerSlot>(i);

        if(!hgl::graph::mtl::MaterialDescriptorStageBinder::AddTextureSampler(descriptor_db,
                                                                               bind_stage,
                                                                               hgl::graph::SamplerType::Sampler2D,
                                                                               slot,
                                                                               hgl::graph::TextureChannelHint::RGBA))
            return false;
    }

    return true;
}

static bool AddSSBOBySemantic(hgl::graph::MaterialDescriptorDB &descriptor_db,
                              const hgl::graph::mtl::SSBODescriptorSemantic semantic,
                              const uint32_t stage_bits)
{
    if(!descriptor_db.AddSSBOStruct(semantic))
        return false;

    const auto &meta=hgl::graph::mtl::GetDescriptorSemanticMeta(semantic);
    auto *ssbo=hgl::graph::mtl::CreateSSBODescriptor(semantic,stage_bits);
    if(!ssbo)
        return false;

    return descriptor_db.AddSSBO(stage_bits,meta.set_type,ssbo)!=nullptr;
}

static bool AddUBOBySemantic(hgl::graph::MaterialDescriptorDB &descriptor_db,
                             const hgl::graph::mtl::UBODescriptorSemantic semantic,
                             const uint32_t stage_bits)
{
    if(!descriptor_db.AddUBOStruct(semantic))
        return false;

    const auto &meta=hgl::graph::mtl::GetDescriptorSemanticMeta(semantic);
    auto *ubo=hgl::graph::mtl::CreateUBODescriptor(semantic,stage_bits);
    if(!ubo)
        return false;

    return descriptor_db.AddUBO(stage_bits,meta.set_type,ubo)!=nullptr;
}

static bool ApplyDescriptorSpec(const hgl::graph::ShaderBuildDescriptorSpec *descriptor_spec,
                                const uint32_t stage_bits,
                                hgl::graph::MaterialDescriptorDB &descriptor_db)
{
    if(!descriptor_spec)
        return true;

    for(const auto semantic:descriptor_spec->ubos)
    {
        if(!AddUBOBySemantic(descriptor_db,semantic,stage_bits))
            return false;
    }

    for(const auto semantic:descriptor_spec->ssbos)
    {
        if(!AddSSBOBySemantic(descriptor_db,semantic,stage_bits))
            return false;
    }

    if(descriptor_spec->material_instance_bytes>0)
    {
        if(!AddSSBOBySemantic(descriptor_db,
                              hgl::graph::mtl::SSBODescriptorSemantic::MaterialBindingInstanceData,
                              stage_bits))
            return false;
    }

    return true;
}

static bool ApplySSBOOverrides(const hgl::graph::mtl::MaterialCreateConfig &config,
                               hgl::graph::MaterialDescriptorDB &descriptor_db)
{
    if(config.local_to_world)
    {
        if(!AddSSBOBySemantic(descriptor_db,
                              hgl::graph::mtl::SSBODescriptorSemantic::TransformData,
                              config.shader_stage_flag_bit))
            return false;
    }

    return true;
}
}

namespace hgl::graph
{
ShaderGenResult<ShaderBuildResult> ShaderBuildPipeline::Build(const mtl::MaterialCreateConfig &config,
                                                              const mtl::contract::PhysicalDeviceProfileLite *profile,
                                                              const ShaderBuildDescriptorSpec *descriptor_spec)
{
    ShaderGenResult<ShaderBuildResult> result{};

    ShaderBuildState state=ShaderBuildState::Empty;

    if(config.shader_stage_flag_bit==0)
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Vertex,
                                      "ShaderBuildPipeline",
                                      "shader_stage_flag_bit is zero"});
        return result;
    }

    state=ShaderBuildState::ConfigValidated;

    if(!profile)
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Vertex,
                                      "ShaderBuildPipeline",
                                      "physical device profile is null"});
        return result;
    }

    if(config.material_instance && (!descriptor_spec || descriptor_spec->material_instance_bytes==0))
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Vertex,
                                      "ShaderBuildPipeline.SSBO.MaterialInstance",
                                      "material_instance descriptor path is not aligned yet"});
        return result;
    }

    if(config.texture_source_bits_override!=0 && config.sampler_feature_bits_override==0)
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Fragment,
                                      "ShaderBuildPipeline.TextureSampler",
                                      "texture source override requires sampler slot override"});
        return result;
    }

    MaterialDescriptorDB descriptor_db;
    mtl::DescriptorBindingSlots binding_contract{};

    if(!ApplyDescriptorSpec(descriptor_spec,config.shader_stage_flag_bit,descriptor_db))
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Vertex,
                                      "ShaderBuildPipeline.DescriptorSpec",
                                      "failed to apply descriptor spec"});
        return result;
    }

    if(!ApplySSBOOverrides(config,descriptor_db))
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Vertex,
                                      "ShaderBuildPipeline.SSBO",
                                      "failed to apply SSBO overrides"});
        return result;
    }

    if(!ApplyTextureSamplerOverrides(config,descriptor_db))
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Fragment,
                                      "ShaderBuildPipeline.TextureSampler",
                                      "failed to apply texture sampler overrides"});
        return result;
    }

    mtl::DescriptorLayoutBuilder::Finalize(descriptor_db,binding_contract);

    result.value.binding_contract=binding_contract;
    result.value.descriptor_count=descriptor_db.GetCount();
    result.value.layout_finalized=true;

    state=ShaderBuildState::DescriptorLayoutFinalized;

    ShaderCompileRequest request{};
    request.profile=*profile;
    request.vulkan_version=0;
    request.spv_version=0;

    if(config.shader_stage_flag_bit&uint32_t(ShaderStage::Vertex))
    {
        request.stage=ShaderStage::Vertex;
        request.source="#version 450\nvoid main(){}\n";
    }
    else
    if(config.shader_stage_flag_bit&uint32_t(ShaderStage::Fragment))
    {
        request.stage=ShaderStage::Fragment;
        request.source="#version 450\nlayout(location=0) out vec4 outColor;\nvoid main(){outColor=vec4(1.0);}\n";
    }

    if(request.source.empty())
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidShaderStage,
                                      ShaderStage::Vertex,
                                      "ShaderBuildPipeline",
                                      "no supported stage found for minimal pipeline"});
        return result;
    }

    state=ShaderBuildState::SourceGenerated;

    ShaderCompilerContext compiler(*profile);
    ShaderGenResult<ShaderBinary> compile_result=compiler.Compile(request);

    if(!compile_result.success)
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.insert(result.diagnostics.end(),compile_result.diagnostics.begin(),compile_result.diagnostics.end());
        return result;
    }

    state=ShaderBuildState::Compiled;

    result.value.final_state=state;
    result.value.binaries.push_back(std::move(compile_result.value));
    result.success=true;
    return result;
}
}//namespace hgl::graph
