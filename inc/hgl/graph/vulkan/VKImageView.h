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

#define CREATE_IMAGE_VIEW(short_name,larget_name) inline ImageView *CreateImageView##short_name(VkDevice device,VkFormat format,VkImageAspectFlags aspectMask,VkImage img=nullptr){return CreateImageView(device,VK_IMAGE_VIEW_TYPE_##larget_name,format,aspectMask,img);}
    CREATE_IMAGE_VIEW(1D,1D);
    CREATE_IMAGE_VIEW(2D,2D);
    CREATE_IMAGE_VIEW(3D,3D);
    CREATE_IMAGE_VIEW(Cube,CUBE);
    CREATE_IMAGE_VIEW(1DArray,1D_ARRAY);
    CREATE_IMAGE_VIEW(2DArray,2D_ARRAY);
    CREATE_IMAGE_VIEW(CubeArray,CUBE_ARRAY);
#undef CREATE_IMAGE_VIEW
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_IMAGE_VIEW_INCLUDE
