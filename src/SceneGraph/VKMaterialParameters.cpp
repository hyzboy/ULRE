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


bool MaterialParameters::BindAttribSSBO(const AttributeSemantic semantic,const IGPUBuffer *gpu,bool dynamic)
{
    return BindSSBO(int(semantic),gpu,dynamic);
}

bool MaterialParameters::BindVertexStreamSSBO(const uint32_t binding,const IGPUBuffer *gpu,bool dynamic)
{
    if(set_type != DescriptorSetType::VertexStreams)
        return false;

    if(binding >= kVertexStreamBindingCount)
        return false;

    if(!desc_manager || !desc_manager->HasBinding(set_type,binding))
    {
        LogWarning(u8"[VKShaderMaterialProgramParameters] BindVertexStreamSSBO skipped: binding=%u is not declared in material='%s' VertexStreams set",
                   binding,
                   desc_manager ? desc_manager->GetMaterialName().c_str() : "<null>");
        return false;
    }

    return BindSSBO(int(binding),gpu,dynamic);
}

bool MaterialParameters::HasBinding(const uint32_t binding)const
{
    return desc_manager&&desc_manager->HasBinding(set_type,binding);
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
        return(false);

    const int index=desc_manager->GetTextureSampler(set_type,slot);

    LogInfo(u8"[VKShaderMaterialProgramParameters] BindResourceSampler slot=%u index=%d set_type=%u tex=%p sampler=%p descriptor_set=%p",
            (uint)slot,
            index,
            (uint)set_type,
            (void*)tex,
            (void*)sampler,
            (void*)descriptor_set);

    if(index<0)
        return(false);

    if(!descriptor_set->BindResourceSampler(index,tex,sampler))
        return(false);

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
