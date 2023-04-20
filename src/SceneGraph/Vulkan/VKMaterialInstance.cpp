#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKShaderModule.h>

VK_NAMESPACE_BEGIN
MaterialInstance *GPUDevice::CreateMI(Material *mtl,const VILConfig *vil_cfg)
{
    if(!mtl)return(nullptr);

    VIL *vil=mtl->CreateVIL(vil_cfg);

    if(!vil)return(nullptr);

    return(new MaterialInstance(mtl,vil));
}

MaterialInstance::MaterialInstance(Material *mtl,VIL *v)
{
    material=mtl;

    vil=v;

    mp_per_mi=mtl->GetMP(DescriptorSetType::PerMaterial);

    /*
        由于PerMaterial的属性每个MaterialInstance不一样，
        所以理论上需要在这里分配属于它自己的MP做绑定记录

    */
}

bool MaterialInstance::BindUBO(const DescriptorSetType &type,const AnsiString &name,DeviceBuffer *ubo,bool dynamic)
{
    MaterialParameters *mp=GetMP(type);
        
    if(!mp)
        return(false);

    if(!mp->BindUBO(name,ubo,dynamic))return(false);

    mp->Update();
    return(true);
}

bool MaterialInstance::BindSSBO(const DescriptorSetType &type,const AnsiString &name,DeviceBuffer *ubo,bool dynamic)
{
    MaterialParameters *mp=GetMP(type);
        
    if(!mp)
        return(false);

    if(!mp->BindSSBO(name,ubo,dynamic))return(false);

    mp->Update();
    return(true);
}

bool MaterialInstance::BindImageSampler(const DescriptorSetType &type,const AnsiString &name,Texture *tex,Sampler *sampler)
{
    MaterialParameters *mp=GetMP(type);
        
    if(!mp)
        return(false);

    if(!mp->BindImageSampler(name,tex,sampler))return(false);

    mp->Update();
    return(true);
}
VK_NAMESPACE_END
