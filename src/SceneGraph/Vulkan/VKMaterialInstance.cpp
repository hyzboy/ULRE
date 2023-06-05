#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKShaderModule.h>

VK_NAMESPACE_BEGIN
MaterialInstance *Material::CreateMI(const VILConfig *vil_cfg)
{
    VIL *vil=CreateVIL(vil_cfg);

    if(!vil)return(nullptr);

    return(new MaterialInstance(this,vil));
}

MaterialInstance::MaterialInstance(Material *mtl,VIL *v)
{
    material=mtl;

    vil=v;
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
