#include"VKMaterial.h"
VK_NAMESPACE_BEGIN
Material::~Material()
{
    delete dsl_creater;
    delete shader;
}

MaterialInstance *Material::CreateInstance()
{
}
VK_NAMESPACE_END
