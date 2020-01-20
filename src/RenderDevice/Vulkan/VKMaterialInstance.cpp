#include<hgl/graph/vulkan/VKMaterialInstance.h>

#include<hgl/graph/vulkan/VKMaterial.h>
#include<hgl/graph/vulkan/VKDescriptorSets.h>

VK_NAMESPACE_BEGIN
MaterialInstance::MaterialInstance(Material *m,DescriptorSets *ds)
{
    material=m;
    descriptor_sets=ds;
}

MaterialInstance::~MaterialInstance()
{
    delete descriptor_sets;
}

bool MaterialInstance::BindUBO(const UTF8String &name,vulkan::Buffer *ubo)
{
    if(name.IsEmpty()||!ubo)
        return(false);

    const int index=material->GetUBO(name);

    if(index<0)
        return(false);

    if(!descriptor_sets->BindUBO(index,ubo))
        return(false);

    return(true);
}

bool MaterialInstance::BindSampler(const UTF8String &name,Texture *tex,Sampler *sampler)
{
    if(name.IsEmpty()||!tex||!sampler)
        return(false);

    const int index=material->GetSampler(name);

    if(index<0)
        return(false);

    if(!descriptor_sets->BindSampler(index,tex,sampler))
        return(false);

    return(true);
}

void MaterialInstance::Update()
{
    descriptor_sets->Update();
}

MaterialInstance *Material::CreateInstance()
{
    DescriptorSets *ds=CreateDescriptorSets();

    if(!ds)return(nullptr);

    return(new MaterialInstance(this,ds));
}
VK_NAMESPACE_END