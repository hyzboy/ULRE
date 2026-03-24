#include<hgl/vk/VKShaderDescriptorSet.h>

namespace hgl::graph{

template<typename TDesc>
static TDesc *FinalizeInsert(ShaderDescriptorSet *owner,const uint32_t ssb,TDesc *new_sd)
{
    if(!owner||!new_sd)
        return nullptr;

    new_sd->set_type=owner->set_type;
    new_sd->stage_flag=ssb;
    ++owner->count;

    return new_sd;
}

/**
* 添加UBO描述符
*/
UBODescriptor *ShaderDescriptorSet::AddUBO(uint32_t ssb,UBODescriptor *new_sd)
{
    if(!new_sd)
        return nullptr;

    const auto exist_ubo_iter=ubo_descriptor_map.find(new_sd->semantic);
    if(exist_ubo_iter!=ubo_descriptor_map.end()&&exist_ubo_iter->second)
    {
        delete new_sd;
        exist_ubo_iter->second->stage_flag|=ssb;
        return exist_ubo_iter->second;
    }

    ubo_descriptor_map.emplace(new_sd->semantic,new_sd);

    return FinalizeInsert(this,ssb,new_sd);
}

/**
* 添加SSBO描述符
*/
SSBODescriptor *ShaderDescriptorSet::AddSSBO(uint32_t ssb,SSBODescriptor *new_sd)
{
    if(!new_sd)
        return nullptr;

    const auto exist_ssbo_iter=ssbo_descriptor_map.find(new_sd->semantic);
    if(exist_ssbo_iter!=ssbo_descriptor_map.end()&&exist_ssbo_iter->second)
    {
        delete new_sd;
        exist_ssbo_iter->second->stage_flag|=ssb;
        return exist_ssbo_iter->second;
    }

    ssbo_descriptor_map.emplace(new_sd->semantic,new_sd);

    return FinalizeInsert(this,ssb,new_sd);
}

/**
* 添加纹理描述符
*/
TextureDescriptor *ShaderDescriptorSet::AddTexture(uint32_t ssb,TextureDescriptor *new_sd)
{
    if(!new_sd)
        return nullptr;

    const auto exist_tex_iter=texture_descriptor_map.find(new_sd->slot);
    if(exist_tex_iter!=texture_descriptor_map.end()&&exist_tex_iter->second)
    {
        delete new_sd;
        exist_tex_iter->second->stage_flag|=ssb;
        return exist_tex_iter->second;
    }

    texture_descriptor_map.emplace(new_sd->slot,new_sd);

    return FinalizeInsert(this,ssb,new_sd);
}

/**
* 添加纹理采样器描述符
*/
TextureSamplerDescriptor *ShaderDescriptorSet::AddTextureSampler(uint32_t ssb,TextureSamplerDescriptor *new_sd)
{
    if(!new_sd)
        return nullptr;

    const auto exist_sampler_iter=texture_sampler_descriptor_map.find(new_sd->slot);
    if(exist_sampler_iter!=texture_sampler_descriptor_map.end()&&exist_sampler_iter->second)
    {
        delete new_sd;
        exist_sampler_iter->second->stage_flag|=ssb;
        return exist_sampler_iter->second;
    }

    texture_sampler_descriptor_map.emplace(new_sd->slot,new_sd);

    return FinalizeInsert(this,ssb,new_sd);
}
}//namespace hgl::graph
