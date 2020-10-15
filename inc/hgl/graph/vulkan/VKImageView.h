#ifndef HGL_GRAPH_VULKAN_IMAGE_VIEW_INCLUDE
#define HGL_GRAPH_VULKAN_IMAGE_VIEW_INCLUDE

#include<hgl/graph/vulkan/VK.h>
VK_NAMESPACE_BEGIN
class ImageView
{
protected:

    VkDevice device;
    VkImageView image_view;
    
    VkImageViewType view_type;
    VkFormat format;
    VkImageAspectFlags aspect_mask;

    VkExtent3D extent;

private:

    friend ImageView *CreateImageView(VkDevice device,VkImageViewType type,VkFormat format,const VkExtent3D &ext,VkImageAspectFlags aspectMask,VkImage img);

    ImageView(VkDevice dev,VkImageView iv,const VkImageViewType vt,const VkFormat fmt,const VkExtent3D &ext,const VkImageAspectFlags am)
    {
        device      =dev;
        image_view  =iv;
        view_type   =vt;
        format      =fmt;
        aspect_mask =am;
        extent      =ext;
    }

public:

    virtual ~ImageView();

    operator VkImageView(){return image_view;}

public:

    const VkImageViewType       GetViewType     ()const{return view_type;}
    const VkFormat              GetFormat       ()const{return format;}
    const VkExtent3D &          GetExtent       ()const{return extent;}
    const VkImageAspectFlags    GetAspectFlags  ()const{return aspect_mask;}

    const bool                  hasColor        ()const{return aspect_mask&VK_IMAGE_ASPECT_COLOR_BIT;}
    const bool                  hasDepth        ()const{return aspect_mask&VK_IMAGE_ASPECT_DEPTH_BIT;}
    const bool                  hasStencil      ()const{return aspect_mask&VK_IMAGE_ASPECT_STENCIL_BIT;}
    const bool                  hasDepthStencil ()const{return aspect_mask&(VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT);}
};//class ImageView

ImageView *CreateImageView(VkDevice device,VkImageViewType type,VkFormat format,const VkExtent3D &ext,VkImageAspectFlags aspectMask,VkImage img);

#define CREATE_IMAGE_VIEW(short_name,larget_name) \
    inline ImageView *CreateImageView##short_name(VkDevice device,VkFormat format,const VkExtent3D &ext,VkImageAspectFlags aspectMask,VkImage img=VK_NULL_HANDLE)   \
    {   \
        return CreateImageView(device,VK_IMAGE_VIEW_TYPE_##larget_name,format,ext,aspectMask,img);  \
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
