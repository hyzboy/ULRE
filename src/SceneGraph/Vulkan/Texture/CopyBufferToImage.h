#pragma once
#include"BufferImageCopy2D.h"

VK_NAMESPACE_BEGIN
struct CopyBufferToImageInfo
{
    VkImage                     image;
    VkBuffer                    buffer;

    VkImageSubresourceRange     isr;

    const VkBufferImageCopy *   bic_list;
    uint32_t                    bic_count;

public:

    CopyBufferToImageInfo()
    {
        hgl_zero(*this);
    }

    CopyBufferToImageInfo(Texture *tex,VkBuffer buf,const VkBufferImageCopy *bic)
    {
        image               =tex->GetImage();
        buffer              =buf;

        isr.aspectMask      =tex->GetAspect();

        isr.baseMipLevel    =0;
        isr.levelCount      =tex->GetMipLevel();

        isr.baseArrayLayer  =0;
        isr.layerCount      =1;

        bic_list            =bic;
        bic_count           =1;
    }
};

struct CopyBufferToImageMipmapsInfo:public CopyBufferToImageInfo
{
    CopyBufferToImageMipmapsInfo(Texture2D *tex,VkBuffer buf,const VkBufferImageCopy *bic,const uint32_t miplevel)
    {
        image               =tex->GetImage();
        buffer              =buf;

        isr.aspectMask      =tex->GetAspect();

        isr.baseMipLevel    =0;
        isr.levelCount      =tex->GetMipLevel();

        isr.baseArrayLayer  =0;
        isr.layerCount      =1;

        bic_list            =bic;
        bic_count           =miplevel;
    }
};

VK_NAMESPACE_END
