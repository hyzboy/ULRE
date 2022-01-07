#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKImageCreateInfo.h>
#include<hgl/graph/VKCommandBuffer.h>
#include"BufferImageCopy2D.h"
VK_NAMESPACE_BEGIN
void GenerateMipmapsCube(TextureCmdBuffer *texture_cmd_buf,VkImage image,VkImageAspectFlags aspect_mask,VkExtent3D extent,const uint32_t mipLevels);

TextureCube *GPUDevice::CreateTextureCube(TextureData *tex_data)
{
    if(!tex_data)
        return(nullptr);

    return(new TextureCube(attr->device,tex_data));
}

TextureCube *GPUDevice::CreateTextureCube(TextureCreateInfo *tci)
{
    if(!tci)return(nullptr);

    if(tci->extent.width*tci->extent.height<=0)return(nullptr);

    if(tci->target_mipmaps==0)
        tci->target_mipmaps=(tci->origin_mipmaps>1?tci->origin_mipmaps:1);

    if(!tci->image)
    {
        ImageCubeCreateInfo ici(tci->usage,tci->tiling,tci->format,tci->extent,tci->target_mipmaps);
        tci->image=CreateImage(&ici);

        if(!tci->image)
        {
            Clear(tci);
            return(nullptr);
        }

        tci->memory=CreateMemory(tci->image);
    }

    if(!tci->image_view)
        tci->image_view=CreateImageViewCube(attr->device,tci->format,tci->extent,tci->target_mipmaps,tci->aspect,tci->image);

    TextureData *tex_data=new TextureData(tci);

    TextureCube *tex=CreateTextureCube(tex_data);

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
                CommitTextureCube(tex,tci->buffer,tci->mipmap_zero_total_bytes,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }
            else //本身有mipmaps数据
            {
                CommitTextureCubeMipmaps(tex,tci->buffer,tci->extent,tci->mipmap_zero_total_bytes);
            }
        }
        else
            if(tci->origin_mipmaps<=1)          //本身不含mipmaps数据,又想要mipmaps
            {
                CommitTextureCube(tex,tci->buffer,tci->mipmap_zero_total_bytes,VK_PIPELINE_STAGE_TRANSFER_BIT);
                GenerateMipmapsCube(texture_cmd_buf,tex->GetImage(),tex->GetAspect(),tci->extent,tex_data->miplevel);
            }
        texture_cmd_buf->End();

        SubmitTexture(*texture_cmd_buf);

        delete tci->buffer;
    }

    delete tci;     //"delete tci" is correct,please don't use "Clear(tci)"
    return tex;
}

bool GPUDevice::CommitTextureCube(TextureCube *tex,GPUBuffer *buf,const VkBufferImageCopy *buffer_image_copy,const int count,VkPipelineStageFlags destinationStage)
{
    if(!tex||!buf)
        return(false);

    ImageSubresourceRange subresourceRange(tex->GetAspect(),tex->GetMipLevel(),6);

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

bool GPUDevice::CommitTextureCube(TextureCube *tex,GPUBuffer *buf,const uint32_t mipmaps_zero_bytes,VkPipelineStageFlags destinationStage)
{
    if(!tex||!buf||!mipmaps_zero_bytes)return(false);

    AutoDeleteArray<VkBufferImageCopy> bic_list(6);

    uint32_t face=0;
    uint32_t offset=0;

    for(VkBufferImageCopy &bic:bic_list)
    {
        bic.bufferOffset      = offset;
        bic.bufferRowLength   = 0;
        bic.bufferImageHeight = 0;
        bic.imageSubresource.aspectMask       = tex->GetAspect();
        bic.imageSubresource.mipLevel         = 0;
        bic.imageSubresource.baseArrayLayer   = face;
        bic.imageSubresource.layerCount       = 1;
        bic.imageOffset.x     = 0;
        bic.imageOffset.y     = 0;
        bic.imageOffset.z     = 0;
        bic.imageExtent.width = tex->GetWidth();
        bic.imageExtent.height= tex->GetHeight();
        bic.imageExtent.depth = 1;

        ++face;
        offset+=mipmaps_zero_bytes;
    }

    return CommitTextureCube(tex,buf,bic_list,6,destinationStage);
}

bool GPUDevice::CommitTextureCubeMipmaps(TextureCube *tex,GPUBuffer *buf,const VkExtent3D &extent,uint32_t total_bytes)
{
    if(!tex||!buf
      ||extent.width*extent.height<=0)
        return(false);

    const uint32_t miplevel=tex->GetMipLevel();

    AutoDeleteArray<VkBufferImageCopy> buffer_image_copy(miplevel*6);

    VkDeviceSize offset=0;
    
    uint32_t face=0;
    uint32_t level=0;

    uint32_t width=extent.width;
    uint32_t height=extent.height;

    buffer_image_copy.zero();

    for(VkBufferImageCopy &bic:buffer_image_copy)
    {
        bic.bufferOffset      = offset;
        bic.bufferRowLength   = 0;
        bic.bufferImageHeight = 0;
        bic.imageSubresource.aspectMask       = tex->GetAspect();
        bic.imageSubresource.mipLevel         = level;
        bic.imageSubresource.baseArrayLayer   = face;
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

        if(face==5)
        {
            face=0;
            ++level;

            if(width>1){width>>=1;total_bytes>>=1;}
            if(height>1){height>>=1;total_bytes>>=1;}
        }
        else
        {
            ++face;
        }
    }

    return CommitTextureCube(tex,buf,buffer_image_copy,miplevel*6,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

//bool GPUDevice::ChangeTexture2D(Texture2D *tex,GPUBuffer *buf,const List<Image2DRegion> &ir_list,VkPipelineStageFlags destinationStage)
//{
//    if(!tex||!buf||ir_list.GetCount()<=0)
//        return(false);
//
//    const int ir_count=ir_list.GetCount();
//    int count=0;
//
//    AutoDeleteArray<VkBufferImageCopy> buffer_image_copy(ir_count);
//    VkBufferImageCopy *tp=buffer_image_copy;
//
//    VkDeviceSize offset=0;
//
//    for(const Image2DRegion &sp:ir_list)
//    {
//        tp->bufferOffset      = offset;
//        tp->bufferRowLength   = 0;
//        tp->bufferImageHeight = 0;
//        tp->imageSubresource.aspectMask       = tex->GetAspect();
//        tp->imageSubresource.mipLevel         = 0;
//        tp->imageSubresource.baseArrayLayer   = 0;
//        tp->imageSubresource.layerCount       = 1;
//        tp->imageOffset.x     = sp.left;
//        tp->imageOffset.y     = sp.top;
//        tp->imageOffset.z     = 0;
//        tp->imageExtent.width = sp.width;
//        tp->imageExtent.height= sp.height;
//        tp->imageExtent.depth = 1;
//
//        offset+=sp.bytes;
//        ++tp;
//    }
//
//    texture_cmd_buf->Begin();
//    bool result=CommitTexture2D(tex,buf,buffer_image_copy,ir_count,destinationStage);
//    texture_cmd_buf->End();
//    SubmitTexture(*texture_cmd_buf);
//    return result;
//}
//
//bool GPUDevice::ChangeTexture2D(Texture2D *tex,GPUBuffer *buf,uint32_t left,uint32_t top,uint32_t width,uint32_t height,VkPipelineStageFlags destinationStage)
//{
//    if(!tex||!buf
//        ||left<0||left+width>tex->GetWidth()
//        ||top<0||top+height>tex->GetHeight()
//        ||width<=0||height<=0)
//        return(false);
//
//    BufferImageCopy buffer_image_copy(tex);
//
//    texture_cmd_buf->Begin();
//    bool result=CommitTexture2D(tex,buf,&buffer_image_copy,1,destinationStage);
//    texture_cmd_buf->End();
//    SubmitTexture(*texture_cmd_buf);
//    return result;
//}
//
//bool GPUDevice::ChangeTexture2D(Texture2D *tex,void *data,uint32_t left,uint32_t top,uint32_t width,uint32_t height,uint32_t size,VkPipelineStageFlags destinationStage)
//{
//    if(!tex||!data
//        ||left<0||left+width>tex->GetWidth()
//        ||top<0||top+height>tex->GetHeight()
//        ||width<=0||height<=0
//        ||size<=0)
//        return(false);
//
//    GPUBuffer *buf=CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,size,data);
//
//    bool result=ChangeTexture2D(tex,buf,left,top,width,height,destinationStage);    
//
//    delete buf;
//    return(result);
//}
VK_NAMESPACE_END