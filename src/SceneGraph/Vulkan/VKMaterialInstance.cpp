#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKShaderModule.h>

VK_NAMESPACE_BEGIN
MaterialInstance *GPUDevice::CreateMI(Material *mtl,const VABConfigInfo *vab_cfg)
{
    if(!mtl)return(nullptr);

    VertexShaderModule *vsm=mtl->GetVertexShaderModule();

    VAB *vab=vsm->CreateVAB(vab_cfg);

    if(!vab)return(nullptr);

    MaterialParameters *mp=CreateMP(mtl,DescriptorSetsType::Value);

    return(new MaterialInstance(mtl,vab,mp));
}

MaterialInstance::MaterialInstance(Material *mtl,VAB *v,MaterialParameters *p)
{
    material=mtl;

    vab=v;

    mp_value=p;
}

MaterialInstance::~MaterialInstance()
{
    SAFE_CLEAR(mp_value);
}

MaterialParameters *MaterialInstance::GetMP(const DescriptorSetsType &type)
{
    //if(type==DescriptorSetsType::Texture
    //    return mp_texture;

    if(type==DescriptorSetsType::Value)
        return mp_value;

    return material->GetMP(type);
}

bool MaterialInstance::BindUBO(const DescriptorSetsType &type,const AnsiString &name,GPUBuffer *ubo,bool dynamic)
{
    MaterialParameters *mp_global=GetMP(type);
        
    if(!mp_global)
        return(false);

    if(!mp_global->BindUBO(name,ubo,dynamic))return(false);

    mp_global->Update();
    return(true);
}

bool MaterialInstance::BindSSBO(const DescriptorSetsType &type,const AnsiString &name,GPUBuffer *ubo,bool dynamic)
{
    MaterialParameters *mp_global=GetMP(type);
        
    if(!mp_global)
        return(false);

    if(!mp_global->BindSSBO(name,ubo,dynamic))return(false);

    mp_global->Update();
    return(true);
}

bool MaterialInstance::BindSampler(const DescriptorSetsType &type,const AnsiString &name,Texture *tex,Sampler *sampler)
{
    MaterialParameters *mp_global=GetMP(type);
        
    if(!mp_global)
        return(false);

    if(!mp_global->BindSampler(name,tex,sampler))return(false);

    mp_global->Update();
    return(true);
}
VK_NAMESPACE_END
