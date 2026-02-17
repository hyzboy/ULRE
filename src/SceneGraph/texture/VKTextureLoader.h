#pragma once
#include<hgl/vk/VK.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/graph/texture/TextureLoader.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/vk/VKTextureCreateInfo.h>

namespace hgl::graph{
template<typename T,typename TL> class VkTextureLoader:public TL
{
protected:

    TextureManager *tex_manager;
    DeviceBuffer *buf;
    T *tex;

    bool auto_mipmaps;
    OSString filename;

public:

    VkTextureLoader(TextureManager *tm,const bool am,const OSString &fn=OSString())
    {
        tex_manager=tm;
        buf=nullptr;
        tex=nullptr;
        auto_mipmaps=am;
        filename=fn;
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

        buf=tex_manager->CreateTransferSourceBuffer(total_bytes);

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
        U8String texture_name = filename.IsEmpty() ? U8String((const u8char*)u8"Texture") : to_u8(filename.c_str(), filename.Length());
        TextureCreateInfo *tci=new TextureCreateInfo(tex_format, texture_name);

        VkExtent3D extent;

        OnExtent(extent);

        tci->SetData(buf,extent);

        tci->origin_mipmaps=tex_file_header.mipmaps;

        if(auto_mipmaps&&tex_file_header.mipmaps<=1)
        {
            if(tex_manager->CheckFormatSupport(tex_format,VK_FORMAT_FEATURE_BLIT_DST_BIT))
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
}//namespace hgl::graph
