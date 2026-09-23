#include<hgl/graph/module/TextureManager.h>
#include<hgl/vk/VKImageCreateInfo.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKFormat.h>
#include<hgl/log/Log.h>
#include"CopyBufferToImage.h"
namespace hgl::graph{
void GenerateMipmaps(TextureCmdBuffer *texture_cmd_buf,
                     VkImage image,
                     VkImageAspectFlags aspect_mask,
                     VkExtent3D extent,
                     const uint32_t mipLevels,
                     const uint32_t base_array_layer,
                     const uint32_t layer_count);

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

    if(tci->target_mipmaps>1)           //多级数组：非压缩格式的逐级 blit 生成需要 TRANSFER_SRC
        tci->usage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    if(!tci->image)
    {
        Image2DArrayCreateInfo ici(tci->usage,tci->tiling,tci->format,tci->extent,tci->target_mipmaps);
        tci->image=CreateImage(&ici);

        if(!tci->image)
        {
            Clear(tci);
            return(nullptr);
        }

        tci->memory=GetDevice()->CreateMemory(tci->image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ObjectNameBuilder(tci->name.IsEmpty() ? "Texture2DArrayMemory" : (const char*)tci->name.c_str()));
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

    //不支持从文件加载整个 texture 2d array：只支持创建空数组后，逐层从单张 2D 文件提交。
    //每一层拷入的是该文件自带的**完整 mip 链**（见 ChangeTexture2DArrayMipmaps）。
    delete tci;     //"delete tci" is correct,please don't use "Clear(tci)"
    return tex;
}

Texture2DArray *TextureManager::CreateTexture2DArray(const uint32_t w,const uint32_t h,const uint32 l,const VkFormat fmt,const uint32_t mip_levels)
{
    if(w*h*l<=0)
        return(nullptr);

    if(!CheckVulkanFormat(fmt))
        return(nullptr);

    TextureCreateInfo *tci=new TextureCreateInfo(fmt);

    tci->extent.width   =w;
    tci->extent.height  =h;
    tci->extent.depth   =l;

    //mip 级数真源 = 源资产（.Tex2D 文件头里的列数）。块压缩格式禁止自动生成，
    //因此这里给出多少级就必须有多少级，级数不符会在逐层装载时被显式拒绝。
    const uint32_t mips=(mip_levels>0?mip_levels:1);

    tci->origin_mipmaps=mips;
    tci->target_mipmaps=mips;

    return CreateTexture2DArray(tci);
}

bool TextureManager::ChangeTexture2DArray(Texture2DArray *tex,DeviceBuffer *buf_dev,const RectScope2ui &scope,const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags2 destinationStage)
{
    if(!tex||!buf_dev
        ||layer_count<=0
        ||scope.GetWidth()<=0
        ||scope.GetHeight()<=0
        ||scope.GetRight()>tex->GetWidth()
        ||scope.GetBottom()>tex->GetHeight())
        return(false);

    const VkBuffer buf=buf_dev->GetBuffer();

    BufferImageCopy buffer_image_copy(tex,scope,base_layer,layer_count);

    texture_cmd_buf->Begin();
    bool result=CopyBufferToImage(tex,buf,&buffer_image_copy,1,base_layer,layer_count,destinationStage);
    texture_cmd_buf->End();
    SubmitTexture(*texture_cmd_buf);
    return result;
}

bool TextureManager::ChangeTexture2DArrayMipmaps(Texture2DArray *tex,DeviceBuffer *buf_dev,const VkExtent3D &extent,const uint32_t top_mipmap_bytes,const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags2 destinationStage)
{
    if(!tex||!buf_dev
        ||layer_count<=0
        ||extent.width*extent.height<=0)
        return(false);

    const uint32_t mip_levels=tex->GetMipLevel();

    if(mip_levels<=1)                   //单级数组走 ChangeTexture2DArray
        return(false);

    //源 buffer 布局 = .Tex2D 文件内的 level0..level(n-1) 连续排列（与加载侧
    //ComputeTexture2DMipmapChainBytes 的算法一致），因此逐级推进偏移即可整链拷入。
    AutoDeleteArray<VkBufferImageCopy> bic_list(mip_levels);

    VkDeviceSize offset=0;
    uint32_t level=0;
    uint32_t width=extent.width;
    uint32_t height=extent.height;
    uint32_t rolling_level_bytes=top_mipmap_bytes;

    bic_list.zero();

    for(VkBufferImageCopy &bic:bic_list)
    {
        bic.bufferOffset      =offset;
        bic.bufferRowLength   =0;
        bic.bufferImageHeight =0;
        bic.imageSubresource.aspectMask       =tex->GetAspect();
        bic.imageSubresource.mipLevel         =level++;
        bic.imageSubresource.baseArrayLayer   =base_layer;
        bic.imageSubresource.layerCount       =layer_count;
        bic.imageOffset.x     =0;
        bic.imageOffset.y     =0;
        bic.imageOffset.z     =0;
        bic.imageExtent.width =width;
        bic.imageExtent.height=height;
        bic.imageExtent.depth =1;

        const bool can_half_width  =(width >1);
        const bool can_half_height =(height>1);

        uint32_t level_bytes=0;
        if(IsBlockCompressedFormat(tex->GetFormat()))
        {
            if(!GetBlockCompressedLevelBytes(tex->GetFormat(),width,height,level_bytes))
                return(false);      //块压缩但字节布局未支持：明确失败，勿按非压缩规则静默推进

            offset+=level_bytes;
        }
        else
        {
            if(rolling_level_bytes<8)
                offset+=8;
            else
                offset+=rolling_level_bytes;

            if(can_half_width)  rolling_level_bytes>>=1;
            if(can_half_height) rolling_level_bytes>>=1;
        }

        if(can_half_width)  width >>=1;
        if(can_half_height) height>>=1;
    }

    const VkBuffer buf=buf_dev->GetBuffer();

    texture_cmd_buf->Begin();
    bool result=CopyBufferToImage(tex,buf,bic_list,mip_levels,base_layer,layer_count,destinationStage);
    texture_cmd_buf->End();
    SubmitTexture(*texture_cmd_buf);
    return result;
}

bool TextureManager::GenerateTexture2DArrayMipmaps(Texture2DArray *tex, const uint32_t layer)
{
    if(!tex||!texture_cmd_buf||!texture_queue)
        return(false);

    if(layer>=tex->GetLayer())
        return(false);

    if(IsBlockCompressedFormat(tex->GetFormat()))       //块压缩格式不允许自动生成 mipmap
    {
        GLogError("[Texture2DArray] compressed format %s does not support mipmap auto-generation (layer=%u)",
                  GetVulkanFormatName(tex->GetFormat())?GetVulkanFormatName(tex->GetFormat()):"unknown",
                  layer);
        return(false);
    }

    if(tex->GetMipLevel()<=1)                           //单级数组无需生成
        return(true);

    ImageSubresourceRange range(tex->GetAspect(), tex->GetMipLevel(), 1);
    range.baseArrayLayer = layer;

    texture_cmd_buf->Begin();
    // 入口屏障：源 buffer 的写入（拷贝以 ALL_TRANSFER 结束，图像停在 TRANSFER_DST 布局）
    // 必须对 blit 读可见
    texture_cmd_buf->ImageMemoryBarrier2(tex->GetImage(),
                                        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                                        VK_PIPELINE_STAGE_2_BLIT_BIT,
                                        VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                        VK_ACCESS_2_TRANSFER_READ_BIT,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        range);
    GenerateMipmaps(texture_cmd_buf,
                    tex->GetImage(),
                    tex->GetAspect(),
                    *tex->GetExtent(),
                    tex->GetMipLevel(),
                    layer,
                    1);
    texture_cmd_buf->End();
    return SubmitTexture(*texture_cmd_buf);
}
}//namespace hgl::graph
