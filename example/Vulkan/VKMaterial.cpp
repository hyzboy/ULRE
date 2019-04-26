#include"VKMaterial.h"
#include"VKDescriptorSets.h"
#include"VKShader.h"
#include"VKVertexAttributeBinding.h"
VK_NAMESPACE_BEGIN
Material::Material(Device *dev,Shader *s)
{
    device=dev;
    shader=s;
    dsl_creater=new DescriptorSetLayoutCreater(dev);
}

Material::~Material()
{
    delete dsl_creater;
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
