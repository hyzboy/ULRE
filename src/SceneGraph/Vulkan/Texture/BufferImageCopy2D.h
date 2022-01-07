#pragma once
#include<hgl/graph/VKTexture.h>
VK_NAMESPACE_BEGIN
struct BufferImageCopy:public VkBufferImageCopy
{
public:

    BufferImageCopy()
    {
        hgl_zero(*this);
        imageSubresource.layerCount=1;
    }

    BufferImageCopy(const VkImageAspectFlags aspect_mask):BufferImageCopy()
    {
        imageSubresource.aspectMask=aspect_mask;
    }

    BufferImageCopy(const Texture2D *tex):BufferImageCopy()
    {
        imageSubresource.aspectMask=tex->GetAspect();
        SetRectScope(0,0,tex->GetWidth(),tex->GetHeight());
    }

    BufferImageCopy(const TextureCube *tex):BufferImageCopy()
    {
        imageSubresource.aspectMask=tex->GetAspect();
        SetRectScope(0,0,tex->GetWidth(),tex->GetHeight());
    }

    void Set(const VkImageAspectFlags aspect_mask,const uint32_t layer_count)
    {
        imageSubresource.aspectMask=aspect_mask;
        imageSubresource.layerCount=layer_count;
    }

    void Set(Image2DRegion *ir)
    {
        imageOffset.x=ir->left;
        imageOffset.y=ir->top;
        imageExtent.width=ir->width;
        imageExtent.height=ir->height;
        imageExtent.depth=1;
    }

    void SetRectScope(int32_t left,int32_t top,uint32_t width,uint32_t height)
    {
        imageOffset.x=left;
        imageOffset.y=top;
        imageExtent.width=width;
        imageExtent.height=height;
        imageExtent.depth=1;
    }
};//
VK_NAMESPACE_END
