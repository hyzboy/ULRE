#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKBuffer.h>
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
        {UFMT_BGRA4,      4,{'B','G','R','A'},{ 4, 4, 4, 4},VulkanDataType::UNORM},
        {UFMT_RGB565,     3,{'R','G','B', 0 },{ 5, 6, 5, 0},VulkanDataType::UNORM},
        {UFMT_A1RGB5,     4,{'A','R','G','B'},{ 1, 5, 5, 5},VulkanDataType::UNORM},
        {UFMT_R8,         1,{'R', 0 , 0 , 0 },{ 8, 0, 0, 0},VulkanDataType::UNORM},
        {UFMT_RG8,        2,{'R','G', 0 , 0 },{ 8, 8, 0, 0},VulkanDataType::UNORM},
        {UFMT_RGBA8,      4,{'R','G','B','A'},{ 8, 8, 8, 8},VulkanDataType::UNORM},
        {UFMT_A2BGR10,    4,{'A','B','G','R'},{ 2,10,10,10},VulkanDataType::UNORM},
        {UFMT_R16,        1,{'R', 0 , 0 , 0 },{16, 0, 0, 0},VulkanDataType::UNORM},
        {UFMT_R16F,       1,{'R', 0 , 0 , 0 },{16, 0, 0, 0},VulkanDataType::SFLOAT},
        {UFMT_RG16,       2,{'R','G', 0 , 0 },{16,16, 0, 0},VulkanDataType::UNORM},
        {UFMT_RG16F,      2,{'R','G', 0 , 0 },{16,16, 0, 0},VulkanDataType::SFLOAT},
        {UFMT_RGBA16,     4,{'R','G','B','A'},{16,16,16,16},VulkanDataType::UNORM},
        {UFMT_RGBA16F,    4,{'R','G','B','A'},{16,16,16,16},VulkanDataType::SFLOAT},
        {UFMT_R32U,       1,{'R', 0 , 0 , 0 },{32, 0, 0, 0},VulkanDataType::UINT},
        {UFMT_R32I,       1,{'R', 0 , 0 , 0 },{32, 0, 0, 0},VulkanDataType::SINT},
        {UFMT_R32F,       1,{'R', 0 , 0 , 0 },{32, 0, 0, 0},VulkanDataType::SFLOAT},
        {UFMT_RG32U,      2,{'R','G', 0 , 0 },{32,32, 0, 0},VulkanDataType::UINT},
        {UFMT_RG32I,      2,{'R','G', 0 , 0 },{32,32, 0, 0},VulkanDataType::SINT},
        {UFMT_RG32F,      2,{'R','G', 0 , 0 },{32,32, 0, 0},VulkanDataType::SFLOAT},
        {UFMT_RGB32U,     3,{'R','G','B', 0 },{32,32,32, 0},VulkanDataType::UINT},
        {UFMT_RGB32I,     3,{'R','G','B', 0 },{32,32,32, 0},VulkanDataType::SINT},
        {UFMT_RGB32F,     3,{'R','G','B', 0 },{32,32,32, 0},VulkanDataType::SFLOAT},
        {UFMT_RGBA32U,    4,{'R','G','B','A'},{32,32,32,32},VulkanDataType::UINT},
        {UFMT_RGBA32I,    4,{'R','G','B','A'},{32,32,32,32},VulkanDataType::SINT},
        {UFMT_RGBA32F,    4,{'R','G','B','A'},{32,32,32,32},VulkanDataType::SFLOAT},
        {UFMT_B10GR11UF,  3,{'B','G','R', 0 },{10,11,11, 0},VulkanDataType::UFLOAT}
    };

    constexpr uint PixelFormatCount=sizeof(pf_list)/sizeof(PixelFormat);

    #pragma pack(push,1)
    struct Tex2DFileHeader
    {
        uint8   id[6];                  ///<Tex2D\x1A
        uint8   version;                ///<必须为2
        bool    mipmaps;
        uint32  width;
        uint32  height;
        uint8   channels;
        char    colors[4];
        uint8   bits[4];
        uint8   datatype;

    public:

        const uint pixel_count()const{return width*height;}
        const uint pixel_bytes()const{return (bits[0]+bits[1]+bits[2]+bits[3])>>3;}
        const uint total_bytes()const{return pixel_count()*pixel_bytes();}

        const VkFormat vk_format()const
        {
            const PixelFormat *pf=pf_list;

            for(uint i=0;i<PixelFormatCount;i++,++pf)
            {
                if(channels!=pf->channels)continue;
                if(datatype!=(uint8)pf->type)continue;

                if(colors[0]!=pf->colors[0])continue;
                if(colors[1]!=pf->colors[1])continue;
                if(colors[2]!=pf->colors[2])continue;
                if(colors[3]!=pf->colors[3])continue;

                if(bits[0]!=pf->bits[0])continue;
                if(bits[1]!=pf->bits[1])continue;
                if(bits[2]!=pf->bits[2])continue;
                if(bits[3]!=pf->bits[3])continue;

                return pf->format;
            }

            return VK_FORMAT_UNDEFINED;
        }
    };//
    #pragma pack(pop)
}//namespace

Texture2D *CreateTextureFromFile(Device *device,const OSString &filename)
{
    if(!device)
        return(nullptr);

    io::OpenFileInputStream fis(filename);

    if(!fis)
    {
        LOG_ERROR(OS_TEXT("[ERROR] open texture file<")+filename+OS_TEXT("> failed."));
        return(nullptr);
    }

    const int64 file_length=fis->GetSize();

    if(file_length<sizeof(Tex2DFileHeader))
        return(nullptr);

    Tex2DFileHeader file_header;

    if(fis->Read(&file_header,sizeof(Tex2DFileHeader))!=sizeof(Tex2DFileHeader))
        return(nullptr);

    if(file_header.version!=2)
        return(nullptr);

    if(file_header.total_bytes()==0)
        return(nullptr);

    const VkFormat format=file_header.vk_format();

    if(format<=VK_FORMAT_BEGIN_RANGE
     ||format>=VK_FORMAT_END_RANGE)
        return(nullptr);

    const uint total_bytes=file_header.total_bytes();

    if(file_length<sizeof(Tex2DFileHeader)+total_bytes)
        return(nullptr);

    {
        vulkan::Buffer *buf=device->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,total_bytes);

        if(!buf)
            return(nullptr);

        void *pixel_data=buf->Map();

        fis->Read(pixel_data,total_bytes);

        buf->Unmap();

        Texture2D *tex=device->CreateTexture2D(format,buf,file_header.width,file_header.height);

        delete buf;
        return tex;
    }
}
VK_NAMESPACE_END
