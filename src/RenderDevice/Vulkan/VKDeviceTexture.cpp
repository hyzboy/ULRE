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

Texture2D *Device::CreateTexture2D(Memory *mem,VkImage image,ImageView *image_view,VkImageLayout image_layout,VkImageTiling tiling)
{
    TextureData *tex_data=new TextureData;

    tex_data->memory        = mem;
    tex_data->image_layout  = image_layout;
    tex_data->image         = image;
    tex_data->image_view    = image_view;

    tex_data->mip_levels    = 0;
    tex_data->tiling        = tiling;

    return(new Texture2D(attr->device,tex_data));
}

Texture2D *Device::CreateTexture2D(VkFormat format,uint32_t width,uint32_t height,VkImageAspectFlagBits aspectMask,VkImage image,VkImageLayout image_layout,VkImageTiling tiling)
{
    VkExtent3D extent={width,height,1};

    ImageView *iv=CreateImageView(attr->device,VK_IMAGE_VIEW_TYPE_2D,format,extent,aspectMask,image);

    return this->CreateTexture2D(nullptr,image,iv,image_layout,tiling);
}

Texture2D *Device::CreateTexture2D(const VkFormat format,uint32_t width,uint32_t height,const VkImageAspectFlags aspectMask,const uint usage,const VkImageLayout image_layout,VkImageTiling tiling)
{
    const VkFormatProperties fp=attr->physical_device->GetFormatProperties(format);

    if(tiling==VK_IMAGE_TILING_OPTIMAL)
    {
        if(fp.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
            tiling=VK_IMAGE_TILING_OPTIMAL;
        else
            tiling=VK_IMAGE_TILING_LINEAR;
    }

    if(tiling==VK_IMAGE_TILING_LINEAR)
    {
        if(fp.linearTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
            tiling=VK_IMAGE_TILING_LINEAR;
        else 
            return(nullptr);
    }

    VkImage img=CreateImage2D(format,width,height,usage,tiling);

    if(!img)return(nullptr);

    Memory *mem=CreateMemory(img);

    if(!mem->BindImage(img))
    {
        delete mem;
        DestoryImage(img);
        return(nullptr);
    }

    const VkExtent3D ext={width,height,1};

    ImageView *iv=CreateImageView2D(attr->device,format,ext,aspectMask,img);

    return CreateTexture2D(mem,img,iv,image_layout,tiling);
}

Texture2D *Device::CreateTexture2D(const VkFormat format,Buffer *buf,uint32_t width,uint32_t height,const VkImageAspectFlags aspectMask,const uint usage,const VkImageLayout image_layout,const VkImageTiling tiling)
{
    if(!buf)return(nullptr);

    Texture2D *tex=CreateTexture2D(format,width,height,aspectMask,usage,image_layout,tiling);

    if(!tex)return(nullptr);

    ChangeTexture2D(tex,buf,0,0,width,height);

    return(tex);
}

Texture2D *Device::CreateTexture2D(const VkFormat format,void *data,uint32_t width,uint32_t height,uint32_t size,const VkImageAspectFlags aspectMask,const uint usage,const VkImageLayout image_layout,const VkImageTiling tiling)
{
    Buffer *buf=CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,size,data);

    if(!buf)return(nullptr);

    Texture2D *tex=CreateTexture2D(format,buf,width,height,aspectMask,image_layout,tiling);

    delete buf;
    return(tex);
}

bool Device::ChangeTexture2D(Texture2D *tex,Buffer *buf,const VkBufferImageCopy *buffer_image_copy,const int count)
{
    if(!tex||!buf)
        return(false);

    VkImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask     = tex->GetAspect();
    subresourceRange.baseMipLevel   = 0;
    subresourceRange.levelCount     = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount     = 1;

    VkImageMemoryBarrier imageMemoryBarrier;
    imageMemoryBarrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.pNext                = nullptr;
    imageMemoryBarrier.srcAccessMask        = 0;
    imageMemoryBarrier.dstAccessMask        = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.oldLayout            = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout            = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image                = tex->GetImage();
    imageMemoryBarrier.subresourceRange     = subresourceRange;

    texture_cmd_buf->Begin();
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
    return(true);
}

bool Device::ChangeTexture2D(Texture2D *tex,Buffer *buf,const List<ImageRegion> &ir_list)
{
    if(!tex||!buf||ir_list.GetCount()<=0)
        return(false);

    const int ir_count=ir_list.GetCount();
    int count=0;

    AutoDeleteArray<VkBufferImageCopy> buffer_image_copy=new VkBufferImageCopy[ir_count];
    VkBufferImageCopy *tp=buffer_image_copy;
    const ImageRegion *sp=ir_list.GetData();

    VkDeviceSize offset=0;

    for(int i=0;i<ir_list.GetCount();i++)
    {
        tp->bufferOffset      = offset;
        tp->bufferRowLength   = 0;
        tp->bufferImageHeight = 0;
        tp->imageSubresource.aspectMask       = tex->GetAspect();
        tp->imageSubresource.mipLevel         = 0;
        tp->imageSubresource.baseArrayLayer   = 0;
        tp->imageSubresource.layerCount       = 1;
        tp->imageOffset.x     = sp->left;
        tp->imageOffset.y     = sp->top;
        tp->imageOffset.z     = 0;
        tp->imageExtent.width = sp->width;
        tp->imageExtent.height= sp->height;
        tp->imageExtent.depth = 1;

        offset+=sp->bytes;
        ++sp;
        ++tp;
    }

    return ChangeTexture2D(tex,buf,buffer_image_copy,ir_count);
}

bool Device::ChangeTexture2D(Texture2D *tex,Buffer *buf,uint32_t left,uint32_t top,uint32_t width,uint32_t height)
{
    if(!tex||!buf
     ||left<0||left+width>tex->GetWidth()
     ||top<0||top+height>tex->GetHeight()
     ||width<=0||height<=0)
        return(false);

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
    
    return ChangeTexture2D(tex,buf,&buffer_image_copy,1);
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
    
    bool result=ChangeTexture2D(tex,buf,left,top,width,height);

    delete buf;

    return(result);
}

bool Device::SubmitTexture(const VkCommandBuffer *cmd_bufs,const uint32_t count)
{
    if(!cmd_bufs||count<=0)
        return(false);

    textureSQ->Submit(cmd_bufs,count,nullptr,nullptr);
    textureSQ->Wait();

    return(true);
}

Sampler *Device::CreateSampler(VkSamplerCreateInfo *sci)
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
        0,
        false,
        VK_COMPARE_OP_NEVER,
        0.0f,
        1.0f,
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
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
VK_NAMESPACE_END
