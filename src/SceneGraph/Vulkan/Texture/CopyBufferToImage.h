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
};
VK_NAMESPACE_END
