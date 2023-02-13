#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialDescriptorSets.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDescriptorSet.h>

VK_NAMESPACE_BEGIN
MaterialParameters::MaterialParameters(const MaterialDescriptorSets *_mds,const DescriptorSetsType &type,DescriptorSet *ds)
{
    mds=_mds;
    ds_type=type;
    descriptor_sets=ds;
}

MaterialParameters::~MaterialParameters()
{
    delete descriptor_sets;
}

bool MaterialParameters::BindUBO(const AnsiString &name,DeviceBuffer *ubo,bool dynamic)
{
    if(name.IsEmpty()||!ubo)
        return(false);

    const int index=mds->GetUBO(name,dynamic);

    if(index<0)
        return(false);

    if(!descriptor_sets->BindUBO(index,ubo,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindSSBO(const AnsiString &name,DeviceBuffer *ssbo,bool dynamic)
{
    if(name.IsEmpty()||!ssbo)
        return(false);

    const int index=mds->GetSSBO(name,dynamic);

    if(index<0)
        return(false);

    if(!descriptor_sets->BindSSBO(index,ssbo,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindSampler(const AnsiString &name,Texture *tex,Sampler *sampler)
{
    if(name.IsEmpty()||!tex||!sampler)
        return(false);

    const int index=mds->GetSampler(name);

    if(index<0)
        return(false);

    if(!descriptor_sets->BindSampler(index,tex,sampler))
        return(false);

    return(true);
}

bool MaterialParameters::BindInputAttachment(const AnsiString &name,ImageView *iv)
{
    if(name.IsEmpty()||!iv)
        return(false);

    const int index=mds->GetAttachment(name);

    if(index<0)
        return(false);

    if(!descriptor_sets->BindInputAttachment(index,iv))
        return(false);

    return(true);
}

void MaterialParameters::Update()
{
    descriptor_sets->Update();
}
VK_NAMESPACE_END