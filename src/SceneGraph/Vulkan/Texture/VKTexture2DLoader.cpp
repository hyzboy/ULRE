#include"VKTextureLoader.h"
#include<hgl/io/FileInputStream.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN
template<> void VkTextureLoader<Texture2D,Texture2DLoader>::OnExtent(VkExtent3D &extent)
{
    extent.width    =file_header.width;
    extent.height   =file_header.height;
    extent.depth    =1;
}

template<> Texture2D *VkTextureLoader<Texture2D,Texture2DLoader>::OnCreateTexture(TextureCreateInfo *tci)
{
    return device->CreateTexture2D(tci);
}

Texture2D *CreateTexture2DFromFile(GPUDevice *device,const OSString &filename,bool auto_mipmaps)
{
    VkTextureLoader<Texture2D,Texture2DLoader> loader(device,auto_mipmaps);

    if(!loader.Load(filename))
        return(nullptr);

    return loader.CreateTexture();
}
VK_NAMESPACE_END
