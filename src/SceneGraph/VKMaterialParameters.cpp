#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/vk/VKBuffer.h>

namespace hgl::graph{
MaterialParameters::MaterialParameters(const MaterialDescriptorManager *mdm,const DescriptorSetType &type,DescriptorSet *ds)
{
    fprintf(stderr, "[DIAG] MaterialParameters ctor type=%d ds=%p\n", (int)type, (void*)ds);
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

bool MaterialParameters::BindUBO(const AnsiString &name,const IGPUBuffer *gpu,bool dynamic)
{
    if(name.IsEmpty()||!gpu)
        return(false);

    const int index=desc_manager->GetUBO(set_type,name,dynamic);

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

bool MaterialParameters::BindSSBO(const AnsiString &name,const IGPUBuffer *gpu,bool dynamic)
{
    if(name.IsEmpty()||!gpu)
        return(false);

    const int index=desc_manager->GetSSBO(set_type,name,dynamic);

    fprintf(stderr, "[DIAG] MP::BindSSBO(gpu) this=%p type=%d name=%s index=%d ds=%p\n",
            (void*)this, (int)set_type, name.c_str(), index, (void*)descriptor_set);

    if(index<0)
        return(false);

    if(!descriptor_set->BindSSBO(index,gpu,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindSSBO(const AnsiString &name,const VkBuffer buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic)
{
    if(name.IsEmpty()||!buf)
        return(false);

    const int index=desc_manager->GetSSBO(set_type,name,dynamic);

    if(index<0)
    {
        fprintf(stderr, "[DIAG] MaterialParameters::BindSSBO name=%s index=-1 (NOT FOUND)\n", name.c_str());
        return(false);
    }
    fprintf(stderr, "[DIAG] MaterialParameters::BindSSBO name=%s index=%d\n", name.c_str(), index);

    if(!descriptor_set->BindSSBO(index,buf,offset,range,dynamic))
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

bool MaterialParameters::BindTexture(const AnsiString &name,Texture *tex)
{
    if(name.IsEmpty() || !tex)
        return(false);

    const int index = desc_manager->GetTexture(set_type,name);

    if(index < 0)
        return(false);

    if(!descriptor_set->BindTexture(index,tex))
        return(false);

    return(true);
}

bool MaterialParameters::BindTextureSampler(const int &index,Texture *tex,Sampler *sampler)
{
    if(index<0||!tex||!sampler)
        return(false);

    if(!descriptor_set->BindTextureSampler(index,tex,sampler))
        return(false);

    return(true);
}

bool MaterialParameters::BindTextureSampler(const AnsiString &name,Texture *tex,Sampler *sampler)
{
    if(name.IsEmpty()||!tex||!sampler)
        return(false);

    const int index=desc_manager->GetTextureSampler(set_type,name);

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

bool MaterialParameters::BindInputAttachment(const AnsiString &name,ImageView *iv)
{
    if(name.IsEmpty()||!iv)
        return(false);

    const int index=desc_manager->GetInputAttachment(set_type,name);

    if(index<0)
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
