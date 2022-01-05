#include<hgl/graph/VKCommandBuffer.h>

VK_NAMESPACE_BEGIN
void GenerateMipmaps2D(TextureCmdBuffer *texture_cmd_buf,VkImage image,VkImageAspectFlags aspect_mask,VkExtent3D extent,const uint32_t mipLevels)
{
    ImageSubresourceRange subresourceRange(aspect_mask);

    VkImageBlit blit;

    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcSubresource.aspectMask = aspect_mask;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;

    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstSubresource=blit.srcSubresource;

    int32_t width   =extent.width;
    int32_t height  =extent.height;
    int32_t depth   =extent.depth;

    for (uint32_t i = 1; i < mipLevels; i++) 
    {
        subresourceRange.baseMipLevel = i - 1;

        texture_cmd_buf->ImageMemoryBarrier(image,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            subresourceRange);

        blit.srcOffsets[1] = {width,height,depth};
        blit.srcSubresource.mipLevel = i - 1;

        if(width >1)width >>=1;
        if(height>1)height>>=1;
        if(depth >1)depth >>=1;

        blit.dstOffsets[1] = {width,height,depth};
        blit.dstSubresource.mipLevel = i;

        texture_cmd_buf->BlitImage(
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        texture_cmd_buf->ImageMemoryBarrier(image,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            subresourceRange);
    }

    subresourceRange.baseMipLevel = mipLevels - 1;

    texture_cmd_buf->ImageMemoryBarrier(image,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        subresourceRange);
}
VK_NAMESPACE_END