#include<hgl/graph/vulkan/VKImageView.h>

VK_NAMESPACE_BEGIN
class StandaloneImageView:public ImageView
{
    friend ImageView *CreateImageView(VkDevice device,VkImageViewType type,VkFormat format,VkImageAspectFlags aspectMask,VkImage img);

    using ImageView::ImageView;

public:

    ~StandaloneImageView()
    {
        vkDestroyImageView(device,image_view,nullptr);
    }
};

ImageView *CreateRefImageView(VkDevice device,VkImageViewType type,VkFormat format,VkImageAspectFlags aspectMask,VkImageView img_view)
{
    return(new ImageView(device,img_view,type,format,aspectMask));
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

    VkImageView img_view;

    if(vkCreateImageView(device,&iv_createinfo,nullptr,&img_view)!=VK_SUCCESS)
        return(nullptr);

    return(new StandaloneImageView(device,img_view,type,format,aspectMask));
}
VK_NAMESPACE_END
