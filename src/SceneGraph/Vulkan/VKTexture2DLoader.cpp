#include<hgl/graph/VK.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/TextureLoader.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN
namespace
{
    class VkTextureLoader
    {
    };

    class VkTexture2DLoader:public Texture2DLoader
    {
    protected:

        GPUDevice *device;
        GPUBuffer *buf;

        bool auto_mipmaps;

        Texture2D *tex;

    public:

        VkTexture2DLoader(GPUDevice *dev,const bool am):device(dev)
        {
            buf=nullptr;
            tex=nullptr;
            auto_mipmaps=am;
        }

        virtual ~VkTexture2DLoader()
        {
            SAFE_CLEAR(buf);
            SAFE_CLEAR(tex);
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

            return buf->Map();
        }

        void OnEnd() override
        {
            buf->Unmap();

            TextureCreateInfo *tci=new TextureCreateInfo(format);

            VkExtent3D extent;

            extent.width    =file_header.width;
            extent.height   =file_header.height;
            extent.depth    =1;

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

        Texture2D *GetTexture()
        {
            Texture2D *result=tex;
            tex=nullptr;
            return result;
        }
    };//class VkTexture2DLoader
}//namespace

Texture2D *CreateTexture2DFromFile(GPUDevice *device,const OSString &filename,bool auto_mipmaps)
{
    VkTexture2DLoader loader(device,auto_mipmaps);

    if(!loader.Load(filename))
        return(nullptr);

    return loader.GetTexture();
}
VK_NAMESPACE_END
