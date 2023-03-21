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

bool MaterialParameters::BindUBO(const AnsiString &name,DeviceBuffer *ubo,bool dynamic)
{
    if(name.IsEmpty()||!ubo)
        return(false);

    const int index=desc_manager->GetUBO(name,dynamic);

    if(index<0)
        return(false);

    if(!descriptor_set->BindUBO(index,ubo,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindSSBO(const AnsiString &name,DeviceBuffer *ssbo,bool dynamic)
{
    if(name.IsEmpty()||!ssbo)
        return(false);

    const int index=desc_manager->GetSSBO(name,dynamic);

    if(index<0)
        return(false);

    if(!descriptor_set->BindSSBO(index,ssbo,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindImageSampler(const AnsiString &name,Texture *tex,Sampler *sampler)
{
    if(name.IsEmpty()||!tex||!sampler)
        return(false);

    const int index=desc_manager->GetImageSampler(name);

    if(index<0)
        return(false);

    if(!descriptor_set->BindImageSampler(index,tex,sampler))
        return(false);

    return(true);
}

bool MaterialParameters::BindInputAttachment(const AnsiString &name,ImageView *iv)
{
    if(name.IsEmpty()||!iv)
        return(false);

    const int index=desc_manager->GetInputAttachment(name);

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