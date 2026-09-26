#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include"CopyBufferToImage.h"

namespace hgl::graph{
DeviceBuffer *TextureManager::CreateTransferSourceBuffer(const VkDeviceSize size,const void *data)
{
    if(size<=0)
        return(nullptr);

    return GetDevice()->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,size,data);
}

bool TextureManager::CheckFormatSupport(const VkFormat format,const uint32_t bits,ImageTiling tiling) const
{
    const VkFormatProperties fp=GetFormatProperties(format);

    if(tiling==ImageTiling::Optimal)
        return(fp.optimalTilingFeatures&bits);
    else
        return(fp.linearTilingFeatures&bits);
}

bool TextureManager::CopyBufferToImage(const CopyBufferToImageInfo *info,VkPipelineStageFlags2 destinationStage)
{
    if(!info)
        return(false);

    if(info->bic_count==0)
        return(false);

    texture_cmd_buf->ImageMemoryBarrier2(info->image,
        VK_PIPELINE_STAGE_2_NONE,
        VK_PIPELINE_STAGE_2_COPY_BIT,
        VK_ACCESS_2_NONE,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        info->isr);

    texture_cmd_buf->CopyBufferToImage(
        info->buffer,
        info->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        info->bic_count,
        info->bic_list);

    if(destinationStage==VK_PIPELINE_STAGE_2_COPY_BIT || destinationStage==VK_PIPELINE_STAGE_2_BLIT_BIT)
    {
        // 接下来有后续 Blit/Mipmap 处理，由调用者或后续步骤负责转换
    }
    else
    {
        texture_cmd_buf->ImageMemoryBarrier2(info->image,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            destinationStage,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            info->isr);
    }

    return(true);
}

bool TextureManager::CopyBufferToImage(Texture *tex,VkBuffer buf,const VkBufferImageCopy *buffer_image_copy,const int count,const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags2 destinationStage)
{
    if(!tex||buf==VK_NULL_HANDLE)
        return(false);

    CopyBufferToImageInfo info;

    info.image      =tex->GetImage();
    info.buffer     =buf;

    info.isr.aspectMask     =tex->GetAspect();
    info.isr.baseMipLevel   =0;
    info.isr.levelCount     =tex->GetMipLevel();
    info.isr.baseArrayLayer =base_layer;
    info.isr.layerCount     =layer_count;

    info.bic_list           =buffer_image_copy;
    info.bic_count          =count;

    return CopyBufferToImage(&info,destinationStage);
}

bool TextureManager::SubmitTexture(const VkCommandBuffer *cmd_bufs,const uint32_t count)
{
    if(!cmd_bufs||count<=0)
        return(false);

    texture_queue->Submit(cmd_bufs,count,nullptr,0,nullptr,0);
//    texture_queue->WaitQueue();
    texture_queue->WaitLastSubmitFence();

    return(true);
}
}//namespace hgl::graph
