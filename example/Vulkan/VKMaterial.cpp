#include"VKMaterial.h"
#include"VKDescriptorSets.h"
#include"VKShader.h"
#include"VKVertexInput.h"
VK_NAMESPACE_BEGIN
Material::~Material()
{
    delete dsl_creater;
    delete shader;
}

MaterialInstance *Material::CreateInstance()
{
    VertexInputStateInstance *vis_instance=vis->CreateInstance();

    return(new MaterialInstance(this,vis_instance));
}

MaterialInstance::~MaterialInstance()
{
    delete vis_instance;
}
VK_NAMESPACE_END
