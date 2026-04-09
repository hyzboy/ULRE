#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/vk/VKBuffer.h>
#include<cstdio>

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
    {
        std::fprintf(stderr,
            "[MaterialParameters::BindSSBO(index)] FAILED: invalid args index=%d gpu=0x%llX dynamic=%d set_type=%u descriptor_set=0x%llX\n",
            index,
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(gpu)),
            dynamic ? 1 : 0,
            static_cast<unsigned>(set_type),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(descriptor_set)));
        return(false);
    }

    if(!descriptor_set->BindSSBO(index,gpu,dynamic))
    {
        std::fprintf(stderr,
            "[MaterialParameters::BindSSBO(index)] FAILED: descriptor_set->BindSSBO returned false index=%d gpu=0x%llX dynamic=%d set_type=%u descriptor_set=0x%llX\n",
            index,
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(gpu)),
            dynamic ? 1 : 0,
            static_cast<unsigned>(set_type),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(descriptor_set)));
        return(false);
    }

    return(true);
}

bool MaterialParameters::BindSSBO(const mtl::SSBODescriptorSemantic semantic,const IGPUBuffer *gpu,bool dynamic)
{
    if(!gpu)
    {
        std::fprintf(stderr,
            "[MaterialParameters::BindSSBO(semantic)] FAILED: gpu=null semantic=%u dynamic=%d set_type=%u descriptor_set=0x%llX\n",
            static_cast<unsigned>(semantic),
            dynamic ? 1 : 0,
            static_cast<unsigned>(set_type),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(descriptor_set)));
        return(false);
    }

    const int index=desc_manager->GetSSBO(set_type,semantic,dynamic);

    if(index<0)
    {
        std::fprintf(stderr,
            "[MaterialParameters::BindSSBO(semantic)] FAILED: no SSBO binding semantic=%u dynamic=%d set_type=%u descriptor_set=0x%llX\n",
            static_cast<unsigned>(semantic),
            dynamic ? 1 : 0,
            static_cast<unsigned>(set_type),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(descriptor_set)));
        return(false);
    }

    if(!descriptor_set->BindSSBO(index,gpu,dynamic))
    {
        std::fprintf(stderr,
            "[MaterialParameters::BindSSBO(semantic)] FAILED: descriptor_set->BindSSBO returned false semantic=%u index=%d dynamic=%d set_type=%u descriptor_set=0x%llX gpu=0x%llX\n",
            static_cast<unsigned>(semantic),
            index,
            dynamic ? 1 : 0,
            static_cast<unsigned>(set_type),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(descriptor_set)),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(gpu)));
        return(false);
    }

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


bool MaterialParameters::BindTextureSampler(const int &index,Texture *tex,Sampler *sampler)
{
        LogInfo(u8"[VKMaterialParameters] BindTextureSampler index=%d set_type=%u tex=%p sampler=%p descriptor_set=%p",
            index,
            (uint)set_type,
            (void*)tex,
            (void*)sampler,
            (void*)descriptor_set);

    if(index<0||!tex||!sampler)
        return(false);

    if(!descriptor_set->BindTextureSampler(index,tex,sampler))
        return(false);

    return(true);
}

bool MaterialParameters::BindTextureSampler(const mtl::SamplerSlot slot,Texture *tex,Sampler *sampler)
{
    if(!tex||!sampler)
        return(false);

    const int index=desc_manager->GetTextureSampler(set_type,slot);

    LogInfo(u8"[VKMaterialParameters] BindTextureSampler slot=%u index=%d set_type=%u tex=%p sampler=%p descriptor_set=%p",
            (uint)slot,
            index,
            (uint)set_type,
            (void*)tex,
            (void*)sampler,
            (void*)descriptor_set);

    if(index<0)
        return(false);

    if(!descriptor_set->BindTextureSampler(index,tex,sampler))
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
