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
    uint32_t            mipmap;     ///<如果值>0表示提供的数据已有mipmaps，如果为0表示自动生成mipmaps
    VkImageAspectFlags  aspect;
    ImageTiling         tiling;

    void *              pixels;
    GPUBuffer *         buffer;

    VkImage             image;
    GPUMemory *         memory;
    ImageView *         image_view;
    VkImageLayout       image_layout;

public:

    TextureCreateInfo()
    {
        hgl_zero(*this);
    }

    TextureCreateInfo(const uint32_t aspect_bit):TextureCreateInfo()
    {
        usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;

        aspect=aspect_bit;
        tiling=ImageTiling::Optimal;
        image_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
};//struct TextureCreateInfo

class AttachmentTextureCreateInfo:public TextureCreateInfo
{
public:

    AttachmentTextureCreateInfo(const uint32_t aspect_bit):TextureCreateInfo()
    {
        aspect=aspect_bit;        
        tiling=ImageTiling::Optimal;

        if(aspect_bit&VK_IMAGE_ASPECT_COLOR_BIT)
        {
            usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            image_layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        else
        if(aspect_bit&VK_IMAGE_ASPECT_DEPTH_BIT)
        {
            usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            image_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
    }
};
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_TEXTURE_CREATE_INFO_INCLUDE