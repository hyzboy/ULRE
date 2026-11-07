#include"VKTextureLoader.h"
#include<hgl/io/FileInputStream.h>
#include<hgl/io/MemoryInputStream.h>
#include<hgl/graph/texture/TextureMemoryLoader.h>

namespace hgl::graph{
template<> void VkTextureLoader<Texture2D,Texture2DLoader>::OnExtent(VkExtent3D &extent)
{
    extent.width    =file_header.width;
    extent.height   =file_header.height;
    extent.depth    =1;
}

template<> Texture2D *VkTextureLoader<Texture2D,Texture2DLoader>::OnCreateTexture(TextureCreateInfo *tci)
{
    return tex_manager->CreateTexture2D(tci);
}

Texture2D *CreateTexture2DFromFile(TextureManager *tm,const OSString &filename,bool auto_mipmaps)
{
    if(!tm||!tm->GetDevice()||filename.IsEmpty())
        return(nullptr);

    VkTextureLoader<Texture2D,Texture2DLoader> loader(tm,auto_mipmaps,filename);

    if(!loader.Load(filename))
        return(nullptr);

    return loader.CreateTexture(loader.GetFileHeader(),loader.GetTextureFormat(),loader.GetZeroMipmapBytes());
}

uint64 CreateTexture2DFromFileAsync(TextureManager *tm,
                                     const OSString &filename,
                                     bool auto_mipmaps,
                                     UploadPriority priority,
                                     uint32 bindless_handle,
                                     void (*callback)(TextureUploadTask *, void *),
                                     void *user_data)
{
    if (!tm || filename.IsEmpty())
        return 0;

    VkTextureLoader<Texture2D, Texture2DLoader> loader(tm, auto_mipmaps, filename);

    if (!loader.Load(filename))
        return 0;

    TextureCreateInfo *tci = loader.CreateTextureCreateInfo(loader.GetFileHeader(), loader.GetTextureFormat(), loader.GetZeroMipmapBytes());
    if (!tci)
        return 0;

    return tm->CreateTexture2DAsync(tci, priority, bindless_handle, callback, user_data);
}

Texture2D *CreateTexture2DFromMemory(TextureManager *tm,const void *data,int64 size,bool auto_mipmaps,const OSString &source_name)
{
    if(!tm||!data||size<=0)
        return(nullptr);

    VkTextureLoader<Texture2D,Texture2DLoader> loader(tm,auto_mipmaps,source_name);

    io::MemoryInputStream stream(const_cast<void *>(data),(int64)size);

    if(!loader.Load(&stream))
        return(nullptr);

    return loader.CreateTexture(loader.GetFileHeader(),loader.GetTextureFormat(),loader.GetZeroMipmapBytes());
}

uint64 CreateTexture2DFromMemoryAsync(TextureManager *tm,
                                      const void *data,
                                      int64 size,
                                      bool auto_mipmaps,
                                      UploadPriority priority,
                                      uint32 bindless_handle,
                                      void (*callback)(TextureUploadTask *,void *),
                                      void *user_data,
                                      const OSString &source_name)
{
    if(!tm||!data||size<=0)
        return(0);

    VkTextureLoader<Texture2D,Texture2DLoader> loader(tm,auto_mipmaps,source_name);

    io::MemoryInputStream stream(const_cast<void *>(data),(int64)size);

    if(!loader.Load(&stream))
        return(0);

    TextureCreateInfo *tci=loader.CreateTextureCreateInfo(loader.GetFileHeader(),loader.GetTextureFormat(),loader.GetZeroMipmapBytes());

    if(!tci)
        return(0);

    return tm->CreateTexture2DAsync(tci,priority,bindless_handle,callback,user_data);
}

uint64 CreateTexture2DFromStagingAsync(TextureManager *tm,
                                       const void *bytes,
                                       int64 size,
                                       DeviceBuffer *staging,
                                       bool auto_mipmaps,
                                       UploadPriority priority,
                                       uint32 bindless_handle,
                                       void (*callback)(TextureUploadTask *,void *),
                                       void *user_data,
                                       const OSString &source_name)
{
    if(!tm||!bytes||size<=0)
    {
        if(staging)
            staging->Unmap();   // 口径统一:返回 0 前映射必定已关闭(除非根本没给缓冲)
        return(0);
    }

    if(!staging)
        return(0);

    TextureFileHeader header;
    VkFormat format;
    uint32 mipmap_zero_bytes,total_bytes;

    const bool decoded=DecodeTexture2DFromFileBytes(bytes,size,header,format,mipmap_zero_bytes,total_bytes);

    // 解析(或失败)之后立即结束映射：之后的可见性由 GPU 侧读取，主机指针不再有效。
    staging->Unmap();

    if(!decoded)
        return(0);      // staging 未被接管:调用方 delete 即可

    U8String texture_name = source_name.IsEmpty() ? U8String((const u8char*)u8"Texture")
                                                   : to_u8(source_name.c_str(), source_name.Length());

    TextureCreateInfo *tci=new TextureCreateInfo(format,texture_name);

    VkExtent3D extent;
    extent.width =header.width;
    extent.height=header.height;
    extent.depth =1;

    tci->SetData(staging,extent);
    tci->total_bytes=total_bytes;                 // 仅像素字节：缓冲里的文件头不参与 DMA
    tci->data_offset=TextureFileHeaderLength;     // 像素起点 = 32 字节文件头之后
    tci->origin_mipmaps=header.mipmaps;

    if(auto_mipmaps&&header.mipmaps<=1)
    {
        if(tm->CheckFormatSupport(format,VK_FORMAT_FEATURE_BLIT_DST_BIT))
        {
            tci->usage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            tci->SetAutoMipmaps();
        }
    }
    else
    {
        tci->target_mipmaps=header.mipmaps;
    }

    tci->mipmap_zero_total_bytes=mipmap_zero_bytes;

    // CreateTexture2DAsync 在所有路径都会消费 tci(失败时内部 Clear(tci) 已 delete tci,
    // 但不会 delete tci->buffer)。返回 0 时 staging 未被接管，由调用方 delete。
    return tm->CreateTexture2DAsync(tci,priority,bindless_handle,callback,user_data);
}
}//namespace hgl::graph