#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialDescriptorManager.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDescriptorSet.h>

VK_NAMESPACE_BEGIN
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

bool MaterialParameters::BindUBO(const int &index,DeviceBuffer *ubo,bool dynamic)
{
    if(index<0||!ubo)
        return(false);

    if(!descriptor_set->BindUBO(index,ubo,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindUBO(const AnsiString &name,DeviceBuffer *ubo,bool dynamic)
{
    if(name.IsEmpty()||!ubo)
        return(false);

    const int index=desc_manager->GetUBO(set_type,name,dynamic);

    if(index<0)
        return(false);

    if(!descriptor_set->BindUBO(index,ubo,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindSSBO(const int &index,DeviceBuffer *ssbo,bool dynamic)
{
    if(index<0||!ssbo)
        return(false);

    if(!descriptor_set->BindSSBO(index,ssbo,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindSSBO(const AnsiString &name,DeviceBuffer *ssbo,bool dynamic)
{
    if(name.IsEmpty()||!ssbo)
        return(false);

    const int index=desc_manager->GetSSBO(set_type,name,dynamic);

    if(index<0)
        return(false);

    if(!descriptor_set->BindSSBO(index,ssbo,dynamic))
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
VK_NAMESPACE_END
