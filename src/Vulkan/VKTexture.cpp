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

    if(data->image)
    {
        VulkanDevice *owner = VulkanDevice::FromDevice(manager->GetVkDevice());
        if (owner)
            owner->UntrackObject(VK_OBJECT_TYPE_IMAGE, (uint64_t)(uintptr_t)data->image);
    }

    if(data->array_view)
        delete data->array_view;

    if(data->image_view)
        delete data->image_view;

    if(data->memory)        //没有memory的纹理都是从其它地方借来的，所以就不存在删除
    {
        delete data->memory;

        if(data->image)
            vkDestroyImage(manager->GetVkDevice(),data->image,nullptr);
    }
}

VkImageView Texture2D::GetBindlessArrayView()
{
    if(!data||!data->image_view)
        return VK_NULL_HANDLE;

    if(data->array_view)
        return data->array_view->GetImageView();

    VkExtent3D ext = data->image_view->GetExtent();
    ext.depth      = 1;      // 单层 2D_ARRAY view（layerCount = ext.depth）

    data->array_view = CreateImageView2DArray(manager->GetVkDevice(),
                                              data->image_view->GetFormat(),
                                              ext,
                                              data->miplevel,
                                              data->image_view->GetAspectFlags(),
                                              data->image);

    return data->array_view ? data->array_view->GetImageView() : VK_NULL_HANDLE;
}
}//namespace hgl::graph
