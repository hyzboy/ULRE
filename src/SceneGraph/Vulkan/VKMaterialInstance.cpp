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

    if(!mp)
    {
        delete vab;
        return nullptr;
    }

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
VK_NAMESPACE_END
