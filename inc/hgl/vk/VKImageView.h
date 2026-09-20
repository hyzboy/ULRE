#ifndef HGL_GRAPH_VULKAN_IMAGE_VIEW_INCLUDE
#define HGL_GRAPH_VULKAN_IMAGE_VIEW_INCLUDE

#include<hgl/vk/VK.h>
namespace hgl::graph{
class ImageView
{
protected:

    VkDevice device;
    VkImageView image_view;
    ImageViewCreateInfo *ivci;

    VkExtent3D extent;

private:

    friend ImageView *CreateImageView(VkDevice device,VkImageViewType type,VkFormat format,const VkExtent3D &ext,const uint32_t &miplevel,VkImageAspectFlags aspectMask,VkImage img,bool sampled_usage);

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

/// 创建 ImageView
///
/// @param sampled_usage 该视图是否会被用于 SAMPLED_IMAGE 描述符（如 bindless 采样）。
///
/// 深度/模板格式的视图有两种互斥的要求：
/// - 附件用途：部分驱动要求 depth-stencil 混合格式的视图同时含 DEPTH|STENCIL aspect
/// - 采样用途：SAMPLED_IMAGE 的视图**只能含 DEPTH 或 STENCIL 之一**
///   （VUID-VkDescriptorImageInfo-imageView-01976），两者同时存在时采样行为未定义
///
/// 因此同一张深度纹理若既要作附件、又要被采样（shadow map 的典型情况），
/// 必须创建**两个** aspect 不同的视图：附件视图用默认值，采样视图传 sampled_usage=true。
ImageView *CreateImageView(VkDevice device,VkImageViewType type,VkFormat format,const VkExtent3D &ext,const uint32_t &miplevel,VkImageAspectFlags aspectMask,VkImage img,bool sampled_usage=false);

#define CREATE_IMAGE_VIEW(short_name,larget_name) \
    inline ImageView *CreateImageView##short_name(VkDevice device,VkFormat format,const VkExtent3D &ext,const uint32_t &miplevel,VkImageAspectFlags aspectMask,VkImage img=VK_NULL_HANDLE,bool sampled_usage=false)   \
    {   \
        return CreateImageView(device,VK_IMAGE_VIEW_TYPE_##larget_name,format,ext,miplevel,aspectMask,img,sampled_usage);  \
    }

    CREATE_IMAGE_VIEW(1D,1D);
    CREATE_IMAGE_VIEW(2D,2D);
    CREATE_IMAGE_VIEW(3D,3D);
    CREATE_IMAGE_VIEW(Cube,CUBE);
    CREATE_IMAGE_VIEW(1DArray,1D_ARRAY);
    CREATE_IMAGE_VIEW(2DArray,2D_ARRAY);
    CREATE_IMAGE_VIEW(CubeArray,CUBE_ARRAY);
#undef CREATE_IMAGE_VIEW
}//namespace hgl::graph
#endif//HGL_GRAPH_VULKAN_IMAGE_VIEW_INCLUDE
