#pragma once
#include<hgl/graph/VKTexture.h>
#include<hgl/type/RectScope.h>
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
        SetRectScope(tex->GetWidth(),tex->GetHeight());
    }

    template<typename T>
    BufferImageCopy(const Texture2D *tex,const RectScope2<T> &rs):BufferImageCopy()
    {
        imageSubresource.aspectMask=tex->GetAspect();
        SetRectScope(rs);
    }

    BufferImageCopy(const Texture2DArray *tex):BufferImageCopy()
    {
        imageSubresource.aspectMask=tex->GetAspect();
        SetRectScope(tex->GetWidth(),tex->GetHeight());
    }

    template<typename T>
    BufferImageCopy(const Texture2DArray *tex,const RectScope2<T> &rs):BufferImageCopy()
    {
        imageSubresource.aspectMask=tex->GetAspect();
        SetRectScope(rs);
    }

    BufferImageCopy(const TextureCube *tex):BufferImageCopy()
    {
        imageSubresource.aspectMask=tex->GetAspect();
        imageSubresource.layerCount=6;
        SetRectScope(tex->GetWidth(),tex->GetHeight());
    }

    void Set(const VkImageAspectFlags aspect_mask,const uint32_t layer_count)
    {
        imageSubresource.aspectMask=aspect_mask;
        imageSubresource.layerCount=layer_count;
    }

    void Set(Image2DRegion *ir)
    {
        imageOffset.x       =ir->left;
        imageOffset.y       =ir->top;
        imageOffset.z       =0;
        imageExtent.width   =ir->width;
        imageExtent.height  =ir->height;
        imageExtent.depth   =1;
    }

    void SetRectScope(uint32_t width,uint32_t height)
    {
        imageOffset.x       =0;
        imageOffset.y       =0;
        imageOffset.z       =0;
        imageExtent.width   =width;
        imageExtent.height  =height;
        imageExtent.depth   =1;
    }

    template<typename T>
    void SetRectScope(const RectScope2<T> &rs)
    {
        imageOffset.x       =rs.Left;
        imageOffset.y       =rs.Top;
        imageOffset.z       =0;
        imageExtent.width   =rs.Width;
        imageExtent.height  =rs.Height;
        imageExtent.depth   =1;
    }
};//
VK_NAMESPACE_END
