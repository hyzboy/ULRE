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
UBODescriptor *ShaderDescriptorSet::AddUBO(uint32_t ssb,std::unique_ptr<UBODescriptor> new_sd)
{
    if(!new_sd)
        return nullptr;

    const size_t index=size_t(new_sd->semantic);
    if(index>=mtl::UBODescriptorSemanticCount)
        return nullptr;

    if(ubo_descriptor_map[index])
    {
        ubo_descriptor_map[index]->stage_flag|=ssb;
        return ubo_descriptor_map[index];
    }

    UBODescriptor *raw=new_sd.get();
    ubo_descriptor_map[index]=raw;
    FinalizeInsert(this,ssb,raw);
    new_sd.release();
    return raw;
}

/**
* 添加SSBO描述符
*/
SSBODescriptor *ShaderDescriptorSet::AddSSBO(uint32_t ssb,std::unique_ptr<SSBODescriptor> new_sd)
{
    if(!new_sd)
        return nullptr;

    const size_t index=size_t(new_sd->semantic);
    const size_t max_index =
        (set_type==DescriptorSetType::VertexStreams)
            ? SHADER_DESCRIPTOR_SSBO_SLOT_COUNT
            : mtl::SSBODescriptorSemanticCount;

    if(index>=max_index)
        return nullptr;

    if(ssbo_descriptor_map[index])
    {
        ssbo_descriptor_map[index]->stage_flag|=ssb;
        return ssbo_descriptor_map[index];
    }

    SSBODescriptor *raw=new_sd.get();
    ssbo_descriptor_map[index]=raw;
    FinalizeInsert(this,ssb,raw);
    new_sd.release();
    return raw;
}

/**
* 添加纹理描述符
*/
TextureDescriptor *ShaderDescriptorSet::AddTexture(uint32_t ssb,std::unique_ptr<TextureDescriptor> new_sd)
{
    if(!new_sd)
        return nullptr;

    const size_t index=size_t(new_sd->slot);
    if(index>=mtl::SamplerSlotCount)
        return nullptr;

    if(texture_descriptor_map[index])
    {
        texture_descriptor_map[index]->stage_flag|=ssb;
        return texture_descriptor_map[index];
    }

    TextureDescriptor *raw=new_sd.get();
    texture_descriptor_map[index]=raw;
    FinalizeInsert(this,ssb,raw);
    new_sd.release();
    return raw;
}

/**
* 添加纹理采样器描述符
*/
TextureSamplerDescriptor *ShaderDescriptorSet::AddTextureSampler(uint32_t ssb,std::unique_ptr<TextureSamplerDescriptor> new_sd)
{
    if(!new_sd)
        return nullptr;

    const size_t index=size_t(new_sd->slot);
    if(index>=mtl::SamplerSlotCount)
        return nullptr;

    if(texture_sampler_descriptor_map[index])
    {
        texture_sampler_descriptor_map[index]->stage_flag|=ssb;
        return texture_sampler_descriptor_map[index];
    }

    TextureSamplerDescriptor *raw=new_sd.get();
    texture_sampler_descriptor_map[index]=raw;
    FinalizeInsert(this,ssb,raw);
    new_sd.release();
    return raw;
}
}//namespace hgl::graph
