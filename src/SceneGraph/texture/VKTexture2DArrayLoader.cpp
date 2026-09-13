#include"VKTextureLoader.h"
#include<hgl/io/FileInputStream.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKFormat.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/log/Log.h>

namespace hgl::graph{
/**
 * 把一张 2D 纹理文件装载进 Texture2DArray 的指定层。
 *
 * 拷贝的内容 = 该文件自带的**完整 mip 链**（.Tex2D 头里的 mipmaps 即级数），
 * 因此调用前数组的 mip 级数必须已按资产真源建好（CreateTexture2DArray 的 mip_levels）。
 *
 * 级数策略：
 *   * 数组只要 0 级（mip_levels=1）        → 只拷 0 级（源链被截断，行为与旧版一致）
 *   * 源链 >= 数组级数                     → 整链拷入（块压缩格式的唯一路径）
 *   * 源链 = 1 且数组多级 且 非块压缩       → 单级拷入 + 逐级 blit 生成
 *   * 源链 = 1 且数组多级 且 块压缩         → 显式失败（不支持自动生成，须由资产提供）
 *   * 1 < 源链 < 数组级数                  → 显式失败（不混合"拷贝 + 生成"两种来源）
 *
 * 尺寸/格式必须与数组完全一致：拷贝按「数组 extent + 源 buffer 紧密排布」进行，
 * 不一致不是画面对不对的问题，而是静默错位——fail-fast。
 */
bool LoadTexture2DLayerFromFile(TextureManager *tm,Texture2DArray *ta,const uint32_t layer,const OSString &filename)
{
    if(!tm||!ta||filename.IsEmpty())
        return(false);

    //注：依然是Texture2D，则非Texture2DArray。因为这里LOAD的是2D纹理，并不是2DArray纹理
    VkTextureLoader<Texture2D,Texture2DLoader> loader(tm,false);

    if(!loader.Load(filename))
        return(false);

    DeviceBuffer *buf=loader.GetBuffer();

    if(!buf)
        return(false);

    const TextureFileHeader &file_header=loader.GetFileHeader();
    const VkFormat file_format=loader.GetTextureFormat();

    const char *file_format_name=GetVulkanFormatName(file_format)?GetVulkanFormatName(file_format):"unknown";
    const U8String filename_u8=to_u8(filename.c_str(),filename.Length());

    if(file_header.width!=ta->GetWidth()
     ||file_header.height!=ta->GetHeight()
     ||file_format!=ta->GetFormat())
    {
        GLogError("[Texture2DArray] layer %u source mismatched: file=%ux%u %s vs array=%ux%u %s (%s)",
                  layer,
                  file_header.width,file_header.height,file_format_name,
                  ta->GetWidth(),ta->GetHeight(),
                  GetVulkanFormatName(ta->GetFormat())?GetVulkanFormatName(ta->GetFormat()):"unknown",
                  filename_u8.c_str());
        return(false);
    }

    const uint32_t array_mips=ta->GetMipLevel();
    const uint32_t file_mips =file_header.mipmaps;

    RectScope2ui scope;
    scope.Width =ta->GetWidth();
    scope.Height=ta->GetHeight();

    if(array_mips<=1)                           //数组只取 0 级
        return tm->ChangeTexture2DArray(ta,buf,scope,layer,1);

    if(file_mips>=array_mips)                   //源链足够：整链拷入
        return tm->ChangeTexture2DArrayMipmaps(ta,buf,*ta->GetExtent(),loader.GetZeroMipmapBytes(),layer,1);

    if(file_mips>1)                             //源链级数不足：不混合两种来源
    {
        GLogError("[Texture2DArray] layer %u source mip chain insufficient: file=%u < array=%u, %s (%s)",
                  layer,file_mips,array_mips,file_format_name,filename_u8.c_str());
        return(false);
    }

    if(IsBlockCompressedFormat(file_format))    //源无链 + 块压缩 = 不支持自动生成
    {
        GLogError("[Texture2DArray] layer %u block-compressed %s has no mip chain in source; "
                  "auto-generation is not supported (provide an asset with a mip chain): %s",
                  layer,file_format_name,filename_u8.c_str());
        return(false);
    }

    if(!tm->ChangeTexture2DArray(ta,buf,scope,layer,1,VK_PIPELINE_STAGE_TRANSFER_BIT))
        return(false);

    return tm->GenerateTexture2DArrayMipmaps(ta,layer);
}
}//namespace hgl::graph
