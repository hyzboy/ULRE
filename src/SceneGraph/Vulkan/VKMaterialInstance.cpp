#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>

VK_NAMESPACE_BEGIN
MaterialInstance *GPUDevice::CreateMI(Material *mtl)
{
    if(!mtl)return(nullptr);

    MaterialParameters *mp=CreateMP(mtl,DescriptorSetType::Value);

    return(new MaterialInstance(mtl,mp));
}

MaterialInstance::MaterialInstance(Material *mtl,MaterialParameters *p)
{
    material=mtl;

    mp_value=p;
}

MaterialInstance::~MaterialInstance()
{
    SAFE_CLEAR(mp_value);
}

MaterialParameters *MaterialInstance::GetMP(const DescriptorSetType &type)
{
    //if(type==DescriptorSetType::Texture
    //    return mp_texture;

    if(type==DescriptorSetType::Value)
        return mp_value;

    return material->GetMP(type);
}
VK_NAMESPACE_END
