#include"VKMaterial.h"
#include"VKDescriptorSets.h"
#include"VKShader.h"
#include"VKVertexAttributeBinding.h"
VK_NAMESPACE_BEGIN
Material::~Material()
{
    delete dsl_creater;
    delete shader;
}

MaterialInstance *Material::CreateInstance()
{
    VertexAttributeBinding *vab=shader->CreateVertexAttributeBinding();

    return(new MaterialInstance(this,vab));
}

MaterialInstance::~MaterialInstance()
{
    delete vab;
}
VK_NAMESPACE_END
