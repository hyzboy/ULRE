#include<hgl/graph/module/TextureManager.h>
#include<hgl/vk/VKImageCreateInfo.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKBufferOwner.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKDevice.h>
#include"CopyBufferToImage.h"
namespace hgl::graph{
void GenerateMipmaps(TextureCmdBuffer *texture_cmd_buf,VkImage image,VkImageAspectFlags aspect_mask,VkExtent3D extent,const uint32_t mipLevels,const uint32_t layer_count);

namespace
{
    static bool ComputeBCLevelBytesByFormat(const VkFormat format,
                                            const uint32_t width,
                                            const uint32_t height,
                                            uint32_t &out_level_bytes)
    {
        uint32_t block_bytes = 0;

        switch(format)
        {
            case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
            case VK_FORMAT_BC4_UNORM_BLOCK:
            case VK_FORMAT_BC4_SNORM_BLOCK:
                block_bytes = 8;
                break;

            case VK_FORMAT_BC2_UNORM_BLOCK:
            case VK_FORMAT_BC2_SRGB_BLOCK:
            case VK_FORMAT_BC3_UNORM_BLOCK:
            case VK_FORMAT_BC3_SRGB_BLOCK:
            case VK_FORMAT_BC5_UNORM_BLOCK:
            case VK_FORMAT_BC5_SNORM_BLOCK:
            case VK_FORMAT_BC6H_UFLOAT_BLOCK:
            case VK_FORMAT_BC6H_SFLOAT_BLOCK:
            case VK_FORMAT_BC7_UNORM_BLOCK:
            case VK_FORMAT_BC7_SRGB_BLOCK:
                block_bytes = 16;
                break;

            default:
                return false;
        }

        const uint32_t blocks_x = (width  + 3u) / 4u;
        const uint32_t blocks_y = (height + 3u) / 4u;
        out_level_bytes = blocks_x * blocks_y * block_bytes;
        return true;
    }
}

Texture2D *TextureManager::CreateTexture2D(TextureData *tex_data)
{
    if(!tex_data)
        return(nullptr);

    Texture2D *tex=new Texture2D(this,AcquireID(),tex_data);

    Add(tex);

    return tex;
}

void TextureManager::Clear(TextureCreateInfo *tci)
{
    if(!tci)return;

    VulkanDevice *owner = GetDevice();

    if(tci->allocation && tci->image)
    {
        if(owner)
        {
            owner->UntrackObject(VK_OBJECT_TYPE_IMAGE, (uint64_t)(uintptr_t)tci->image);
            if(tci->vk_memory)
                owner->UntrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)tci->vk_memory);
            vmaDestroyImage(owner->GetVmaAllocator(), tci->image, tci->allocation);
        }
        else
        {
            DestroyImage(tci->image);
        }
    }
    else if(tci->image)
    {
        DestroyImage(tci->image);
    }

    if(tci->image_view)delete tci->image_view;

    delete tci;
}

Texture2D *TextureManager::CreateTexture2D(TextureCreateInfo *tci)
{
    if(!tci)return(nullptr);

    if(tci->extent.width*tci->extent.height<=0)
    {
        Clear(tci);
        return(nullptr);
    }

    if(tci->target_mipmaps==0)
        tci->target_mipmaps=(tci->origin_mipmaps>1?tci->origin_mipmaps:1);

    if(!tci->image)
    {
        Image2DCreateInfo ici(tci->usage,tci->tiling,tci->format,tci->extent,tci->target_mipmaps);

        VmaAllocationCreateInfo alloc_ci{};
        alloc_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        VulkanDevice *owner = GetDevice();
        if(vmaCreateImage(owner->GetVmaAllocator(),
                          static_cast<const VkImageCreateInfo *>(&ici),
                          &alloc_ci,
                          &tci->image,
                          &tci->allocation,
                          nullptr)!=VK_SUCCESS)
        {
            Clear(tci);
            return(nullptr);
        }

        VmaAllocationInfo ai{};
        vmaGetAllocationInfo(owner->GetVmaAllocator(), tci->allocation, &ai);
        tci->vk_memory = ai.deviceMemory;

        owner->TrackObject(VK_OBJECT_TYPE_IMAGE,
                           (uint64_t)(uintptr_t)tci->image,
                           ObjectNameBuilder(tci->name.IsEmpty() ? "Texture2D" : (const char*)tci->name.c_str()));
        if(tci->vk_memory)
            owner->TrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY,
                               (uint64_t)(uintptr_t)tci->vk_memory,
                               ObjectNameBuilder(tci->name.IsEmpty() ? "Texture2DMemory" : (const char*)tci->name.c_str()));
    }

    if(!tci->image_view)
        tci->image_view=CreateImageView2D(GetVkDevice(),tci->format,tci->extent,tci->target_mipmaps,tci->aspect,tci->image);

    Texture2D *tex=CreateTexture2D(new TextureData(tci));

    if(!tex)
    {
        Clear(tci);
        return(nullptr);
    }

    if((!tci->buffer)&&tci->pixels&&tci->total_bytes>0)
        tci->buffer=CreateTransferSourceBuffer(tci->total_bytes,tci->pixels);

    if(tci->buffer)
    {
        texture_cmd_buf->Begin();

        if(tci->target_mipmaps==tci->origin_mipmaps)
        {
            if(tci->target_mipmaps<=1)
            {
                CommitTexture2D(tex,tci->buffer->GetBuffer(),VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }
            else
            {
                CommitTexture2DMipmaps(tex,tci->buffer->GetBuffer(),tci->extent,tci->mipmap_zero_total_bytes);
            }
        }
        else
        if(tci->origin_mipmaps<=1)
        {
            CommitTexture2D(tex,tci->buffer->GetBuffer(),VK_PIPELINE_STAGE_TRANSFER_BIT);
            GenerateMipmaps(texture_cmd_buf,tex->GetImage(),tex->GetAspect(),tci->extent,tci->target_mipmaps,1);
        }

        texture_cmd_buf->End();

        SubmitTexture(*texture_cmd_buf);

        delete tci->buffer;
    }

    delete tci;
    return tex;
}

bool TextureManager::CommitTexture2D(Texture2D *tex,VkBuffer buf,VkPipelineStageFlags destinationStage)
{
    if(!tex||buf==VK_NULL_HANDLE)return(false);

    BufferImageCopy buffer_image_copy(tex);

    return CopyBufferToImage2D(tex,buf,&buffer_image_copy,destinationStage);
}

bool TextureManager::CommitTexture2DMipmaps(Texture2D *tex,VkBuffer buf,const VkExtent3D &extent,uint32_t total_bytes)
{
    if(!tex||!buf
      ||extent.width*extent.height<=0)
        return(false);

    const uint32_t miplevel=tex->GetMipLevel();

    AutoDeleteArray<VkBufferImageCopy> buffer_image_copy(miplevel);

    VkDeviceSize offset=0;
    uint32_t level=0;

    uint32_t width=extent.width;
    uint32_t height=extent.height;
    uint32_t rolling_level_bytes = total_bytes;

    buffer_image_copy.zero();

    for(VkBufferImageCopy &bic:buffer_image_copy)
    {
        bic.bufferOffset      = offset;
        bic.bufferRowLength   = 0;
        bic.bufferImageHeight = 0;
        bic.imageSubresource.aspectMask       = tex->GetAspect();
        bic.imageSubresource.mipLevel         = level++;
        bic.imageSubresource.baseArrayLayer   = 0;
        bic.imageSubresource.layerCount       = 1;
        bic.imageOffset.x     = 0;
        bic.imageOffset.y     = 0;
        bic.imageOffset.z     = 0;
        bic.imageExtent.width = width;
        bic.imageExtent.height= height;
        bic.imageExtent.depth = 1;

        const bool can_half_width  = (width  > 1);
        const bool can_half_height = (height > 1);

        uint32_t level_bytes = 0;
        if(ComputeBCLevelBytesByFormat(tex->GetFormat(), width, height, level_bytes))
        {
            offset += level_bytes;
        }
        else
        {
            if(rolling_level_bytes < 8)
                offset += 8;
            else
                offset += rolling_level_bytes;

            if(can_half_width)  rolling_level_bytes >>= 1;
            if(can_half_height) rolling_level_bytes >>= 1;
        }

        if(can_half_width)  width  >>= 1;
        if(can_half_height) height >>= 1;
    }

    return CopyBufferToImage2D(tex,buf,buffer_image_copy,miplevel,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

bool TextureManager::ChangeTexture2D(Texture2D *tex,VkBufferOwner *buf_dev,const std::vector<Image2DRegion> &ir_list,VkPipelineStageFlags destinationStage)
{
    if(!tex||!buf_dev||ir_list.size()<=0)
        return(false);

    const VkBuffer buf=buf_dev->GetBuffer();

    const int ir_count=(int)ir_list.size();
    int count=0;

    AutoDeleteArray<VkBufferImageCopy> buffer_image_copy(ir_count);
    VkBufferImageCopy *tp=buffer_image_copy;

    VkDeviceSize offset=0;

    for(const Image2DRegion &sp:ir_list)
    {
        tp->bufferOffset      = offset;
        tp->bufferRowLength   = 0;
        tp->bufferImageHeight = 0;
        tp->imageSubresource.aspectMask       = tex->GetAspect();
        tp->imageSubresource.mipLevel         = 0;
        tp->imageSubresource.baseArrayLayer   = 0;
        tp->imageSubresource.layerCount       = 1;
        tp->imageOffset.x     = sp.left;
        tp->imageOffset.y     = sp.top;
        tp->imageOffset.z     = 0;
        tp->imageExtent.width = sp.width;
        tp->imageExtent.height= sp.height;
        tp->imageExtent.depth = 1;

        offset+=sp.bytes;
        ++tp;
    }

    texture_cmd_buf->Begin();
    bool result=CopyBufferToImage2D(tex,buf,buffer_image_copy,ir_count,destinationStage);
    texture_cmd_buf->End();
    SubmitTexture(*texture_cmd_buf);
    return result;
}

bool TextureManager::ChangeTexture2D(Texture2D *tex,VkBufferOwner *buf,const RectScope2ui &scope,VkPipelineStageFlags destinationStage)
{
    if(!tex||!buf
        ||scope.GetWidth()<=0
        ||scope.GetHeight()<=0
        ||scope.GetRight()>tex->GetWidth()
        ||scope.GetBottom()>tex->GetHeight())
        return(false);

    const VkBuffer vk_buf=buf->GetBuffer();
    BufferImageCopy buffer_image_copy(tex,scope);

    texture_cmd_buf->Begin();
    bool result=CopyBufferToImage2D(tex,vk_buf,&buffer_image_copy,1,destinationStage);
    texture_cmd_buf->End();
    SubmitTexture(*texture_cmd_buf);
    return result;
}

bool TextureManager::ChangeTexture2D(Texture2D *tex,const void *data,const VkDeviceSize size,const RectScope2ui &scope,VkPipelineStageFlags destinationStage)
{
    if(!tex||!data
        ||size<=0
        ||scope.GetWidth()<=0
        ||scope.GetHeight()<=0
        ||scope.GetRight()>tex->GetWidth()
        ||scope.GetBottom()>tex->GetHeight())
        return(false);

    VkBufferOwner *buf=CreateTransferSourceBuffer(size,data);

    bool result=ChangeTexture2D(tex,buf,scope,destinationStage);

    delete buf;
    return(result);
}
}//namespace hgl::graph
