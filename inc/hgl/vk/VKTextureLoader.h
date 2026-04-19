#ifndef HGL_GRAPH_VULKAN_TEXTURE_LOADER_INCLUDE
#define HGL_GRAPH_VULKAN_TEXTURE_LOADER_INCLUDE

#include<hgl/graph/texture/TextureLoader.h>

namespace hgl::graph{
template<typename T,typename L>
class VkTextureLoader:public L
{
protected:

    VulkanDevice *device;
    GPUBuffer *buf;

    T *tex;

    bool auto_mipmaps;

public:

    VkTextureLoader(VulkanDevice *dev,const bool am)
    {
        device      =dev;
        buf         =nullptr;
        tex         =nullptr;
        auto_mipmaps=am;
    }

    virtual ~VkTextureLoader()
    {
        SAFE_CLEAR(buf);
        SAFE_CLEAR(tex);
    }

    T *GetTexture()
    {
        T *result=tex;
        tex=nullptr;
        return result;
    }

    void *OnBegin(uint32 total_bytes) override
    {
        SAFE_CLEAR(buf);
        SAFE_CLEAR(tex);

        if(!CheckVulkanFormat(format))
            return(nullptr);

        buf=device->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,total_bytes);

        if(!buf)
            return(nullptr);

        auto *gpu = buf->GetGPUBuffer();
        return gpu ? gpu->Map(0, gpu->GetSize()) : nullptr;
    }

    void OnEnd() override
    {
        auto *gpu = buf->GetGPUBuffer();
        if(gpu)
            gpu->Unmap();

        TextureCreateInfo *tci=new TextureCreateInfo(format);

        tci->type=type;

        VkExtent3D extent;

        extent.width    =file_header.width;
        extent.height   =file_header.height;
        extent.depth    =file_header.depth;

        tci->SetData(buf,extent);

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
            tci->origin_mipmaps=
            tci->target_mipmaps=file_header.mipmaps;
        }

        tci->mipmap_zero_total_bytes=mipmap_zero_total_bytes;

        tex=device->CreateTexture2D(tci);

        if(tex)
            buf=nullptr;
    }
};//class VkTextureLoader:public L
}//namespace hgl::graph
#endif//HGL_GRAPH_VULKAN_TEXTURE_LOADER_INCLUDE
