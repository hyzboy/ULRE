#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKImageView.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/graph/module/TextureManager.h>
#include<cstdint>
namespace hgl::graph{
Texture::~Texture()
{
    if (manager)
        manager->Release(this);

    if(!data)return;

    VulkanDevice *owner = manager ? VulkanDevice::FromDevice(manager->GetVkDevice()) : nullptr;

    if(data->image)
    {
        if (owner)
            owner->UntrackObject(VK_OBJECT_TYPE_IMAGE, (uint64_t)(uintptr_t)data->image);
    }

    if(data->vk_memory)
    {
        if (owner)
            owner->UntrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)data->vk_memory);
    }

    if(data->image_view)
        delete data->image_view;

    if(data->allocation && data->image)
    {
        if (owner)
            vmaDestroyImage(owner->GetVmaAllocator(), data->image, data->allocation);
        else
            vkDestroyImage(manager->GetVkDevice(),data->image,nullptr);
    }
    else if(data->memory)        // legacy path (pre-VMA / external image ownership)
    {
        delete data->memory;

        if(data->image)
            vkDestroyImage(manager->GetVkDevice(),data->image,nullptr);
    }
}
}//namespace hgl::graph
