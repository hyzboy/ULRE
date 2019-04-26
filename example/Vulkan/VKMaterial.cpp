#include"VKMaterial.h"
#include"VKDescriptorSets.h"
#include"VKShader.h"
#include"VKVertexAttributeBinding.h"
VK_NAMESPACE_BEGIN
Material::~Material()
{
    delete shader;
}

MaterialInstance *Material::CreateInstance()
{
    return(new MaterialInstance(this));
}

MaterialInstance::MaterialInstance(Material *m)
{
    mat=m;
    vab=->CreateVertexAttributeBinding();
}

MaterialInstance::~MaterialInstance()
{
    delete vab;
    delete dsl_creater;
}
VK_NAMESPACE_END
