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

    const size_t index=size_t(new_sd->semantic);
    if(index>=mtl::UBODescriptorSemanticCount)
    {
        delete new_sd;
        return nullptr;
    }

    if(ubo_descriptor_map[index])
    {
        delete new_sd;
        ubo_descriptor_map[index]->stage_flag|=ssb;
        return ubo_descriptor_map[index];
    }

    ubo_descriptor_map[index]=new_sd;
    return FinalizeInsert(this,ssb,new_sd);
}

/**
* 添加SSBO描述符
*/
SSBODescriptor *ShaderDescriptorSet::AddSSBO(uint32_t ssb,SSBODescriptor *new_sd)
{
    if(!new_sd)
        return nullptr;

    const size_t index=size_t(new_sd->semantic);
    const size_t max_index =
        (set_type==DescriptorSetType::VertexStreams)
            ? SHADER_DESCRIPTOR_SSBO_SLOT_COUNT
            : mtl::SSBODescriptorSemanticCount;

    if(index>=max_index)
    {
        delete new_sd;
        return nullptr;
    }

    if(ssbo_descriptor_map[index])
    {
        delete new_sd;
        ssbo_descriptor_map[index]->stage_flag|=ssb;
        return ssbo_descriptor_map[index];
    }

    ssbo_descriptor_map[index]=new_sd;
    return FinalizeInsert(this,ssb,new_sd);
}

/**
* 添加纹理描述符
*/
TextureDescriptor *ShaderDescriptorSet::AddTexture(uint32_t ssb,TextureDescriptor *new_sd)
{
    if(!new_sd)
        return nullptr;

    const size_t index=size_t(new_sd->slot);
    if(index>=mtl::SamplerSlotCount)
    {
        delete new_sd;
        return nullptr;
    }

    if(texture_descriptor_map[index])
    {
        delete new_sd;
        texture_descriptor_map[index]->stage_flag|=ssb;
        return texture_descriptor_map[index];
    }

    texture_descriptor_map[index]=new_sd;
    return FinalizeInsert(this,ssb,new_sd);
}

/**
* 添加纹理采样器描述符
*/
TextureSamplerDescriptor *ShaderDescriptorSet::AddTextureSampler(uint32_t ssb,TextureSamplerDescriptor *new_sd)
{
    if(!new_sd)
        return nullptr;

    const size_t index=size_t(new_sd->slot);
    if(index>=mtl::SamplerSlotCount)
    {
        delete new_sd;
        return nullptr;
    }

    if(texture_sampler_descriptor_map[index])
    {
        delete new_sd;
        texture_sampler_descriptor_map[index]->stage_flag|=ssb;
        return texture_sampler_descriptor_map[index];
    }

    texture_sampler_descriptor_map[index]=new_sd;
    return FinalizeInsert(this,ssb,new_sd);
}
}//namespace hgl::graph
