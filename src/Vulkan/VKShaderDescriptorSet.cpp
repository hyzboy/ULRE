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

    UBODescriptor *exist_ubo=nullptr;

    if(ubo_descriptor_map.Get(new_sd->semantic,exist_ubo)
    && exist_ubo)
    {
        delete new_sd;
        exist_ubo->stage_flag|=ssb;
        return exist_ubo;
    }

    ubo_descriptor_map.Add(new_sd->semantic,new_sd);

    return FinalizeInsert(this,ssb,new_sd);
}

/**
* 添加SSBO描述符
*/
SSBODescriptor *ShaderDescriptorSet::AddSSBO(uint32_t ssb,SSBODescriptor *new_sd)
{
    if(!new_sd)
        return nullptr;

    SSBODescriptor *exist_ssbo=nullptr;

    if(ssbo_descriptor_map.Get(new_sd->semantic,exist_ssbo)
    && exist_ssbo)
    {
        delete new_sd;
        exist_ssbo->stage_flag|=ssb;
        return exist_ssbo;
    }

    ssbo_descriptor_map.Add(new_sd->semantic,new_sd);

    return FinalizeInsert(this,ssb,new_sd);
}

/**
* 添加纹理描述符
*/
TextureDescriptor *ShaderDescriptorSet::AddTexture(uint32_t ssb,TextureDescriptor *new_sd)
{
    if(!new_sd)
        return nullptr;

    TextureDescriptor *exist_sd=nullptr;

    if(texture_descriptor_map.Get(new_sd->slot,exist_sd)&&exist_sd)
    {
        delete new_sd;
        exist_sd->stage_flag|=ssb;
        return exist_sd;
    }

    texture_descriptor_map.Add(new_sd->slot,new_sd);

    return FinalizeInsert(this,ssb,new_sd);
}

/**
* 添加纹理采样器描述符
*/
TextureSamplerDescriptor *ShaderDescriptorSet::AddTextureSampler(uint32_t ssb,TextureSamplerDescriptor *new_sd)
{
    if(!new_sd)
        return nullptr;

    TextureSamplerDescriptor *exist_sd=nullptr;

    if(texture_sampler_descriptor_map.Get(new_sd->slot,exist_sd)&&exist_sd)
    {
        delete new_sd;
        exist_sd->stage_flag|=ssb;
        return exist_sd;
    }

    texture_sampler_descriptor_map.Add(new_sd->slot,new_sd);

    return FinalizeInsert(this,ssb,new_sd);
}
}//namespace hgl::graph
