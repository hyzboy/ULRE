#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKSampler.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<hgl/graph/vulkan/VKFence.h>
#include<hgl/graph/vulkan/VKBuffer.h>
#include<hgl/graph/vulkan/VKCommandBuffer.h>
VK_NAMESPACE_BEGIN
namespace
{
    uint32_t GetMipLevels(uint32_t size)
    {
        uint32_t level=1;

        while(size>>=1)
            ++level;

        return level;
    }
}//namespace

Texture2D *Device::CreateTexture2D(const VkFormat video_format,void *data,uint32_t width,uint32_t height,uint32_t size,bool force_linear)
{
    if(!data||width<=1||height<=1)return(nullptr);

    const VkFormatProperties fp=attr->physical_device->GetFormatProperties(video_format);

    if(fp.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)        
    {
        if(force_linear)
        {
            if(!(fp.linearTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
                force_linear=false;
        }
    }
    else    //不能用优化存储，这种现像好像不存在啊    
    {
        if(!(fp.linearTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
        {
            //也不能用线性储存？？WTF ?
            return(nullptr);
        }

        force_linear=true;
        return(nullptr);        //这个我们暂时不支持
    }

    TextureData *tex_data=new TextureData();

    tex_data->memory=nullptr;
    tex_data->image=nullptr;
    tex_data->image_view=nullptr;

    tex_data->mip_levels=1;
    tex_data->linear=false;

    if(force_linear)
    {
        delete tex_data;
        return(nullptr);
    }
    else
    {
    #define VK_CHECK_RESULT(func)   if(func!=VK_SUCCESS){delete tex_data;delete buf;return(nullptr);}

        Buffer *buf=CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,size,data);

        VkBufferImageCopy buffer_image_copy{};
        buffer_image_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        buffer_image_copy.imageSubresource.mipLevel = 0;
        buffer_image_copy.imageSubresource.baseArrayLayer = 0;
        buffer_image_copy.imageSubresource.layerCount = 1;
        buffer_image_copy.imageExtent.width = width;
        buffer_image_copy.imageExtent.height = height;
        buffer_image_copy.imageExtent.depth = 1;
        buffer_image_copy.bufferOffset = 0;

        tex_data->image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = video_format;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // Set initial layout of the image to undefined
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = { width, height, 1 };
        imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        VK_CHECK_RESULT(vkCreateImage(attr->device, &imageCreateInfo, nullptr, &tex_data->image))

        VkMemoryAllocateInfo memAllocInfo{};
        VkMemoryRequirements memReqs{};

        memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

        vkGetImageMemoryRequirements(attr->device, tex_data->image, &memReqs);
        memAllocInfo.allocationSize = memReqs.size;
        attr->CheckMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&memAllocInfo.memoryTypeIndex);
        VK_CHECK_RESULT(vkAllocateMemory(attr->device, &memAllocInfo, nullptr, &tex_data->memory))
        VK_CHECK_RESULT(vkBindImageMemory(attr->device, tex_data->image, tex_data->memory, 0))

        CommandBuffer *cmd_buf=CreateCommandBuffer();
        
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;

        VkImageMemoryBarrier imageMemoryBarrier{};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.image = tex_data->image;
        imageMemoryBarrier.subresourceRange = subresourceRange;
        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        cmd_buf->Begin();
        cmd_buf->PipelineBarrier(
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrier);

        cmd_buf->CopyBufferToImage(
            *buf,
            tex_data->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &buffer_image_copy);

        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        cmd_buf->PipelineBarrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrier);

        cmd_buf->End();

        SubmitTexture(*cmd_buf);

        delete buf;

        tex_data->image_view=CreateImageView2D(attr->device,video_format,VK_IMAGE_ASPECT_COLOR_BIT,tex_data->image);

    #undef VK_CHECK_RESULT
    }

    return(new Texture2D(width,height,attr->device,tex_data));
}

bool Device::SubmitTexture(const VkCommandBuffer *cmd_bufs,const uint32_t count)
{
    if(!cmd_bufs||count<=0)
        return(false);

    texture_submitInfo.commandBufferCount = count;
    texture_submitInfo.pCommandBuffers = cmd_bufs;

    VkFence fence=*texture_fence;

    if(vkQueueSubmit(attr->graphics_queue, 1, &texture_submitInfo, fence))return(false);
    if(vkWaitForFences(attr->device, 1, &fence, VK_TRUE, HGL_NANO_SEC_PER_SEC*0.1)!=VK_SUCCESS)return(false);

    return(true);
}

Sampler *Device::CreateSampler(VkSamplerCreateInfo *sci)
{
    if(!sci)return(nullptr);

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
VK_NAMESPACE_END
