#ifndef HGL_GRAPH_VULKAN_IMAGE_VIEW_INCLUDE
#define HGL_GRAPH_VULKAN_IMAGE_VIEW_INCLUDE

#include<hgl/graph/VK.h>
VK_NAMESPACE_BEGIN
class ImageView
{
protected:

    VkDevice device;
    VkImageView image_view;
    ImageViewCreateInfo *ivci;
    
    VkExtent3D extent;

private:

    friend ImageView *CreateImageView(VkDevice device,VkImageViewType type,VkFormat format,const VkExtent3D &ext,const uint32_t &miplevel,VkImageAspectFlags aspectMask,VkImage img);

    ImageView(VkDevice dev,VkImageView iv,ImageViewCreateInfo *ci,const VkExtent3D &ext)
    {
        device      =dev;
        image_view  =iv;
        ivci        =ci;
        extent      =ext;
    }

public:

    virtual ~ImageView();

public:
    

          VkImageView           GetImageView    ()     {return image_view;}
    const VkImageViewType       GetViewType     ()const{return ivci->viewType;}
    const VkFormat              GetFormat       ()const{return ivci->format;}
    const VkExtent3D &          GetExtent       ()const{return extent;}
    const VkImageAspectFlags    GetAspectFlags  ()const{return ivci->subresourceRange.aspectMask;}
    const uint32_t              GetLayerCount   ()const{return ivci->subresourceRange.layerCount;}

    const bool                  hasColor        ()const{return ivci->subresourceRange.aspectMask&VK_IMAGE_ASPECT_COLOR_BIT;}
    const bool                  hasDepth        ()const{return ivci->subresourceRange.aspectMask&VK_IMAGE_ASPECT_DEPTH_BIT;}
    const bool                  hasStencil      ()const{return ivci->subresourceRange.aspectMask&VK_IMAGE_ASPECT_STENCIL_BIT;}
    const bool                  hasDepthStencil ()const{return ivci->subresourceRange.aspectMask&(VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT);}
};//class ImageView

ImageView *CreateImageView(VkDevice device,VkImageViewType type,VkFormat format,const VkExtent3D &ext,const uint32_t &miplevel,VkImageAspectFlags aspectMask,VkImage img);

#define CREATE_IMAGE_VIEW(short_name,larget_name) \
    inline ImageView *CreateImageView##short_name(VkDevice device,VkFormat format,const VkExtent3D &ext,const uint32_t &miplevel,VkImageAspectFlags aspectMask,VkImage img=VK_NULL_HANDLE)   \
    {   \
        return CreateImageView(device,VK_IMAGE_VIEW_TYPE_##larget_name,format,ext,miplevel,aspectMask,img);  \
    }

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
