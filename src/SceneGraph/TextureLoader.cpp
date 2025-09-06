#include<hgl/graph/TextureLoader.h>
#include<hgl/io/FileInputStream.h>

namespace hgl
{
    namespace graph
    {
        namespace
        {
            constexpr VkFormat CompressFormatList[]=
            {
                PF_BC1_RGBUN,
                PF_BC1_RGBAUN,
                PF_BC2UN,
                PF_BC3UN,
                PF_BC4UN,
                PF_BC5UN,
                PF_BC6UF,
                PF_BC6SF,
                PF_BC7UN
            };

            constexpr uint32 CompressFormatBits[]={4,4,8,8,4,8,8,8,8};

            constexpr uint32 CompressFormatCount=sizeof(CompressFormatList)/sizeof(VkFormat);
        }

        const uint TexPixelFormat::pixel_bits()const
        {
            return channels ?bits[0]+bits[1]+bits[2]+bits[3]
                :CompressFormatBits[compress_format];
        }

        const uint32 ComputeMipmapBytes(uint32 length,uint32 bytes)
        {
            uint32 total=0;

            while(length>=1)
            {
                if(bytes<8)
                    total+=8;
                else
                    total+=bytes;

                if(length==1)break;

                if(length>1){length>>=1;bytes>>=1;}
            }

            return total;
        }

        const uint32 ComputeMipmapBytes(uint32 width,uint32 height,uint32 bytes)
        {
            uint32 total=0;

            while(width>=1&&height>=1)
            {
                if(bytes<8)
                    total+=8;
                else
                    total+=bytes;

                if(width==1&&height==1)break;

                if(width >1){width >>=1;bytes>>=1;}
                if(height>1){height>>=1;bytes>>=1;}
            }

            return total;
        }

        const uint32 ComputeMipmapBytes(uint32 width,uint32 height,uint32 depth,uint32 bytes)
        {
            uint32 total=0;

            while(width>=1&&height>=1&&depth>=1)
            {
                if(bytes<8)
                    total+=8;
                else
                    total+=bytes;

                if(width==1&&height==1&&depth==1)break;

                if(depth >1){depth >>=1;bytes>>=1;}
                if(width >1){width >>=1;bytes>>=1;}
                if(height>1){height>>=1;bytes>>=1;}
            }

            return total;
        }

        struct VulkanTexturePixelFormat
        {
            VkFormat                format;

            uint8                   channels;   //颜色通道数
            char                    colors[4];
            uint8                   bits[4];
            VulkanBaseType          type;
        };//

        constexpr VulkanTexturePixelFormat pf_list[]=
        {
            { PF_RGBA4,      4,{'R','G','B','A'},{ 4, 4, 4, 4},VulkanBaseType::UNORM},      //Android 部分不支持
            { PF_BGRA4,      4,{'B','G','R','A'},{ 4, 4, 4, 4},VulkanBaseType::UNORM},      //ios不支持这个
            {UPF_RGB565,     3,{'R','G','B', 0 },{ 5, 6, 5, 0},VulkanBaseType::UNORM},
            {UPF_A1RGB5,     4,{'A','R','G','B'},{ 1, 5, 5, 5},VulkanBaseType::UNORM},
            {UPF_R8,         1,{'R', 0 , 0 , 0 },{ 8, 0, 0, 0},VulkanBaseType::UNORM},
            {UPF_RG8,        2,{'R','G', 0 , 0 },{ 8, 8, 0, 0},VulkanBaseType::UNORM},
            {UPF_RGBA8,      4,{'R','G','B','A'},{ 8, 8, 8, 8},VulkanBaseType::UNORM},
            {UPF_RGBA8S,     4,{'R','G','B','A'},{ 8, 8, 8, 8},VulkanBaseType::SNORM},
            {UPF_RGBA8U,     4,{'R','G','B','A'},{ 8, 8, 8, 8},VulkanBaseType::UINT},
            {UPF_RGBA8I,     4,{'R','G','B','A'},{ 8, 8, 8, 8},VulkanBaseType::SINT},
            {UPF_ABGR8,      4,{'A','B','G','R'},{ 8, 8, 8, 8},VulkanBaseType::UNORM},
            {UPF_A2BGR10,    4,{'A','B','G','R'},{ 2,10,10,10},VulkanBaseType::UNORM},
            {UPF_R16,        1,{'R', 0 , 0 , 0 },{16, 0, 0, 0},VulkanBaseType::UNORM},
            {UPF_R16U,       1,{'R', 0 , 0 , 0 },{16, 0, 0, 0},VulkanBaseType::UINT},
            {UPF_R16I,       1,{'R', 0 , 0 , 0 },{16, 0, 0, 0},VulkanBaseType::SINT},
            {UPF_R16F,       1,{'R', 0 , 0 , 0 },{16, 0, 0, 0},VulkanBaseType::SFLOAT},
            {UPF_RG16,       2,{'R','G', 0 , 0 },{16,16, 0, 0},VulkanBaseType::UNORM},
            {UPF_RG16U,      2,{'R','G', 0 , 0 },{16,16, 0, 0},VulkanBaseType::UINT},
            {UPF_RG16I,      2,{'R','G', 0 , 0 },{16,16, 0, 0},VulkanBaseType::SINT},
            {UPF_RG16F,      2,{'R','G', 0 , 0 },{16,16, 0, 0},VulkanBaseType::SFLOAT},
            { PF_RGBA16UN,   4,{'R','G','B','A'},{16,16,16,16},VulkanBaseType::UNORM},
            { PF_RGBA16SN,   4,{'R','G','B','A'},{16,16,16,16},VulkanBaseType::SNORM},
            {UPF_RGBA16U,    4,{'R','G','B','A'},{16,16,16,16},VulkanBaseType::UINT},
            {UPF_RGBA16I,    4,{'R','G','B','A'},{16,16,16,16},VulkanBaseType::SINT},
            {UPF_RGBA16F,    4,{'R','G','B','A'},{16,16,16,16},VulkanBaseType::SFLOAT},
            {UPF_R32U,       1,{'R', 0 , 0 , 0 },{32, 0, 0, 0},VulkanBaseType::UINT},
            {UPF_R32I,       1,{'R', 0 , 0 , 0 },{32, 0, 0, 0},VulkanBaseType::SINT},
            {UPF_R32F,       1,{'R', 0 , 0 , 0 },{32, 0, 0, 0},VulkanBaseType::SFLOAT},
            {UPF_RG32U,      2,{'R','G', 0 , 0 },{32,32, 0, 0},VulkanBaseType::UINT},
            {UPF_RG32I,      2,{'R','G', 0 , 0 },{32,32, 0, 0},VulkanBaseType::SINT},
            {UPF_RG32F,      2,{'R','G', 0 , 0 },{32,32, 0, 0},VulkanBaseType::SFLOAT},
            { PF_RGB32U,     3,{'R','G','B', 0 },{32,32,32, 0},VulkanBaseType::UINT},
            { PF_RGB32I,     3,{'R','G','B', 0 },{32,32,32, 0},VulkanBaseType::SINT},
            { PF_RGB32F,     3,{'R','G','B', 0 },{32,32,32, 0},VulkanBaseType::SFLOAT},
            {UPF_RGBA32U,    4,{'R','G','B','A'},{32,32,32,32},VulkanBaseType::UINT},
            {UPF_RGBA32I,    4,{'R','G','B','A'},{32,32,32,32},VulkanBaseType::SINT},
            {UPF_RGBA32F,    4,{'R','G','B','A'},{32,32,32,32},VulkanBaseType::SFLOAT},
            {UPF_B10GR11UF,  3,{'B','G','R', 0 },{10,11,11, 0},VulkanBaseType::UFLOAT}
        };

        constexpr uint VulkanTexturePixelFormatCount=sizeof(pf_list)/sizeof(VulkanTexturePixelFormat);

        const VkFormat GetVulkanFormat(const TexPixelFormat &tpf)
        {
            const VulkanTexturePixelFormat *pf=pf_list;

            for(uint i=0;i<VulkanTexturePixelFormatCount;i++,++pf)
            {
                if(tpf.channels!=pf->channels)continue;
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

        bool TextureLoader::Load(io::InputStream *is)
        {
            if(!is)return(false);

            if(is->Read(&file_header,sizeof(TextureFileHeader))!=sizeof(TextureFileHeader))
                return(false);
            
            constexpr char TEXTURE_FILE_HEADER[]="Texture";
            constexpr uint TEXTURE_FILE_HEADER_LENGTH=sizeof(TEXTURE_FILE_HEADER)-1;

            if(memcmp(&file_header.id_str,TEXTURE_FILE_HEADER,TEXTURE_FILE_HEADER_LENGTH))
                return(false);

            if(file_header.version!=0)
                return(false);

//            if(file_header.type!=type)
//                return(false);

            if(file_header.pixel_format.channels==0)
            {
                if(file_header.pixel_format.compress_format<0
                 ||file_header.pixel_format.compress_format>=CompressFormatCount)
                    return(false);

                format=CompressFormatList[file_header.pixel_format.compress_format];
            }
            else
            {
                format=GetVulkanFormat(file_header.pixel_format);
            }

            //计算0级mipmap图像的字节数
            mipmap_zero_total_bytes=(GetPixelsCount()*file_header.pixel_format.pixel_bits())>>3;

            total_bytes=GetTotalBytes();

            const uint32 file_left_bytes=is->Available();

            if(file_left_bytes<total_bytes)
                return(false);

            void *ptr=OnBegin(total_bytes,format);

            if(!ptr)
                return(false);

            if(is->Read(ptr,total_bytes)!=total_bytes)
                OnError();

            OnEnd();

            return(true);
        }

        bool TextureLoader::Load(const OSString &filename)
        {
            io::OpenFileInputStream fis(filename);

            if(!fis)
            {
                GLogError(OS_TEXT("[ERROR] open texture file<")+filename+OS_TEXT("> failed."));
                return(false);
            }

            return this->Load(&fis);
        }
    }//namespace graph
}//namespace hgl
