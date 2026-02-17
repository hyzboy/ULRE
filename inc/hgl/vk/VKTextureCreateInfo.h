#pragma once

#include<hgl/vk/VK.h>
namespace hgl::graph{
struct TextureCreateInfo
{
    VkImageViewType     type;

    VkExtent3D          extent;         // For arrays, depth is treated as layers

    VkFormat            format;
    uint32_t            usage;
    uint32_t            mipmap_zero_total_bytes;
    uint32_t            origin_mipmaps; // Original mipmap count (0/1)
    uint32_t            target_mipmaps; // Target mipmap count; if not equal to origin_mipmaps, ensures no auto-generation of mipmaps
    VkImageAspectFlags  aspect;
    ImageTiling         tiling;

    VkImageLayout       image_layout;

    VkImage             image;          // If no IMAGE, create one; otherwise directly provide image (can be from external source)
    DeviceMemory *      memory;         // Bind memory at the same time

    ImageView *         image_view;     // If no imageview, create one

    void *              pixels;         // If buffer or pixels data exists, use pixels data; otherwise read from buffer
    VkDeviceSize        total_bytes;
    DeviceBuffer *      buffer;         // If pixels is also missing, write image data from this buffer

    U8String            name;           // Texture name for debugging/tracing

public:

    TextureCreateInfo()
    {
        mem_zero(*this);
    }

    TextureCreateInfo(const uint32_t aspect_bit,const VkExtent2D &ext,const VkFormat &fmt,VkImage img,const U8String &n=U8String((const u8char*)u8"Texture")):TextureCreateInfo()
    {
        aspect=aspect_bit;

        extent.width=ext.width;
        extent.height=ext.height;
        extent.depth=1;

        format=fmt;
        image=img;
        name=n;
    }

    TextureCreateInfo(const uint32_t aspect_bit,const uint32_t u,const ImageTiling it,const VkImageLayout il,const U8String &n=U8String((const u8char*)u8"Texture")):TextureCreateInfo()
    {
        aspect=aspect_bit;

        usage=u;
        tiling=it;
        image_layout=il;
        name=n;
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

    TextureCreateInfo(const uint32_t aspect_bit,const VkFormat &fmt,const VkExtent2D &ext,const uint32_t u,const ImageTiling it,const VkImageLayout il,const U8String &n=U8String((const u8char*)u8"Texture"))
        :TextureCreateInfo(aspect_bit,u,it,il,n)
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

    TextureCreateInfo(const VkFormat fmt,const U8String &n=U8String((const u8char*)u8"Texture")):TextureCreateInfo()
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
        name=n;
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

    bool SetData(DeviceBuffer *buf,const VkExtent3D &ext)
    {
        if(!buf)return(false);
        if(ext.width<=0||ext.height<=0||ext.depth<=0)return(false);

        buffer=buf;

        extent=ext;

        return(true);
    }

    void SetAutoMipmaps()
    {
        target_mipmaps=hgl::GetMipLevel(extent.width,extent.height,extent.depth);
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

    ColorAttachmentTextureCreateInfo(const VkFormat fmt,const VkExtent3D &ext,const U8String &n=U8String((const u8char*)u8"ColorAttachment")):ColorAttachmentTextureCreateInfo()
    {
        format=fmt;
        extent=ext;
        name=n;
    }

    ColorAttachmentTextureCreateInfo(const VkFormat fmt,const VkExtent2D &ext,const U8String &n=U8String((const u8char*)u8"ColorAttachment")):ColorAttachmentTextureCreateInfo()
    {
        format=fmt;
        extent.width=ext.width;
        extent.height=ext.height;
        extent.depth=1;
        name=n;
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

    DepthAttachmentTextureCreateInfo(const VkFormat fmt,const VkExtent3D &ext,const U8String &n=U8String((const u8char*)u8"DepthAttachment")):DepthAttachmentTextureCreateInfo()
    {
        format=fmt;
        extent=ext;
        name=n;
    }

    DepthAttachmentTextureCreateInfo(const VkFormat fmt,const VkExtent2D &ext,const U8String &n=U8String((const u8char*)u8"DepthAttachment")):DepthAttachmentTextureCreateInfo()
    {
        format=fmt;
        extent.width=ext.width;
        extent.height=ext.height;
        extent.depth=1;
        name=n;
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

    DepthStencilAttachmentTextureCreateInfo(const VkFormat fmt,const VkExtent3D &ext,const U8String &n=U8String((const u8char*)u8"DepthStencilAttachment")):DepthStencilAttachmentTextureCreateInfo()
    {
        format=fmt;
        extent=ext;
        name=n;
    }

    DepthStencilAttachmentTextureCreateInfo(const VkFormat fmt,const VkExtent2D &ext,const U8String &n=U8String((const u8char*)u8"DepthStencilAttachment")):DepthStencilAttachmentTextureCreateInfo()
    {
        format=fmt;
        extent.width=ext.width;
        extent.height=ext.height;
        extent.depth=1;
        name=n;
    }
};

struct SwapchainColorTextureCreateInfo:public TextureCreateInfo
{
    SwapchainColorTextureCreateInfo(const VkFormat fmt,const VkExtent2D &ext,VkImage img,const U8String &n=U8String((const u8char*)u8"SwapchainColor"))
        :TextureCreateInfo(VK_IMAGE_ASPECT_COLOR_BIT,ext,fmt,img,n){}
};//struct SwapchainColorTextureCreateInfo:public TextureCreateInfo

struct SwapchainDepthTextureCreateInfo:public TextureCreateInfo
{
    SwapchainDepthTextureCreateInfo(const VkFormat fmt,const VkExtent2D &ext,const U8String &n=U8String((const u8char*)u8"SwapchainDepth"))
        :TextureCreateInfo( VK_IMAGE_ASPECT_DEPTH_BIT,
                            fmt,
                            ext,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                            ImageTiling::Optimal,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            n){}
};//struct SwapchainColorTextureCreateInfo:public TextureCreateInfo

struct TextureData
{
    DeviceMemory *      memory;
    VkImage             image;
    VkImageLayout       image_layout;
    ImageView *         image_view;
    uint32              miplevel;
    VkImageTiling       tiling;

public:

    TextureData()
    {
        mem_zero(*this);
    }

    TextureData(const TextureCreateInfo *tci)
    {
        memory      =tci->memory;
        image       =tci->image;
        image_view  =tci->image_view;
        miplevel    =tci->target_mipmaps;
        tiling      =VkImageTiling(tci->tiling);

        if(!tci->buffer&&!tci->pixels&&tci->image_layout==VK_IMAGE_LAYOUT_UNDEFINED)
            image_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        else
            image_layout=tci->image_layout;
    }
};//struct TextureData
}//namespace hgl::graph
