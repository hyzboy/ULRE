#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKImageView.h>
#include<hgl/graph/vulkan/VKMemory.h>
VK_NAMESPACE_BEGIN
Texture::~Texture()
{
    if(!data)return;

    if(data->image_view)
        delete data->image_view;

    if(data->ref)return;

    if(data->image)
        vkDestroyImage(device,data->image,nullptr);

    if(data->memory)
        delete data->memory;
}
VK_NAMESPACE_END
