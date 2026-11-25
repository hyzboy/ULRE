#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKImageView.h>
#include<hgl/vk/buffer/BufferMemory.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/log/Log.h>
#include<cstdint>
namespace hgl::graph{
Texture::~Texture()
{
    if(owner)
        owner->OnTextureDestroying(this);

    if(!data)return;

    const VkDevice vk_dev = owner ? owner->GetOwnerVkDevice() : VK_NULL_HANDLE;

    if(data->image && vk_dev != VK_NULL_HANDLE && data->owns_image)
    {
        VulkanDevice *owner = VulkanDevice::FromDevice(vk_dev);
        if (owner)
            owner->UntrackObject(VK_OBJECT_TYPE_IMAGE, (uint64_t)(uintptr_t)data->image);
    }

    if(data->array_view)
        delete data->array_view;

    if(data->image_view)
        delete data->image_view;

    if(data->image && vk_dev != VK_NULL_HANDLE && data->owns_image)
        vkDestroyImage(vk_dev, data->image, nullptr);

    if(data->memory)
    {
        delete data->memory;
    }

    delete data;
    data = nullptr;
}

VkImageView TextureCube::GetBindlessArrayView()
{
    if(!data||!data->image_view)
        return VK_NULL_HANDLE;

    if(data->array_view)
        return data->array_view->GetImageView();

    VkExtent3D ext = data->image_view->GetExtent();
    ext.depth      = 1;      // 单张 Cubemap：6 层 CUBE_ARRAY view（6*depth）

    data->array_view = CreateImageViewCubeArray(owner->GetOwnerVkDevice(),
                                                data->image_view->GetFormat(),
                                                ext,
                                                data->miplevel,
                                                data->image_view->GetAspectFlags(),
                                                data->image);

    GLogInfo("[CubeView] tex=%p device=%p view=%p image=%p mips=%u",
             (const void *)this,
             owner->GetOwnerVkDevice(),
             data->array_view?data->array_view->GetImageView():VK_NULL_HANDLE,
             data->image,
             data->miplevel);

    return data->array_view ? data->array_view->GetImageView() : VK_NULL_HANDLE;
}

VkImageView Texture2D::GetBindlessArrayView()
{
    if(!data||!data->image_view)
        return VK_NULL_HANDLE;

    if(data->array_view)
        return data->array_view->GetImageView();

    VkExtent3D ext = data->image_view->GetExtent();
    ext.depth      = 1;      // 单层 2D_ARRAY view（layerCount = ext.depth）

    // sampled_usage=true：本视图专供 bindless 采样，深度模板格式必须只保留 DEPTH
    // aspect（VUID-VkDescriptorImageInfo-imageView-01976），否则采样行为未定义。
    // 附件视图（data->image_view）仍保持 DEPTH|STENCIL，两者互不影响。
    data->array_view = CreateImageView2DArray(owner->GetOwnerVkDevice(),
                                              data->image_view->GetFormat(),
                                              ext,
                                              data->miplevel,
                                              data->image_view->GetAspectFlags(),
                                              data->image,
                                              true);

    return data->array_view ? data->array_view->GetImageView() : VK_NULL_HANDLE;
}
}//namespace hgl::graph
