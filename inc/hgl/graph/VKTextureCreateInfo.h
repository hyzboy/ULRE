#ifndef HGL_GRAPH_VULKAN_TEXTURE_CREATE_INFO_INCLUDE
#define HGL_GRAPH_VULKAN_TEXTURE_CREATE_INFO_INCLUDE

#include<hgl/graph/VK.h>
VK_NAMESPACE_BEGIN
struct TextureData
{
    GPUMemory *         memory      =nullptr;
    VkImage             image       =VK_NULL_HANDLE;
    VkImageLayout       image_layout=VK_IMAGE_LAYOUT_UNDEFINED;
    ImageView *         image_view  =nullptr;
    uint32              mip_levels  =0;
    VkImageTiling       tiling      =VK_IMAGE_TILING_OPTIMAL;
};//struct TextureData

struct TextureCreateInfo
{
    VkExtent3D          extent;
    VkFormat            format;
    uint32_t            usage;
    uint32_t            mipmap;         ///<如果值>0表示提供的数据已有mipmaps，如果为0表示自动生成mipmaps
    VkImageAspectFlags  aspect;
    ImageTiling         tiling;

    VkImageLayout       image_layout;

    VkImage             image;          //如果没有IMAGE，则创建。（交换链等会直接提供image，所以存在外部传入现像)
    GPUMemory *         memory;         //同时需分配内存并绑定

    ImageView *         image_view;     //如果没有imageview，则创建

    void *              pixels;         //如果没有buffer但有pixels，则根据pixels和以上条件创建buffer
    VkDeviceSize        pixel_bytes;
    GPUBuffer *         buffer;         //如果pixels也没有，则代表不会立即写入图像数据

public:

    TextureCreateInfo()
    {
        hgl_zero(*this);
        mipmap=1;
    }

    void SetAutoMipmaps(){mipmap=0;}

    TextureCreateInfo(const uint32_t aspect_bit,const VkExtent2D &ext,const VkFormat &fmt,VkImage img):TextureCreateInfo()
    {   
        aspect=aspect_bit;

        extent.width=ext.width;
        extent.height=ext.height;
        extent.depth=1;

        format=fmt;
        image=img;
    }

    TextureCreateInfo(const uint32_t aspect_bit,const uint32_t u,const ImageTiling it,const VkImageLayout il):TextureCreateInfo()
    {
        aspect=aspect_bit;

        usage=u;
        tiling=it;
        image_layout=il;
    }

    TextureCreateInfo(const uint32_t aspect_bit,const VkFormat &fmt,const uint32_t u,const ImageTiling it,const VkImageLayout il):TextureCreateInfo()
    {
        aspect=aspect_bit;

        format=fmt;
        usage=u;
        tiling=it;
        image_layout=il;
    }

    TextureCreateInfo(const uint32_t aspect_bit,const uint32_t u,const ImageTiling it)
        :TextureCreateInfo(aspect_bit,u,it,
            (tiling==ImageTiling::Optimal?
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                VK_IMAGE_LAYOUT_GENERAL)){}
                
    TextureCreateInfo(const uint32_t aspect_bit,const VkExtent2D &ext,const uint32_t u,const ImageTiling it)
        :TextureCreateInfo(aspect_bit,u,it)
    {
        extent.width=ext.width;
        extent.height=ext.height;
        extent.depth=1;        
    }

    TextureCreateInfo(const uint32_t aspect_bit,const VkFormat &fmt,const VkExtent2D &ext,const uint32_t u,const ImageTiling it,const VkImageLayout il)
        :TextureCreateInfo(aspect_bit,u,it,il)
    {
        format=fmt;
        extent.width=ext.width;
        extent.height=ext.height;
        extent.depth=1;        
    }
            
    TextureCreateInfo(const uint32_t aspect_bit,const uint32_t u)
        :TextureCreateInfo(aspect_bit,u,ImageTiling::Optimal,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL){}
        
    TextureCreateInfo(const uint32_t aspect_bit)
        :TextureCreateInfo( aspect_bit,
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                            ImageTiling::Optimal,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL){}

    TextureCreateInfo(const VkFormat fmt):TextureCreateInfo()
    {
        if(IsDepthStencilFormat(fmt))
            aspect=VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;
        else
        if(IsDepthFormat(fmt))
            aspect=VK_IMAGE_ASPECT_DEPTH_BIT;
        else
        if(CheckVulkanFormat(fmt))
        {
            aspect=VK_IMAGE_ASPECT_COLOR_BIT;
        }
        else
        {
            aspect=0;
        }

        format=fmt;
        usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    TextureCreateInfo(const uint32_t aspect_bits,const VkFormat fmt,const VkExtent2D &ext):TextureCreateInfo(fmt)
    {
        aspect=aspect;

        extent.width=ext.width;
        extent.height=ext.height;
        extent.depth=1;
    }

    bool SetFormat(const VkFormat fmt)
    {
        if(!CheckVulkanFormat(fmt))return(false);

        if(aspect&VK_IMAGE_ASPECT_DEPTH_BIT)
        {
            if(aspect&VK_IMAGE_ASPECT_STENCIL_BIT)
            {
                if(!IsDepthStencilFormat(fmt))return(false);
            }
            else
            {
                if(!IsDepthFormat(fmt))return(false);
            }
        }
        else
        {
            if(aspect&VK_IMAGE_ASPECT_STENCIL_BIT)
                if(!IsStencilFormat(fmt))return(false);
        }

        return(true);
    }

    bool SetData(GPUBuffer *buf,const uint32_t w,const uint32_t h)
    {
        if(!buf||w<=0||h<=0)return(false);

        buffer=buf;

        extent.width=w;
        extent.height=h;
        extent.depth=1;

        return(true);
    }
};//struct TextureCreateInfo

struct ColorTextureCreateInfo:public TextureCreateInfo
{
    ColorTextureCreateInfo():TextureCreateInfo(VK_IMAGE_ASPECT_COLOR_BIT){}
    ColorTextureCreateInfo(const VkFormat format,const VkExtent2D &ext):TextureCreateInfo(VK_IMAGE_ASPECT_COLOR_BIT,format,ext){}
};//struct ColorTextureCreateInfo:public TextureCreateInfo

struct DepthTextureCreateInfo:public TextureCreateInfo
{
    DepthTextureCreateInfo():TextureCreateInfo(VK_IMAGE_ASPECT_DEPTH_BIT){}
    DepthTextureCreateInfo(const VkFormat format,const VkExtent2D &ext):TextureCreateInfo(VK_IMAGE_ASPECT_COLOR_BIT,format,ext){}
};//struct DepthTextureCreateInfo:public TextureCreateInfo

struct DepthStencilTextureCreateInfo:public TextureCreateInfo
{
    DepthStencilTextureCreateInfo():TextureCreateInfo(VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT){}
};//struct DepthStencilTextureCreateInfo:public TextureCreateInfo

struct AttachmentTextureCreateInfo:public TextureCreateInfo
{
    AttachmentTextureCreateInfo(const uint32_t aspect_bit,const uint32_t u,const VkImageLayout il):TextureCreateInfo(aspect_bit,u,ImageTiling::Optimal,il){}
};//struct AttachmentTextureCreateInfo:public TextureCreateInfo

struct ColorAttachmentTextureCreateInfo:public AttachmentTextureCreateInfo
{
    ColorAttachmentTextureCreateInfo()
        :AttachmentTextureCreateInfo(   VK_IMAGE_ASPECT_COLOR_BIT,

                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                        |VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                        |VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                        |VK_IMAGE_USAGE_SAMPLED_BIT,

                                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL){}

    ColorAttachmentTextureCreateInfo(const VkFormat fmt,const VkExtent3D &ext):ColorAttachmentTextureCreateInfo()
    {
        format=fmt;
        extent=ext;
    }
};

struct DepthAttachmentTextureCreateInfo:public AttachmentTextureCreateInfo
{
    DepthAttachmentTextureCreateInfo()
        :AttachmentTextureCreateInfo(   VK_IMAGE_ASPECT_DEPTH_BIT,

                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                        |VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                        |VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                        |VK_IMAGE_USAGE_SAMPLED_BIT,

                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL){}

    DepthAttachmentTextureCreateInfo(const VkFormat fmt,const VkExtent3D &ext):DepthAttachmentTextureCreateInfo()
    {
        format=fmt;
        extent=ext;
    }
};

struct DepthStencilAttachmentTextureCreateInfo:public AttachmentTextureCreateInfo
{
    DepthStencilAttachmentTextureCreateInfo()
        :AttachmentTextureCreateInfo(   VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,

                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                        |VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                        |VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                        |VK_IMAGE_USAGE_SAMPLED_BIT,

                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL){}

    DepthStencilAttachmentTextureCreateInfo(const VkFormat fmt,const VkExtent3D &ext):DepthStencilAttachmentTextureCreateInfo()
    {
        format=fmt;
        extent=ext;
    }
};

struct SwapchainColorTextureCreateInfo:public TextureCreateInfo
{
    SwapchainColorTextureCreateInfo(const VkFormat fmt,const VkExtent2D &ext,VkImage img)
        :TextureCreateInfo(VK_IMAGE_ASPECT_COLOR_BIT,ext,fmt,img){}
};//struct SwapchainColorTextureCreateInfo:public TextureCreateInfo

struct SwapchainDepthTextureCreateInfo:public TextureCreateInfo
{
    SwapchainDepthTextureCreateInfo(const VkFormat fmt,const VkExtent2D &ext)
        :TextureCreateInfo( VK_IMAGE_ASPECT_DEPTH_BIT,
                            fmt,
                            ext,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                            ImageTiling::Optimal,
                            VK_IMAGE_LAYOUT_UNDEFINED){}
};//struct SwapchainColorTextureCreateInfo:public TextureCreateInfo
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_TEXTURE_CREATE_INFO_INCLUDE