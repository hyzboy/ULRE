#include<hgl/shadergen/ShaderBuildPipeline.h>
#include<hgl/shadergen/DescriptorLayoutBuilder.h>
#include<hgl/shadergen/MaterialDescriptorDB.h>

namespace hgl::graph
{
ShaderGenResult<ShaderBuildResult> ShaderBuildPipeline::Build(const mtl::MaterialCreateConfig &config,
                                                              const mtl::contract::PhysicalDeviceProfileLite *profile)
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

    if(config.material_instance)
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Vertex,
                                      "ShaderBuildPipeline.SSBO.MaterialInstance",
                                      "minimal pipeline does not support material_instance yet"});
        return result;
    }

    if(config.local_to_world)
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Vertex,
                                      "ShaderBuildPipeline.SSBO.LocalToWorld",
                                      "minimal pipeline does not support local_to_world yet"});
        return result;
    }

    if(config.sampler_feature_bits_override!=0 || config.texture_source_bits_override!=0)
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Fragment,
                                      "ShaderBuildPipeline.TextureSampler",
                                      "minimal pipeline does not support texture sampler overrides yet"});
        return result;
    }

    MaterialDescriptorDB descriptor_db;
    mtl::DescriptorBindingSlots binding_contract{};

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
