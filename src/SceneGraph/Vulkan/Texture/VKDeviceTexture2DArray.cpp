#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/VKImageCreateInfo.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKDevice.h>
#include"CopyBufferToImage.h"
VK_NAMESPACE_BEGIN
void GenerateMipmaps(TextureCmdBuffer *texture_cmd_buf,VkImage image,VkImageAspectFlags aspect_mask,VkExtent3D extent,const uint32_t mipLevels,const uint32_t layer_count);

Texture2DArray *TextureManager::CreateTexture2DArray(TextureData *tex_data)
{
    if(!tex_data)
        return(nullptr);

    Texture2DArray *tex=new Texture2DArray(this,AcquireID(),tex_data);

    Add(tex);

    return tex;
}

Texture2DArray *TextureManager::CreateTexture2DArray(TextureCreateInfo *tci)
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
        Image2DArrayCreateInfo ici(tci->usage,tci->tiling,tci->format,tci->extent,tci->target_mipmaps);
        tci->image=CreateImage(&ici);

        if(!tci->image)
        {
            Clear(tci);
            return(nullptr);
        }
        
        tci->memory=GetDevice()->CreateMemory(tci->image);
    }

    if(!tci->image_view)
        tci->image_view=CreateImageView2DArray(GetVkDevice(),tci->format,tci->extent,tci->target_mipmaps,tci->aspect,tci->image);

    TextureData *tex_data=new TextureData(tci);

    Texture2DArray *tex=CreateTexture2DArray(tex_data);

    if(!tex)
    {
        Clear(tci);
        return(nullptr);
    }

    //我们暂不，也不准备支持从文件加载整个texture 2d array，所以这里的代码暂时注释掉。仅支持创建空的Texture2d array后，一张张2D纹理单独提交
// 
    //if((!tci->buffer)&&tci->pixels&&tci->total_bytes>0)
    //    tci->buffer=CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,tci->total_bytes,tci->pixels);

    //if(tci->buffer)
    //{
    //    texture_cmd_buf->Begin();
    //    if(tci->target_mipmaps==tci->origin_mipmaps)
    //    {
    //        if(tci->target_mipmaps<=1)      //本身不含mipmaps，但也不要mipmaps
    //        {
    //            CommitTexture2DArray(tex,tci->buffer,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    //        }
    //        else //本身有mipmaps数据
    //        {
    //            CommitTexture2DMipmapsArray(tex,tci->buffer,tci->extent,tci->mipmap_zero_total_bytes);
    //        }
    //    }
    //    else
    //        if(tci->origin_mipmaps<=1)          //本身不含mipmaps数据,又想要mipmaps
    //        {
    //            CommitTexture2DArray(tex,tci->buffer,VK_PIPELINE_STAGE_TRANSFER_BIT);
    //            GenerateMipmaps(texture_cmd_buf,tex->GetImage(),tex->GetAspect(),tci->extent,tex_data->miplevel,1);
    //        }
    //    texture_cmd_buf->End();

    //    SubmitTexture(*texture_cmd_buf);

    //    delete tci->buffer;
    //}

    delete tci;     //"delete tci" is correct,please don't use "Clear(tci)"
    return tex;
}

Texture2DArray *TextureManager::CreateTexture2DArray(const uint32_t w,const uint32_t h,const uint32 l,const VkFormat fmt,const bool mipmaps)
{
    if(w*h*l<=0)
        return(nullptr);

    if(!CheckVulkanFormat(fmt))
        return(nullptr);

    TextureCreateInfo *tci=new TextureCreateInfo(fmt);

    tci->extent.width   =w;
    tci->extent.height  =h;
    tci->extent.depth   =l;

    return CreateTexture2DArray(tci);
}

//
//bool GPUDevice::CommitTexture2DArray(Texture2DArray *tex,DeviceBuffer *buf,VkPipelineStageFlags destinationStage)
//{
//    if(!tex||!buf)return(false);
//
//    BufferImageCopy buffer_image_copy(tex);
//
//    return CopyBufferToImageArray(tex,buf,&buffer_image_copy,destinationStage);
//}
//
//bool GPUDevice::CommitTexture2DArrayMipmaps(Texture2DArray *tex,DeviceBuffer *buf,const VkExtent3D &extent,uint32_t total_bytes)
//{
//    if(!tex||!buf
//      ||extent.width*extent.height<=0)
//        return(false);
//
//    const uint32_t miplevel=tex->GetMipLevel();
//
//    AutoDeleteArray<VkBufferImageCopy> buffer_image_copy(miplevel);
//
//    VkDeviceSize offset=0;
//    uint32_t level=0;
//
//    uint32_t width=extent.width;
//    uint32_t height=extent.height;
//
//    buffer_image_copy.zero();
//
//    for(VkBufferImageCopy &bic:buffer_image_copy)
//    {
//        bic.bufferOffset      = offset;
//        bic.bufferRowLength   = 0;
//        bic.bufferImageHeight = 0;
//        bic.imageSubresource.aspectMask       = tex->GetAspect();
//        bic.imageSubresource.mipLevel         = level++;
//        bic.imageSubresource.baseArrayLayer   = 0;
//        bic.imageSubresource.layerCount       = 1;
//        bic.imageOffset.x     = 0;
//        bic.imageOffset.y     = 0;
//        bic.imageOffset.z     = 0;
//        bic.imageExtent.width = width;
//        bic.imageExtent.height= height;
//        bic.imageExtent.depth = 1;
//
//        if(total_bytes<8)
//            offset+=8;
//        else
//            offset+=total_bytes;
//
//        if(width>1){width>>=1;total_bytes>>=1;}
//        if(height>1){height>>=1;total_bytes>>=1;}
//    }
//
//    return CopyBufferToImageArray(tex,buf,buffer_image_copy,miplevel,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
//}
//
//bool GPUDevice::ChangeTexture2DArray(Texture2DArray *tex,DeviceBuffer *buf,const ArrayList<Image2DRegion> &ir_list,const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags destinationStage)
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
//        tp->imageSubresource.baseArrayLayer   = base_layer;
//        tp->imageSubresource.layerCount       = layer_count;
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
//    bool result=CopyBufferToImageArray(tex,buf,buffer_image_copy,ir_count,1,destinationStage);
//    texture_cmd_buf->End();
//    SubmitTexture(*texture_cmd_buf);
//    return result;
//}

bool TextureManager::ChangeTexture2DArray(Texture2DArray *tex,DeviceBuffer *buf,const RectScope2ui &scope,const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags destinationStage)
{
    if(!tex||!buf
        ||base_layer<0
        ||layer_count<=0
        ||scope.GetWidth()<=0
        ||scope.GetHeight()<=0
        ||scope.GetRight()>tex->GetWidth()
        ||scope.GetBottom()>tex->GetHeight())
        return(false);

    BufferImageCopy buffer_image_copy(tex,scope,base_layer,layer_count);

    texture_cmd_buf->Begin();
    bool result=CopyBufferToImage(tex,buf,&buffer_image_copy,1,base_layer,layer_count,destinationStage);
    texture_cmd_buf->End();
    SubmitTexture(*texture_cmd_buf);
    return result;
}

bool TextureManager::ChangeTexture2DArray(Texture2DArray *tex,const void *data,const VkDeviceSize size,const RectScope2ui &scope,const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags destinationStage)
{
    if(!tex||!data
        ||size<=0
        ||base_layer<0
        ||layer_count<=0
        ||scope.GetWidth()<=0
        ||scope.GetHeight()<=0
        ||scope.GetRight()>tex->GetWidth()
        ||scope.GetBottom()>tex->GetHeight())
        return(false);
    
    DeviceBuffer *buf=CreateTransferSourceBuffer(size,data);

    bool result=ChangeTexture2DArray(tex,buf,scope,base_layer,layer_count,destinationStage);

    delete buf;
    return(result);
}
VK_NAMESPACE_END