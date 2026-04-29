#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/vk/VKBufferOwner.h>

namespace hgl::graph{
MaterialParameters::MaterialParameters(const MaterialDescriptorManager *mdm,const DescriptorSetType &type,DescriptorSet *ds)
{
    desc_manager=mdm;
    set_type=type;
    descriptor_set=ds;
}

MaterialParameters::~MaterialParameters()
{
    delete descriptor_set;
}

bool MaterialParameters::BindUBO(const int &index,const IGPUBuffer *gpu,bool dynamic)
{
    if(index<0||!gpu)
        return(false);

    if(!descriptor_set->BindUBO(index,gpu,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindUBO(const mtl::UBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic)
{
    if(!gpu)
        return(false);

    const int index=desc_manager->GetUBO(set_type,semantic,dynamic);

    if(index<0)
        return(false);

    if(!descriptor_set->BindUBO(index,gpu,dynamic))
        return(false);

    return(true);
}


bool MaterialParameters::BindSSBO(const int &index,const IGPUBuffer *gpu,bool dynamic)
{
    if(index<0||!gpu)
        return(false);

    if(!descriptor_set->BindSSBO(index,gpu,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindSSBO(const mtl::SSBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic)
{
    if(!gpu)
        return(false);

    const int index=desc_manager->GetSSBO(set_type,semantic,dynamic);

    if(index<0)
        return(false);

    if(!descriptor_set->BindSSBO(index,gpu,dynamic))
        return(false);

    return(true);
}


bool MaterialParameters::BindTexture(const int &index,Texture *tex)
{
    if(index < 0 || !tex)
        return(false);

    if(!descriptor_set->BindTexture(index,tex))
        return(false);

    return(true);
}

bool MaterialParameters::BindTexture(const mtl::SamplerSlot slot,Texture *tex)
{
    if(!tex)
        return(false);

    const int index=desc_manager->GetTexture(set_type,slot);

    if(index<0)
        return(false);

    if(!descriptor_set->BindTexture(index,tex))
        return(false);

    return(true);
}


bool MaterialParameters::BindResourceSampler(const int &index,Texture *tex,Sampler *sampler)
{
        LogInfo(u8"[VKShaderMaterialProgramParameters] BindResourceSampler index=%d set_type=%u tex=%p sampler=%p descriptor_set=%p",
            index,
            (uint)set_type,
            (void*)tex,
            (void*)sampler,
            (void*)descriptor_set);

    if(index<0||!tex||!sampler)
        return(false);

    if(!descriptor_set->BindResourceSampler(index,tex,sampler))
        return(false);

    return(true);
}

bool MaterialParameters::BindResourceSampler(const mtl::SamplerSlot slot,Texture *tex,Sampler *sampler)
{
    if(!tex||!sampler)
    {
        std::fprintf(stderr,
            "[MaterialParameters] BindResourceSampler(slot=%u) FAILED: tex=%p sampler=%p (null pointer)\n",
            (uint)slot, (void*)tex, (void*)sampler);
        return(false);
    }

    const int index=desc_manager->GetTextureSampler(set_type,slot);

    std::fprintf(stderr,
        "[MaterialParameters] BindResourceSampler slot=%u index=%d set_type=%u tex=%p sampler=%p descriptor_set=%p\n",
        (uint)slot,
        index,
        (uint)set_type,
        (void*)tex,
        (void*)sampler,
        (void*)descriptor_set);

    if(index<0)
    {
        std::fprintf(stderr,
            "[MaterialParameters] BindResourceSampler FAILED: slot=%u not found in set_type=%u (index=%d)\n",
            (uint)slot, (uint)set_type, index);
        return(false);
    }

    if(!descriptor_set->BindResourceSampler(index,tex,sampler))
    {
        std::fprintf(stderr,
            "[MaterialParameters] BindResourceSampler FAILED: descriptor_set->BindResourceSampler index=%d returned false\n",
            index);
        return(false);
    }

    return(true);
}


bool MaterialParameters::BindInputAttachment(const int &index,ImageView *iv)
{
    if(index<0||!iv)
        return(false);

    if(!descriptor_set->BindInputAttachment(index,iv))
        return(false);

    return(true);
}


void MaterialParameters::Update()
{
    descriptor_set->Update();
}
}//namespace hgl::graph
