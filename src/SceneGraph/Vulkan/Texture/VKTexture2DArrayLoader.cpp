#include"VKTextureLoader.h"
#include<hgl/io/FileInputStream.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN
template<> void VkTextureLoader<Texture2DArray,Texture2DArrayLoader>::OnExtent(VkExtent3D &extent)
{
    extent.width    =file_header.width;
    extent.height   =file_header.height;
    extent.depth    =file_header.layers;
}

template<> Texture2DArray *VkTextureLoader<Texture2DArray,Texture2DArrayLoader>::OnCreateTexture(TextureCreateInfo *tci)
{
    return device->CreateTexture2DArray(tci);
}

Texture2DArray *CreateTexture2DArrayFromFile(GPUDevice *device,const OSString &filename,bool auto_mipmaps)
{
    VkTextureLoader<Texture2DArray,Texture2DArrayLoader> loader(device,auto_mipmaps);

    if(!loader.Load(filename))
        return(nullptr);

    return loader.GetTexture();
}
VK_NAMESPACE_END
