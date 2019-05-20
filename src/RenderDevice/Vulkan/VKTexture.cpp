#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKImageView.h>
VK_NAMESPACE_BEGIN
Texture::~Texture()
{
    if(!data)return;

    if(data->image_view)
        delete data->image_view;

    if(data->image)
        vkDestroyImage(device,data->image,nullptr);

    if(data->memory)
        vkFreeMemory(device,data->memory,nullptr);
}
VK_NAMESPACE_END
