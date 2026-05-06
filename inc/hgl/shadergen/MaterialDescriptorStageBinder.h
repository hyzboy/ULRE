#pragma once

#include<hgl/shadergen/MaterialDescriptorDB.h>
#include<hgl/shadergen/ShaderStageMap.h>
#include<hgl/common/TextureSamplerTypeDef.h>
#include<string>

namespace hgl::graph::mtl
{
class MaterialDescriptorStageBinder
{
public:
    static bool AddResolvedUBO(MaterialDescriptorDB &descriptor_db,ShaderStageMap &shader_map,const uint32_t flag_bits,const DescriptorSetType &set_type,const UBODescriptorSemantic semantic,const std::string &struct_name,const std::string &name);
    static bool AddResolvedSSBO(MaterialDescriptorDB &descriptor_db,ShaderStageMap &shader_map,const uint32_t flag_bits,const DescriptorSetType &set_type,const SSBODescriptorSemantic semantic,const std::string &struct_name,const std::string &name);
    static bool AddTexture(MaterialDescriptorDB &descriptor_db,const ShaderStage flag_bit,const TextureType &tt,const SamplerSlot slot);
    static bool AddTextureSampler(MaterialDescriptorDB &descriptor_db,const ShaderStage flag_bit,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint=TextureChannelHint::RGBA);
    static bool AddTextureSampler(MaterialDescriptorDB &descriptor_db,ShaderStageMap &shader_map,const uint32_t flag_bits,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint=TextureChannelHint::RGBA);
};
}//namespace hgl::graph::mtl
