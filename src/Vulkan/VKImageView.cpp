#include<hgl/vk/VKImageView.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>

namespace hgl::graph{
ImageView::~ImageView()
{
    VulkanDevice *owner = VulkanDevice::FromDevice(device);
    if (owner)
        owner->UntrackObject(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)(uintptr_t)image_view);

    delete ivci;
    vkDestroyImageView(device,image_view,nullptr);
}

ImageView *CreateImageView(VkDevice device,VkImageViewType type,VkFormat format,const VkExtent3D &ext,const uint32_t &miplevel,VkImageAspectFlags aspectMask,VkImage img,bool sampled_usage)
{
    // 采样用途（SAMPLED_IMAGE 描述符）下，depth/stencil 视图只能含其中一个 aspect
    //（VUID-VkDescriptorImageInfo-imageView-01976）。两者同时存在时采样行为未定义，
    // 表现为采样结果全 0 或全 1——"深度贴图什么都看不到"就是这个原因。
    // 附件用途无此限制，且部分驱动要求 depth-stencil 格式的附件视图同时含两个 aspect。
    if(sampled_usage && (aspectMask&VK_IMAGE_ASPECT_DEPTH_BIT))
        aspectMask&=~VK_IMAGE_ASPECT_STENCIL_BIT;

    ImageViewCreateInfo *iv_createinfo=new ImageViewCreateInfo;

    iv_createinfo->image     =img;
    iv_createinfo->format    =format;
    iv_createinfo->viewType  =type;
    iv_createinfo->subresourceRange.aspectMask       =aspectMask;
    iv_createinfo->subresourceRange.baseMipLevel     =0;
    iv_createinfo->subresourceRange.levelCount       =miplevel;
    iv_createinfo->subresourceRange.baseArrayLayer   =0;

    if(type==VK_IMAGE_VIEW_TYPE_CUBE)
        iv_createinfo->subresourceRange.layerCount   =6;
    else
    if(type==VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
        iv_createinfo->subresourceRange.layerCount   =6*ext.depth;
    else
        iv_createinfo->subresourceRange.layerCount   =ext.depth;

    if(aspectMask&VK_IMAGE_ASPECT_DEPTH_BIT)
    {
        // 仅附件用途才补 STENCIL（见函数入口处说明）
        if(!sampled_usage && format>=VK_FORMAT_D16_UNORM_S8_UINT)
            iv_createinfo->subresourceRange.aspectMask|=VK_IMAGE_ASPECT_STENCIL_BIT;

        if(sampled_usage)
        {
            // 采样用途的深度视图：把 R 广播到 RGB，A 置 1。
            //
            // 深度格式只有一个通道，规范规定缺失分量为 (0,0,1)，即 `texture()`
            // 返回 (d,0,0,1)。按 IDENTITY 保留原样的话，任何「把深度当颜色看」的
            // 材质（调试视图、把 RT 深度附件贴到物体上）都只能看到一片纯红，
            // 亮暗信息全压在 R 通道里，肉眼无法判读。
            // 广播后同一份数据表现为灰度，且 `.r` 语义不变（阴影 PCF 这类只取
            // 标量的路径不受影响）；同时把原本「未定义」的 G/B 明确钉死，
            // 消除驱动差异。
            iv_createinfo->components.r=VK_COMPONENT_SWIZZLE_R;
            iv_createinfo->components.g=VK_COMPONENT_SWIZZLE_R;
            iv_createinfo->components.b=VK_COMPONENT_SWIZZLE_R;
            iv_createinfo->components.a=VK_COMPONENT_SWIZZLE_ONE;
        }
        else
        {
            iv_createinfo->components.r=VK_COMPONENT_SWIZZLE_IDENTITY;
            iv_createinfo->components.g=VK_COMPONENT_SWIZZLE_IDENTITY;
            iv_createinfo->components.b=VK_COMPONENT_SWIZZLE_IDENTITY;
            iv_createinfo->components.a=VK_COMPONENT_SWIZZLE_IDENTITY;
        }
    }
    else
    {
        iv_createinfo->components.r=VK_COMPONENT_SWIZZLE_R;
        iv_createinfo->components.g=VK_COMPONENT_SWIZZLE_G;
        iv_createinfo->components.b=VK_COMPONENT_SWIZZLE_B;
        iv_createinfo->components.a=VK_COMPONENT_SWIZZLE_A;
    }

    VkImageView img_view;

    if(vkCreateImageView(device,iv_createinfo,nullptr,&img_view)!=VK_SUCCESS)
    {
        delete iv_createinfo;
        return(nullptr);
    }

    ImageView *result = new ImageView(device,img_view,iv_createinfo,ext);
    VulkanDevice *owner = VulkanDevice::FromDevice(device);
    if (owner)
        owner->TrackObject(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)(uintptr_t)img_view, "ImageView");
    return result;
}
}//namespace hgl::graph
