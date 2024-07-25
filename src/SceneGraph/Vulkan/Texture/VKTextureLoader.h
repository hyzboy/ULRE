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

    void *OnBegin(uint32 total_bytes,const VkFormat &tex_format) override
    {
        SAFE_CLEAR(buf);

        if(!CheckVulkanFormat(tex_format))
            return(nullptr);

        buf=device->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,total_bytes);

        if(!buf)
            return(nullptr);

        return buf->Map();
    }

    void OnExtent(VkExtent3D &extent);

    T *OnCreateTexture(TextureCreateInfo *);

    bool OnEnd() override
    {
        if(!buf)return(false);
        buf->Unmap();

        return(true);
    }

    DeviceBuffer *GetBuffer(){return buf;}

    T *CreateTexture(const TextureFileHeader &tex_file_header,const VkFormat &tex_format,const uint32 top_mipmap_bytes)
    {
        TextureCreateInfo *tci=new TextureCreateInfo(tex_format);

        VkExtent3D extent;

        OnExtent(extent);

        tci->SetData(buf,extent);

        tci->origin_mipmaps=tex_file_header.mipmaps;

        if(auto_mipmaps&&tex_file_header.mipmaps<=1)
        {
            if(device->CheckFormatSupport(tex_format,VK_FORMAT_FEATURE_BLIT_DST_BIT))
            {
                tci->usage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
                tci->SetAutoMipmaps();
            }
        }
        else
        {
            tci->target_mipmaps=tex_file_header.mipmaps;
        }

        tci->mipmap_zero_total_bytes=top_mipmap_bytes;

        SAFE_CLEAR(tex);
        tex=OnCreateTexture(tci);

        if(!tex)
            return nullptr;
        
        buf=nullptr;

        T *result=tex;
        tex=nullptr;
        return result;
    }
};//class VkTextureLoader
VK_NAMESPACE_END