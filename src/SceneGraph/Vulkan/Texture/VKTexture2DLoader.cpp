#include"VKTextureLoader.h"
#include<hgl/io/FileInputStream.h>

VK_NAMESPACE_BEGIN
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

    VkTextureLoader<Texture2D,Texture2DLoader> loader(tm,auto_mipmaps);

    if(!loader.Load(filename))
        return(nullptr);

    return loader.CreateTexture(loader.GetFileHeader(),loader.GetTextureFormat(),loader.GetZeroMipmapBytes());
}
VK_NAMESPACE_END
