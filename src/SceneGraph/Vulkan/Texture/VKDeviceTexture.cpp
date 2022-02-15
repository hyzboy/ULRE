#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>

VK_NAMESPACE_BEGIN
bool GPUDevice::CheckFormatSupport(const VkFormat format,const uint32_t bits,ImageTiling tiling) const
{
    const VkFormatProperties fp=attr->physical_device->GetFormatProperties(format);
    
    if(tiling==ImageTiling::Optimal)
        return(fp.optimalTilingFeatures&bits);
    else
        return(fp.linearTilingFeatures&bits);
}

void GPUDevice::Clear(TextureCreateInfo *tci)
{
    if(!tci)return;

    if(tci->image)DestroyImage(tci->image);
    if(tci->image_view)delete tci->image_view;
    if(tci->memory)delete tci->memory;

    delete tci;
}

bool GPUDevice::CommitTexture(Texture *tex,GPUBuffer *buf,const VkBufferImageCopy *buffer_image_copy,const int count,const uint32_t layer_count,VkPipelineStageFlags destinationStage)
{
    if(!tex||!buf)
        return(false);

    ImageSubresourceRange subresourceRange(tex->GetAspect(),tex->GetMipLevel(),layer_count);

    texture_cmd_buf->ImageMemoryBarrier(tex->GetImage(),
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        subresourceRange);

    texture_cmd_buf->CopyBufferToImage(
        buf->GetBuffer(),
        tex->GetImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        count,
        buffer_image_copy);

    if(destinationStage==VK_PIPELINE_STAGE_TRANSFER_BIT)                            //接下来还有，一般是给自动生成mipmaps
    {
        //texture_cmd_buf->ImageMemoryBarrier(tex->GetImage(),
        //    VK_PIPELINE_STAGE_TRANSFER_BIT,
        //    VK_PIPELINE_STAGE_TRANSFER_BIT,
        //    VK_ACCESS_TRANSFER_WRITE_BIT,
        //    VK_ACCESS_TRANSFER_READ_BIT,
        //    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        //    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        //    subresourceRange);
    }
    else// if(destinationStage==VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)              //接下来就给fragment shader用了，证明是最后一步
    {
        texture_cmd_buf->ImageMemoryBarrier(tex->GetImage(),
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            subresourceRange);
    }

    return(true);
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
VK_NAMESPACE_END
