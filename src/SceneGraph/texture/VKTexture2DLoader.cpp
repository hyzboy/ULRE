#include"VKTextureLoader.h"
#include<hgl/io/FileInputStream.h>

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
    if(!tm||filename.IsEmpty())
        return(nullptr);

    VkTextureLoader<Texture2D,Texture2DLoader> loader(tm,auto_mipmaps,filename);

    if(!loader.Load(filename))
        return(nullptr);

    return loader.CreateTexture(loader.GetFileHeader(),loader.GetTextureFormat(),loader.GetZeroMipmapBytes());
}

uint64_t CreateTexture2DFromFileAsync(TextureManager *tm,
                                     const OSString &filename,
                                     bool auto_mipmaps,
                                     UploadPriority priority,
                                     uint32_t bindless_handle,
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
}//namespace hgl::graph
