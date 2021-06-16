#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>

VK_NAMESPACE_BEGIN
MaterialInstance *Material::CreateInstance()
{
    MaterialParameters *mp=CreateMP(DescriptorSetsType::Value);

    return(new MaterialInstance(this,mp));
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

MaterialParameters *MaterialInstance::GetMP(const DescriptorSetsType &type)
{
    //if(type==DescriptorSetsType::Texture
    //    return mp_texture;

    if(type==DescriptorSetsType::Value)
        return mp_value;

    return material->GetMP(type);
}
VK_NAMESPACE_END
