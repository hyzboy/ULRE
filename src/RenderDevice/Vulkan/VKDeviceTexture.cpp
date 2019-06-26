#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKSampler.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<hgl/graph/vulkan/VKFence.h>
#include<hgl/graph/vulkan/VKBuffer.h>
#include<hgl/graph/vulkan/VKCommandBuffer.h>
#include<hgl/graph/vulkan/VKMemory.h>
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

Texture2D *Device::CreateTexture2D(const VkFormat video_format,uint32_t width,uint32_t height,const VkImageAspectFlags aspectMask,const uint usage,const VkImageLayout image_layout)
{
    if(video_format<VK_FORMAT_BEGIN_RANGE||video_format>VK_FORMAT_END_RANGE)return(nullptr);
    if(width<1||height<1)return(nullptr);

    const VkFormatProperties fp=attr->physical_device->GetFormatProperties(video_format);

    if(!(fp.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
     &&!(fp.linearTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
        return(nullptr);
   
    VkImageCreateInfo imageCreateInfo;

    imageCreateInfo.sType                   = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext                   = nullptr;
    imageCreateInfo.flags                   = 0;
    imageCreateInfo.imageType               = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format                  = video_format;
    imageCreateInfo.extent.width            = width;
    imageCreateInfo.extent.height           = height;
    imageCreateInfo.extent.depth            = 1;
    imageCreateInfo.mipLevels               = 1;
    imageCreateInfo.arrayLayers             = 1;
    imageCreateInfo.samples                 = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.usage                   = usage;
    imageCreateInfo.sharingMode             = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount   = 0;
    imageCreateInfo.pQueueFamilyIndices     = nullptr;
    imageCreateInfo.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.tiling                  = (fp.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)?VK_IMAGE_TILING_OPTIMAL:VK_IMAGE_TILING_LINEAR;

    VkImage image;

    if(vkCreateImage(attr->device, &imageCreateInfo, nullptr, &image)!=VK_SUCCESS)
        return(nullptr);

    VkMemoryRequirements memReqs;

    vkGetImageMemoryRequirements(attr->device, image, &memReqs);

    Memory *dm=CreateMemory(memReqs,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
     
    if(dm&&dm->Bind(image))
    {
        ImageView *image_view=CreateImageView2D(attr->device,video_format,aspectMask,image);

        if(image_view)
        {
            TextureData *tex_data=new TextureData;

            tex_data->ref           = false;
            tex_data->mip_levels    = 0;
            tex_data->memory        = dm;
            tex_data->image_layout  = image_layout;
            tex_data->image         = image;
            tex_data->image_view    = image_view;
            tex_data->format        = video_format;
            tex_data->aspect        = aspectMask;
            tex_data->extent        = imageCreateInfo.extent;
    
            return(new Texture2D(width,height,attr->device,tex_data));
        }
    }

    delete dm;
    vkDestroyImage(attr->device,image,nullptr);
    return(nullptr);
}

Texture2D *Device::CreateTexture2D(const VkFormat video_format,void *data,uint32_t width,uint32_t height,uint32_t size,const VkImageAspectFlags aspectMask,const uint usage,const VkImageLayout image_layout)
{
    Texture2D *tex=CreateTexture2D(video_format,width,height,aspectMask,usage,image_layout);

    if(!tex)return(nullptr);

    ChangeTexture2D(tex,data,0,0,width,height,size);

    return(tex);
}

Texture2D *Device::CreateRefTexture2D(uint32_t width,uint32_t height,VkFormat format,VkImageAspectFlagBits aspectMask,VkImage image,VkImageLayout image_layout,VkImageView image_view)
{
    TextureData *tex_data=new TextureData;

    tex_data->ref           =true;

    tex_data->memory        =nullptr;
    tex_data->image         =image;
    tex_data->image_layout  =image_layout;
    tex_data->image_view    =CreateRefImageView(attr->device,VK_IMAGE_VIEW_TYPE_2D,format,aspectMask,image_view);

    tex_data->mip_levels    =0;
    tex_data->linear        =false;
    tex_data->format        =format;
    tex_data->aspect        =aspectMask;

    tex_data->extent.width  =width;
    tex_data->extent.height =height;
    tex_data->extent.depth  =1;

    return(new Texture2D(width,height,attr->device,tex_data));
}

bool Device::ChangeTexture2D(Texture2D *tex,void *data,uint32_t left,uint32_t top,uint32_t width,uint32_t height,uint32_t size)
{
    if(!tex||!data
     ||left<0||left+width>tex->GetWidth()
     ||top<0||top+height>tex->GetHeight()
     ||width<=0||height<=0
     ||size<=0)
        return(false);

    Buffer *buf=CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,size,data);

    VkBufferImageCopy buffer_image_copy;
    buffer_image_copy.bufferOffset      = 0;
    buffer_image_copy.bufferRowLength   = 0;
    buffer_image_copy.bufferImageHeight = 0;
    buffer_image_copy.imageSubresource.aspectMask       = tex->GetAspect();
    buffer_image_copy.imageSubresource.mipLevel         = 0;
    buffer_image_copy.imageSubresource.baseArrayLayer   = 0;
    buffer_image_copy.imageSubresource.layerCount       = 1;
    buffer_image_copy.imageOffset.x     = left;
    buffer_image_copy.imageOffset.y     = top;
    buffer_image_copy.imageOffset.z     = 0;
    buffer_image_copy.imageExtent.width = width;
    buffer_image_copy.imageExtent.height= height;
    buffer_image_copy.imageExtent.depth = 1;

    VkImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask     = tex->GetAspect();
    subresourceRange.baseMipLevel   = 0;
    subresourceRange.levelCount     = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount     = 1;

    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = *tex;
    imageMemoryBarrier.subresourceRange = subresourceRange;
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    texture_cmd_buf->Begin();
    texture_cmd_buf->PipelineBarrier(
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);

    texture_cmd_buf->CopyBufferToImage(
        *buf,
        *tex,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &buffer_image_copy);

    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    texture_cmd_buf->PipelineBarrier(
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);

    texture_cmd_buf->End();

    SubmitTexture(*texture_cmd_buf);

    delete buf;
    return(true);
}

bool Device::SubmitTexture(const VkCommandBuffer *cmd_bufs,const uint32_t count)
{
    if(!cmd_bufs||count<=0)
        return(false);

    texture_submit_info.commandBufferCount = count;
    texture_submit_info.pCommandBuffers = cmd_bufs;

    VkFence fence=*texture_fence;
    
    if(vkQueueSubmit(attr->graphics_queue, 1, &texture_submit_info, fence))return(false);
    if(vkWaitForFences(attr->device, 1, &fence, VK_TRUE, HGL_NANO_SEC_PER_SEC*0.1)!=VK_SUCCESS)return(false);    
    vkResetFences(attr->device,1,&fence);

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
