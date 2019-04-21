#include"VKImageView.h"

VK_NAMESPACE_BEGIN
ImageView::~ImageView()
{
    vkDestroyImageView(device,image_view,nullptr);
}

ImageView *CreateImageView(VkDevice device,VkImageViewType type,VkFormat format,VkImageAspectFlags aspectMask,VkImage img)
{
    VkImageViewCreateInfo iv_createinfo={};

    iv_createinfo.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    iv_createinfo.pNext=nullptr;
    iv_createinfo.flags=0;
    iv_createinfo.image=img;
    iv_createinfo.format=format;
    iv_createinfo.viewType=type;
    iv_createinfo.components.r=VK_COMPONENT_SWIZZLE_R;
    iv_createinfo.components.g=VK_COMPONENT_SWIZZLE_G;
    iv_createinfo.components.b=VK_COMPONENT_SWIZZLE_B;
    iv_createinfo.components.a=VK_COMPONENT_SWIZZLE_A;
    iv_createinfo.subresourceRange.aspectMask=aspectMask;
    iv_createinfo.subresourceRange.baseMipLevel=0;
    iv_createinfo.subresourceRange.levelCount=1;
    iv_createinfo.subresourceRange.baseArrayLayer=0;
    iv_createinfo.subresourceRange.layerCount=1;

    VkImageView iv;

    if(vkCreateImageView(device,&iv_createinfo,nullptr,&iv)!=VK_SUCCESS)
        return(nullptr);

    return(new ImageView(device,iv,iv_createinfo));
}
VK_NAMESPACE_END
