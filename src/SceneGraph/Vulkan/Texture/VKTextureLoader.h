#pragma once
#include<hgl/graph/VK.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/TextureLoader.h>
#include<hgl/graph/VKTextureCreateInfo.h>

VK_NAMESPACE_BEGIN
template<typename T,typename TL> class VkTextureLoader:public TL
{
protected:

    GPUDevice *device;
    DeviceBuffer *buf;
    T *tex;

    bool auto_mipmaps;

public:

    VkTextureLoader(GPUDevice *dev,const bool am)
    {
        device=dev;
        buf=nullptr;
        tex=nullptr;
        auto_mipmaps=am;
    }

    virtual ~VkTextureLoader()
    {
        SAFE_CLEAR(tex);
        SAFE_CLEAR(buf);
    }

    void *OnBegin(uint32 total_bytes) override
    {
        SAFE_CLEAR(buf);

        if(!CheckVulkanFormat(format))
            return(nullptr);

        buf=device->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,total_bytes);

        if(!buf)
            return(nullptr);

        return buf->Map();
    }

    void OnExtent(VkExtent3D &extent);
    T *OnCreateTexture(TextureCreateInfo *);

    void OnEnd() override
    {
        buf->Unmap();

        TextureCreateInfo *tci=new TextureCreateInfo(format);

        VkExtent3D extent;

        OnExtent(extent);

        tci->SetData(buf,extent);

        tci->origin_mipmaps=file_header.mipmaps;

        if(auto_mipmaps&&file_header.mipmaps<=1)
        {
            if(device->CheckFormatSupport(format,VK_FORMAT_FEATURE_BLIT_DST_BIT))
            {
                tci->usage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
                tci->SetAutoMipmaps();
            }
        }
        else
        {
            tci->target_mipmaps=file_header.mipmaps;
        }

        tci->mipmap_zero_total_bytes=mipmap_zero_total_bytes;

        SAFE_CLEAR(tex);
        tex=OnCreateTexture(tci);

        if(tex)
            buf=nullptr;
    }

    T *GetTexture()
    {
        T *result=tex;
        tex=nullptr;
        return result;
    }
};//class VkTextureLoader
VK_NAMESPACE_END