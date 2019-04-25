#include"VKMaterial.h"
#include"VKDescriptorSets.h"
#include"VKShader.h"
VK_NAMESPACE_BEGIN
Material::~Material()
{
    delete dsl_creater;
    delete shader;
}

MaterialInstance *Material::CreateInstance()
{
    return(nullptr);
}
VK_NAMESPACE_END
