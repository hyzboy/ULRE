#include<hgl/graph/VKTexture.h>
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

        void Set(Image2DRegion *ir)
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

    void GenerateMipmaps(TextureCmdBuffer *texture_cmd_buf,VkImage image,VkImageAspectFlags aspect_mask,VkExtent3D extent,const uint32_t mipLevels)
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
                    CommitTexture2D(tex,tci->buffer,tci->extent.width,tci->extent.height,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                }
                else //本身有mipmaps数据
                {
                    CommitTexture2DMipmaps(tex,tci->buffer,tci->extent.width,tci->extent.height,tci->mipmap_zero_total_bytes);
                }
            }
            else
            if(tci->origin_mipmaps<=1)          //本身不含mipmaps数据,又想要mipmaps
            {
                CommitTexture2D(tex,tci->buffer,tci->extent.width,tci->extent.height,VK_PIPELINE_STAGE_TRANSFER_BIT);
                GenerateMipmaps(texture_cmd_buf,tex->GetImage(),tex->GetAspect(),tci->extent,tex_data->miplevel);              
            }
        texture_cmd_buf->End();

        SubmitTexture(*texture_cmd_buf);

        delete tci->buffer;
    }

    delete tci;
    return tex;
}

bool GPUDevice::CommitTexture2D(Texture2D *tex,GPUBuffer *buf,const VkBufferImageCopy *buffer_image_copy,const int count,VkPipelineStageFlags destinationStage)
{
    if(!tex||!buf)
        return(false);

    ImageSubresourceRange subresourceRange(tex->GetAspect(),tex->GetMipLevel());

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
        texture_cmd_buf->ImageMemoryBarrier(tex->GetImage(),
                                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                                            VK_ACCESS_TRANSFER_WRITE_BIT,
                                            VK_ACCESS_TRANSFER_WRITE_BIT,
                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                            subresourceRange);
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

bool GPUDevice::CommitTexture2D(Texture2D *tex,GPUBuffer *buf,uint32_t width,uint32_t height,VkPipelineStageFlags destinationStage)
{
    if(!tex||!buf)return(false);

    BufferImageCopy buffer_image_copy(tex);
    
    return CommitTexture2D(tex,buf,&buffer_image_copy,1,destinationStage);
}

bool GPUDevice::CommitTexture2DMipmaps(Texture2D *tex,GPUBuffer *buf,uint32_t width,uint32_t height,uint32_t total_bytes)
{
    if(!tex||!buf
     ||width<=0||height<=0)
        return(false);

    const uint32_t miplevel=tex->GetMipLevel();

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
    
    return CommitTexture2D(tex,buf,buffer_image_copy,miplevel,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

bool GPUDevice::ChangeTexture2D(Texture2D *tex,GPUBuffer *buf,const List<Image2DRegion> &ir_list,VkPipelineStageFlags destinationStage)
{
    if(!tex||!buf||ir_list.GetCount()<=0)
        return(false);

    const int ir_count=ir_list.GetCount();
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
    bool result=CommitTexture2D(tex,buf,buffer_image_copy,ir_count,destinationStage);
    texture_cmd_buf->End();
    SubmitTexture(*texture_cmd_buf);
    return result;
}

bool GPUDevice::ChangeTexture2D(Texture2D *tex,GPUBuffer *buf,uint32_t left,uint32_t top,uint32_t width,uint32_t height,VkPipelineStageFlags destinationStage)
{
    if(!tex||!buf
     ||left<0||left+width>tex->GetWidth()
     ||top<0||top+height>tex->GetHeight()
     ||width<=0||height<=0)
        return(false);

    BufferImageCopy buffer_image_copy(tex);
    
    texture_cmd_buf->Begin();
    bool result=CommitTexture2D(tex,buf,&buffer_image_copy,1,destinationStage);
    texture_cmd_buf->End();
    SubmitTexture(*texture_cmd_buf);
    return result;
}

bool GPUDevice::ChangeTexture2D(Texture2D *tex,void *data,uint32_t left,uint32_t top,uint32_t width,uint32_t height,uint32_t size,VkPipelineStageFlags destinationStage)
{
    if(!tex||!data
     ||left<0||left+width>tex->GetWidth()
     ||top<0||top+height>tex->GetHeight()
     ||width<=0||height<=0
     ||size<=0)
        return(false);

    GPUBuffer *buf=CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,size,data);
    
    bool result=ChangeTexture2D(tex,buf,left,top,width,height,destinationStage);    

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
VK_NAMESPACE_END
