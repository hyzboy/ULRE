#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKImageView.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/graph/module/TextureManager.h>
#include<cstdint>
VK_NAMESPACE_BEGIN
Texture::~Texture()
{
    if(!data)return;

    if(data->image)
    {
        VulkanDevice *owner = VulkanDevice::FromDevice(manager->GetVkDevice());
        if (owner)
            owner->UntrackObject(VK_OBJECT_TYPE_IMAGE, (uint64_t)(uintptr_t)data->image);
    }

    if(data->image_view)
        delete data->image_view;

    if(data->memory)        //没有memory的纹理都是从其它地方借来的，所以就不存在删除
    {
        delete data->memory;

        if(data->image)
            vkDestroyImage(manager->GetVkDevice(),data->image,nullptr);
    }
}
VK_NAMESPACE_END
