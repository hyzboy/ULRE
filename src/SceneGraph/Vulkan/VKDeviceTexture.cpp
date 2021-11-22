#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKFence.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKMemory.h>
#include<hgl/graph/VKImageCreateInfo.h>
VK_NAMESPACE_BEGIN
namespace
{
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

        void Set(const VkImageAspectFlags aspect_mask,const uint32_t layer_count)
        {
            imageSubresource.aspectMask=aspect_mask;
            imageSubresource.layerCount=layer_count;
        }

        void Set(ImageRegion *ir)
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

    void GenerateMipmaps(TextureCmdBuffer *texture_cmd_buf,VkImage image,VkImageAspectFlags aspect_mask,const int32_t width,const int32_t height,const uint32_t mipLevels)
    {
    //VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
        // Check if image format supports linear blitting

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = aspect_mask;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        int32_t mipWidth = width;
        int32_t mipHeight = height;

        for (uint32_t i = 1; i < mipLevels; i++) 
        {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            texture_cmd_buf->PipelineBarrier(
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = aspect_mask;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask = aspect_mask;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            texture_cmd_buf->BlitImage(
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            texture_cmd_buf->PipelineBarrier(
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        texture_cmd_buf->PipelineBarrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
    }
}//namespace

bool GPUDevice::CheckFormatSupport(const VkFormat format,const uint32_t bits,ImageTiling tiling) const
{
    const VkFormatProperties fp=attr->physical_device->GetFormatProperties(format);
    
    if(tiling==ImageTiling::Optimal)
        return(fp.optimalTilingFeatures&bits);
    else
        return(fp.linearTilingFeatures&bits);
}

Texture2D *GPUDevice::CreateTexture2D(TextureData *tex_data)
{
    if(!tex_data)
        return(nullptr);

    return(new Texture2D(attr->device,tex_data));
}

void GPUDevice::Clear(TextureCreateInfo *tci)
{
    if(!tci)return;

    if(tci->image)DestroyImage(tci->image);
    if(tci->image_view)delete tci->image_view;
    if(tci->memory)delete tci->memory;

    delete tci;
}

Texture2D *GPUDevice::CreateTexture2D(TextureCreateInfo *tci)
{
    if(!tci)return(nullptr);

    if(tci->extent.width*tci->extent.height*tci->extent.depth<=0)return(nullptr);

    if(tci->target_mipmaps==0)
        tci->target_mipmaps=(tci->origin_mipmaps>1?tci->origin_mipmaps:1);

    if(!tci->image)
    {
        Image2DCreateInfo ici(tci->usage,tci->tiling,tci->format,tci->extent.width,tci->extent.height,tci->target_mipmaps);
        tci->image=CreateImage(&ici);

        if(!tci->image)
        {
            Clear(tci);
            return(nullptr);
        }

        tci->memory=CreateMemory(tci->image);
    }

    if(!tci->image_view)
        tci->image_view=CreateImageView2D(attr->device,tci->format,tci->extent,tci->target_mipmaps,tci->aspect,tci->image);

    TextureData *tex_data=new TextureData(tci);

    Texture2D *tex=CreateTexture2D(tex_data);

    if(!tex)
    {
        Clear(tci);
        return(nullptr);
    }

    if((!tci->buffer)&&tci->pixels&&tci->total_bytes>0)
        tci->buffer=CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,tci->total_bytes,tci->pixels);

    if(tci->buffer)
    {
        texture_cmd_buf->Begin();
            if(tci->target_mipmaps==tci->origin_mipmaps)
            {
                if(tci->target_mipmaps<=1)      //本身不含mipmaps，但也不要mipmaps
                {
                    CommitTexture2D(tex,tci->buffer,tci->extent.width,tci->extent.height,tex_data->miplevel,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                }
                else //本身有mipmaps数据
                {
                    CommitTexture2DMipmaps(tex,tci->buffer,tci->extent.width,tci->extent.height,tex_data->miplevel,tci->mipmap_zero_total_bytes);
                }
            }
            else
            if(tci->origin_mipmaps<=1)          //本身不含mipmaps数据,又想要mipmaps
            {
                CommitTexture2D(tex,tci->buffer,tci->extent.width,tci->extent.height,tex_data->miplevel,VK_PIPELINE_STAGE_TRANSFER_BIT);
                GenerateMipmaps(texture_cmd_buf,tex->GetImage(),tex->GetAspect(),tex->GetWidth(),tex->GetHeight(),tex_data->miplevel);              
            }
        texture_cmd_buf->End();

        SubmitTexture(*texture_cmd_buf);

        delete tci->buffer;
    }

    delete tci;
    return tex;
}

bool GPUDevice::CommitTexture2D(Texture2D *tex,GPUBuffer *buf,const VkBufferImageCopy *buffer_image_copy,const int count,const uint32_t miplevel,VkPipelineStageFlags destinationStage)
{
    if(!tex||!buf)
        return(false);

    VkImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask     = tex->GetAspect();
    subresourceRange.baseMipLevel   = 0;
    subresourceRange.levelCount     = miplevel;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount     = 1;

    ImageMemoryBarrier imageMemoryBarrier(tex->GetImage());
    imageMemoryBarrier.srcAccessMask        = 0;
    imageMemoryBarrier.dstAccessMask        = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.oldLayout            = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout            = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.subresourceRange     = subresourceRange;

    texture_cmd_buf->PipelineBarrier(
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);

    texture_cmd_buf->CopyBufferToImage(
        buf->GetBuffer(),
        tex->GetImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        count,
        buffer_image_copy);
        
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    
    if(destinationStage==VK_PIPELINE_STAGE_TRANSFER_BIT)
    {
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }
    else// if(destinationStage==VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
    {
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    texture_cmd_buf->PipelineBarrier(
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);

    return(true);
}

bool GPUDevice::CommitTexture2D(Texture2D *tex,GPUBuffer *buf,uint32_t width,uint32_t height,const uint32_t miplevel,VkPipelineStageFlags destinationStage)
{
    if(!tex||!buf)return(false);

    BufferImageCopy buffer_image_copy(tex);
    
    return CommitTexture2D(tex,buf,&buffer_image_copy,1,miplevel,destinationStage);
}

bool GPUDevice::CommitTexture2DMipmaps(Texture2D *tex,GPUBuffer *buf,uint32_t width,uint32_t height,uint32_t miplevel,uint32_t total_bytes)
{
    if(!tex||!buf
     ||width<=0||height<=0)
        return(false);

    AutoDeleteArray<VkBufferImageCopy> buffer_image_copy(miplevel);
    
    VkDeviceSize offset=0;
    uint32_t level=0;

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

        if(total_bytes<8)
            offset+=8;
        else
            offset+=total_bytes;

        if(width>1){width>>=1;total_bytes>>=1;}
        if(height>1){height>>=1;total_bytes>>=1;}
    }
    
    return CommitTexture2D(tex,buf,buffer_image_copy,miplevel,miplevel,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

bool GPUDevice::ChangeTexture2D(Texture2D *tex,GPUBuffer *buf,const List<ImageRegion> &ir_list,const uint32_t miplevel,VkPipelineStageFlags destinationStage)
{
    if(!tex||!buf||ir_list.GetCount()<=0)
        return(false);

    const int ir_count=ir_list.GetCount();
    int count=0;

    AutoDeleteArray<VkBufferImageCopy> buffer_image_copy(ir_count);
    VkBufferImageCopy *tp=buffer_image_copy;

    VkDeviceSize offset=0;

    for(const ImageRegion &sp:ir_list)
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
    bool result=CommitTexture2D(tex,buf,buffer_image_copy,ir_count,miplevel,destinationStage);
    texture_cmd_buf->End();
    SubmitTexture(*texture_cmd_buf);
    return result;
}

bool GPUDevice::ChangeTexture2D(Texture2D *tex,GPUBuffer *buf,uint32_t left,uint32_t top,uint32_t width,uint32_t height,const uint32_t miplevel,VkPipelineStageFlags destinationStage)
{
    if(!tex||!buf
     ||left<0||left+width>tex->GetWidth()
     ||top<0||top+height>tex->GetHeight()
     ||width<=0||height<=0)
        return(false);

    BufferImageCopy buffer_image_copy(tex);
    
    texture_cmd_buf->Begin();
    bool result=CommitTexture2D(tex,buf,&buffer_image_copy,1,miplevel,destinationStage);    
    texture_cmd_buf->End();
    SubmitTexture(*texture_cmd_buf);
    return result;
}

bool GPUDevice::ChangeTexture2D(Texture2D *tex,void *data,uint32_t left,uint32_t top,uint32_t width,uint32_t height,uint32_t size,const uint32_t miplevel,VkPipelineStageFlags destinationStage)
{
    if(!tex||!data
     ||left<0||left+width>tex->GetWidth()
     ||top<0||top+height>tex->GetHeight()
     ||width<=0||height<=0
     ||size<=0)
        return(false);

    GPUBuffer *buf=CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,size,data);
    
    bool result=ChangeTexture2D(tex,buf,left,top,width,height,miplevel,destinationStage);    

    delete buf;
    return(result);
}

bool GPUDevice::SubmitTexture(const VkCommandBuffer *cmd_bufs,const uint32_t count)
{
    if(!cmd_bufs||count<=0)
        return(false);

    texture_queue->Submit(cmd_bufs,count,nullptr,nullptr);
//    texture_queue->WaitQueue();
    texture_queue->WaitFence();

    return(true);
}

Sampler *GPUDevice::CreateSampler(VkSamplerCreateInfo *sci)
{
    static VkSamplerCreateInfo default_sampler_create_info=
    {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0,
        VK_FILTER_LINEAR,
        VK_FILTER_LINEAR,
        VK_SAMPLER_MIPMAP_MODE_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        0.0f,
        false,
        1.0f,
        false,
        VK_COMPARE_OP_NEVER,
        0.0f,
        0.0f,
        VK_BORDER_COLOR_INT_OPAQUE_BLACK,//VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        false
    };

    if(!sci)
        sci=&default_sampler_create_info;

    VkSampler sampler;

    sci->sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    //if(attr->physical_device->features.samplerAnisotropy)         //不知道为什么不准，先全部禁用吧
    //{
    //    sci->maxAnisotropy = attr->physical_device->properties.limits.maxSamplerAnisotropy;
    //    sci->anisotropyEnable = VK_TRUE;
    //} 
    //else 
    {
        sci->maxAnisotropy = 1.0;
        sci->anisotropyEnable = VK_FALSE;
    }

    if(vkCreateSampler(attr->device,sci,nullptr,&sampler)!=VK_SUCCESS)
        return(nullptr);

    return(new Sampler(attr->device,sampler));
}


Sampler *GPUDevice::CreateSampler(Texture *tex)
{
    VkSamplerCreateInfo sci=
    {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0,
        VK_FILTER_LINEAR,
        VK_FILTER_LINEAR,
        VK_SAMPLER_MIPMAP_MODE_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        0.0f,
        false,
        1.0f,
        false,
        VK_COMPARE_OP_NEVER,
        0.0f,
        0.0f,
        VK_BORDER_COLOR_INT_OPAQUE_BLACK,//VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        false
    };

    VkSampler sampler;
    
    sci.anisotropyEnable = attr->physical_device->SupportSamplerAnisotropy();

    if(sci.anisotropyEnable)
        sci.maxAnisotropy = attr->physical_device->GetMaxSamplerAnisotropy();

    if(tex)
        sci.maxLod=tex->GetMipLevel();

    if(vkCreateSampler(attr->device,&sci,nullptr,&sampler)!=VK_SUCCESS)
        return(nullptr);

    return(new Sampler(attr->device,sampler));
}
VK_NAMESPACE_END
