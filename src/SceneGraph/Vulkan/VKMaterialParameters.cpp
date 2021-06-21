#include<hgl/graph/VKMaterialParameters.h>

#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDescriptorSets.h>

VK_NAMESPACE_BEGIN
MaterialParameters::MaterialParameters(const ShaderModuleMap *smm,const DescriptorSetsType &type,DescriptorSets *ds)
{
    shader_map=smm;
    ds_type=type;
    descriptor_sets=ds;
}

MaterialParameters::~MaterialParameters()
{
    delete descriptor_sets;
}

bool MaterialParameters::BindUBO(const AnsiString &name,GPUBuffer *ubo,bool dynamic)
{
    if(name.IsEmpty()||!ubo)
        return(false);

    const int index=shader_map->GetUBO(name);

    if(index<0)
        return(false);

    if(!descriptor_sets->BindUBO(index,ubo,dynamic))
        return(false);

    return(true);
}

bool MaterialParameters::BindSSBO(const AnsiString &name,GPUBuffer *ssbo,bool dynamic)
{
    if(name.IsEmpty()||!ssbo)
        return(false);

    const int index=shader_map->GetSSBO(name);

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

    const int index=shader_map->GetSampler(name);

    if(index<0)
        return(false);

    if(!descriptor_sets->BindSampler(index,tex,sampler))
        return(false);

    return(true);
}

void MaterialParameters::Update()
{
    descriptor_sets->Update();
}
VK_NAMESPACE_END