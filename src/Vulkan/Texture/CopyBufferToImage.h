#pragma once
#include"BufferImageCopy2D.h"

namespace hgl::graph{
struct CopyBufferToImageInfo
{
    VkImage                     image;
    VkBuffer                    buffer;

    VkImageSubresourceRange     isr;

    const VkBufferImageCopy *   bic_list;
    uint32_t                    bic_count;
};
}//namespace hgl::graph
