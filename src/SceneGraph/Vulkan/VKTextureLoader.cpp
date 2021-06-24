#include<hgl/graph/VK.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/TextureLoader.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN
namespace
{
    struct PixelFormat
    {
        VkFormat        format;

        uint8           channels;   //颜色通道数
        char            colors[4];
        uint8           bits[4];
        VulkanDataType  type;

    public:

        const uint GetPixelBytes()const{return (bits[0]+bits[1]+bits[2]+bits[3])>>3;}                   ///<获取单个象素所需字节数
    };//

    constexpr PixelFormat pf_list[]=
    {
        {UPF_BGRA4,      4,{'B','G','R','A'},{ 4, 4, 4, 4},VulkanDataType::UNORM},
        {UPF_RGB565,     3,{'R','G','B', 0 },{ 5, 6, 5, 0},VulkanDataType::UNORM},
        {UPF_A1RGB5,     4,{'A','R','G','B'},{ 1, 5, 5, 5},VulkanDataType::UNORM},
        {UPF_R8,         1,{'R', 0 , 0 , 0 },{ 8, 0, 0, 0},VulkanDataType::UNORM},
        {UPF_RG8,        2,{'R','G', 0 , 0 },{ 8, 8, 0, 0},VulkanDataType::UNORM},
        {UPF_RGBA8,      4,{'R','G','B','A'},{ 8, 8, 8, 8},VulkanDataType::UNORM},
        {UPF_RGBA8S,     4,{'R','G','B','A'},{ 8, 8, 8, 8},VulkanDataType::SNORM},
        {UPF_RGBA8U,     4,{'R','G','B','A'},{ 8, 8, 8, 8},VulkanDataType::UINT},
        {UPF_RGBA8I,     4,{'R','G','B','A'},{ 8, 8, 8, 8},VulkanDataType::SINT},
        {UPF_ABGR8,      4,{'A','B','G','R'},{ 8, 8, 8, 8},VulkanDataType::UNORM},
        {UPF_A2BGR10,    4,{'A','B','G','R'},{ 2,10,10,10},VulkanDataType::UNORM},
        {UPF_R16,        1,{'R', 0 , 0 , 0 },{16, 0, 0, 0},VulkanDataType::UNORM},
        {UPF_R16F,       1,{'R', 0 , 0 , 0 },{16, 0, 0, 0},VulkanDataType::SFLOAT},
        {UPF_RG16,       2,{'R','G', 0 , 0 },{16,16, 0, 0},VulkanDataType::UNORM},
        {UPF_RG16F,      2,{'R','G', 0 , 0 },{16,16, 0, 0},VulkanDataType::SFLOAT},
        { PF_RGBA16UN,   4,{'R','G','B','A'},{16,16,16,16},VulkanDataType::UNORM},
        { PF_RGBA16SN,   4,{'R','G','B','A'},{16,16,16,16},VulkanDataType::SNORM},
        {UPF_RGBA16U,    4,{'R','G','B','A'},{16,16,16,16},VulkanDataType::UINT},
        {UPF_RGBA16I,    4,{'R','G','B','A'},{16,16,16,16},VulkanDataType::SINT},
        {UPF_RGBA16F,    4,{'R','G','B','A'},{16,16,16,16},VulkanDataType::SFLOAT},
        {UPF_R32U,       1,{'R', 0 , 0 , 0 },{32, 0, 0, 0},VulkanDataType::UINT},
        {UPF_R32I,       1,{'R', 0 , 0 , 0 },{32, 0, 0, 0},VulkanDataType::SINT},
        {UPF_R32F,       1,{'R', 0 , 0 , 0 },{32, 0, 0, 0},VulkanDataType::SFLOAT},
        {UPF_RG32U,      2,{'R','G', 0 , 0 },{32,32, 0, 0},VulkanDataType::UINT},
        {UPF_RG32I,      2,{'R','G', 0 , 0 },{32,32, 0, 0},VulkanDataType::SINT},
        {UPF_RG32F,      2,{'R','G', 0 , 0 },{32,32, 0, 0},VulkanDataType::SFLOAT},
        { PF_RGB32U,     3,{'R','G','B', 0 },{32,32,32, 0},VulkanDataType::UINT},
        { PF_RGB32I,     3,{'R','G','B', 0 },{32,32,32, 0},VulkanDataType::SINT},
        { PF_RGB32F,     3,{'R','G','B', 0 },{32,32,32, 0},VulkanDataType::SFLOAT},
        {UPF_RGBA32U,    4,{'R','G','B','A'},{32,32,32,32},VulkanDataType::UINT},
        {UPF_RGBA32I,    4,{'R','G','B','A'},{32,32,32,32},VulkanDataType::SINT},
        {UPF_RGBA32F,    4,{'R','G','B','A'},{32,32,32,32},VulkanDataType::SFLOAT},
        {UPF_B10GR11UF,  3,{'B','G','R', 0 },{10,11,11, 0},VulkanDataType::UFLOAT}
    };

    constexpr uint PixelFormatCount=sizeof(pf_list)/sizeof(PixelFormat);

    const VkFormat GetVulkanFormat(const int channels,const TexPixelFormat &tpf)
    {
        const PixelFormat *pf=pf_list;

        for(uint i=0;i<PixelFormatCount;i++,++pf)
        {
            if(channels!=pf->channels)continue;
            if(tpf.datatype!=(uint8)pf->type)continue;

            if(tpf.colors[0]!=pf->colors[0])continue;
            if(tpf.colors[1]!=pf->colors[1])continue;
            if(tpf.colors[2]!=pf->colors[2])continue;
            if(tpf.colors[3]!=pf->colors[3])continue;

            if(tpf.bits[0]!=pf->bits[0])continue;
            if(tpf.bits[1]!=pf->bits[1])continue;
            if(tpf.bits[2]!=pf->bits[2])continue;
            if(tpf.bits[3]!=pf->bits[3])continue;

            return pf->format;
        }

        return VK_FORMAT_UNDEFINED;
    }

    class VkTexture2DLoader:public Texture2DLoader
    {
    protected:

        GPUDevice *device;

        VkFormat format;
        GPUBuffer *buf;

        bool auto_mipmaps;

        Texture2D *tex;

    public:

        VkTexture2DLoader(GPUDevice *dev,const bool am):device(dev)
        {
            buf=nullptr;
            format=VK_FORMAT_UNDEFINED;
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

            if(file_header.channels==0)
            {
                if(compress_format<0||compress_format>=CompressFormatCount)
                    return(nullptr);

                format=CompressFormatList[compress_format];
            }
            else
                format=GetVulkanFormat(file_header.channels,pixel_format);

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
            
            tci->SetData(buf,file_header.width,file_header.height);

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

Texture2D *CreateTextureFromFile(GPUDevice *device,const OSString &filename,bool auto_mipmaps)
{
    VkTexture2DLoader loader(device,auto_mipmaps);

    if(!loader.Load(filename))
        return(nullptr);

    return loader.GetTexture();
}
VK_NAMESPACE_END
