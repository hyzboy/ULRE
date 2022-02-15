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
    return device->CreateTextureCube(tci);
}

TextureCube *CreateTextureCubeFromFile(GPUDevice *device,const OSString &filename,bool auto_mipmaps)
{
    VkTextureLoader<TextureCube,TextureCubeLoader> loader(device,auto_mipmaps);

    if(!loader.Load(filename))
        return(nullptr);

    return loader.GetTexture();
}
VK_NAMESPACE_END
