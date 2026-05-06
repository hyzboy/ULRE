#include<hgl/shadergen/MaterialBuilder.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<memory>

using namespace hgl::graph;
using namespace hgl::graph::mtl;

namespace hgl::graph::mtl
{
    MaterialBuilder::MaterialBuilder(const MaterialCreateConfig *config)
        : building_(std::make_unique<MaterialCreateInfo>(config))
    {
    }

    MaterialBuilder::~MaterialBuilder()=default;

    void MaterialBuilder::SetDevice(const contract::PhysicalDeviceProfileLite *profile)
    {
        if(building_)
            building_->SetDevice(profile);
    }

    bool MaterialBuilder::AddUBOStruct(const uint32_t flag_bits,const UBODescriptorSemantic semantic)
    {
        if(!building_)
            return false;
        return building_->AddUBOStruct(flag_bits,semantic);
    }

    bool MaterialBuilder::AddSSBOStruct(const uint32_t flag_bits,const SSBODescriptorSemantic semantic)
    {
        if(!building_)
            return false;
        return building_->AddSSBOStruct(flag_bits,semantic);
    }

    bool MaterialBuilder::AddTexture(const ShaderStage flag_bits,const TextureType &tt,const SamplerSlot slot)
    {
        if(!building_)
            return false;
        return building_->AddTexture(flag_bits,tt,slot);
    }

    bool MaterialBuilder::AddTextureSampler(const ShaderStage flag_bits,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint)
    {
        if(!building_)
            return false;
        return building_->AddTextureSampler(flag_bits,st,slot,channel_hint);
    }

    bool MaterialBuilder::AddTextureSampler(const uint32_t flag_bits,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint)
    {
        if(!building_)
            return false;
        return building_->AddTextureSampler(flag_bits,st,slot,channel_hint);
    }

    bool MaterialBuilder::SetMaterialInstance(const uint32_t data_bytes,const uint32_t shader_stage_flag_bits)
    {
        if(!building_)
            return false;
        return building_->SetMaterialInstance(data_bytes,shader_stage_flag_bits);
    }

    bool MaterialBuilder::SetMaterialInstance(const ShaderDataSchema schema,const ShaderDataSchemaInfo &schema_info,const uint32_t shader_stage_flag_bits)
    {
        if(!building_)
            return false;
        return building_->SetMaterialInstance(schema,schema_info,shader_stage_flag_bits);
    }

    bool MaterialBuilder::SetLocalToWorld(const uint32_t shader_stage_flag_bits)
    {
        if(!building_)
            return false;
        return building_->SetLocalToWorld(shader_stage_flag_bits);
    }

    ShaderCreateInfoVertex *MaterialBuilder::GetVertexShader()
    {
        if(!building_)
            return nullptr;
        return building_->GetVertexShader();
    }

    ShaderCreateInfo *MaterialBuilder::GetStageShader(ShaderStage ss)
    {
        if(!building_)
            return nullptr;
        return building_->GetStageShader(ss);
    }

    void MaterialBuilder::Resort()
    {
        if(building_)
            building_->Resort();
    }

    MaterialCreateInfo *MaterialBuilder::Build()
    {
        if(!building_)
            return nullptr;

        // Sort descriptors and finalize set/binding numbers
        building_->Resort();
        building_->BuildBindingContract();

        // Compile final GLSL to SPV for each shader stage
        const ShaderStageMap &shaders = building_->GetShaderMap();
        for(auto &[stage, sc] : shaders)
        {
            if(sc && !sc->CompileFinalGLSLToSPV())
            {
                return nullptr;
            }
        }

        // Transfer ownership to caller
        return building_.release();
    }

    MaterialCreateInfo *MaterialBuilder::BuildSnapshotOnly()
    {
        if(!building_)
            return nullptr;

        // Sort descriptors and finalize set/binding numbers, but don't compile SPV
        building_->Resort();
        building_->BuildBindingContract();

        // Transfer ownership to caller
        return building_.release();
    }
}
