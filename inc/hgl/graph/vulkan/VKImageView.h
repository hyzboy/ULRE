#ifndef HGL_GRAPH_VULKAN_IMAGE_VIEW_INCLUDE
#define HGL_GRAPH_VULKAN_IMAGE_VIEW_INCLUDE

#include<hgl/graph/vulkan/VK.h>
VK_NAMESPACE_BEGIN
class ImageView
{
    VkDevice device;
    VkImageView image_view;
    VkImageViewCreateInfo info;

private:

    friend ImageView *CreateImageView(VkDevice device,VkImageViewType type,VkFormat format,VkImageAspectFlags aspectMask,VkImage img);

    ImageView(VkDevice dev,VkImageView iv,const VkImageViewCreateInfo &ci)
    {
        device=dev;
        image_view=iv;
        info=ci;
    }

public:

    ~ImageView();

    operator VkImageView(){return image_view;}

public:

    const VkImageViewType       GetViewType     ()const{return info.viewType;}
    const VkFormat              GetFormat       ()const{return info.format;}
    const VkImageAspectFlags    GetAspectFlags  ()const{return info.subresourceRange.aspectMask;}
};//class ImageView

ImageView *CreateImageView(VkDevice device,VkImageViewType type,VkFormat format,VkImageAspectFlags aspectMask,VkImage img=nullptr);
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_IMAGE_VIEW_INCLUDE
