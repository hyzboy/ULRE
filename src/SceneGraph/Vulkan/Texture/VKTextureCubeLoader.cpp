#include"VKTextureLoader.h"
#include<hgl/io/FileInputStream.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN
template<> void VkTextureLoader<TextureCube,TextureCubeLoader>::OnExtent(VkExtent3D &extent)
{
    extent.width    =file_header.width;
    extent.height   =file_header.height;
    extent.depth    =1;
}

template<> TextureCube *VkTextureLoader<TextureCube,TextureCubeLoader>::OnCreateTexture(TextureCreateInfo *tci)
{
    return tex_manager->CreateTextureCube(tci);
}

TextureCube *CreateTextureCubeFromFile(TextureManager *tm,const OSString &filename,bool auto_mipmaps)
{
    if(!tm||filename.IsEmpty())
        return(nullptr);

    VkTextureLoader<TextureCube,TextureCubeLoader> loader(tm,auto_mipmaps);

    if(!loader.Load(filename))
        return(nullptr);

    return loader.CreateTexture(loader.GetFileHeader(),loader.GetTextureFormat(),loader.GetZeroMipmapBytes());
}
VK_NAMESPACE_END
