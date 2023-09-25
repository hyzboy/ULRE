#include"VKTextureLoader.h"
#include<hgl/io/FileInputStream.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN
//template<> void VkTextureLoader<Texture2DArray,Texture2DArrayLoader>::OnExtent(VkExtent3D &extent)
//{
//    extent.width    =file_header.width;
//    extent.height   =file_header.height;
//    extent.depth    =file_header.layers;
//}
//
//template<> Texture2DArray *VkTextureLoader<Texture2DArray,Texture2DArrayLoader>::OnCreateTexture(TextureCreateInfo *tci)
//{
//    return device->CreateTexture2DArray(tci);
//}

bool LoadTexture2DLayerFromFile(GPUDevice *device,Texture2DArray *ta,const uint32_t layer,const OSString &filename,bool auto_mipmaps)
{
    //注：依然是Texture2D，则非Texture2DArray。因为这里LOAD的是2D纹理，并不是2DArray纹理
    VkTextureLoader<Texture2D,Texture2DLoader> loader(device,auto_mipmaps);

    if(!loader.Load(filename))
        return(nullptr);

    DeviceBuffer *buf=loader.GetBuffer();

    if(!buf)
        return(false);

    RectScope2ui scope;

    scope.Width=ta->GetWidth();
    scope.Height=ta->GetHeight();

    return device->ChangeTexture2DArray(ta,buf,scope,layer,1);
}
VK_NAMESPACE_END
