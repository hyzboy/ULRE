#include<hgl/graph/vulkan/VK.h>

VK_NAMESPACE_BEGIN

namespace
{
    //enum class ColorDataTypeEnum:uint8
    //{
    //    X=0,
    //    R,G,B,A,DEPTH,STENCIL
    //};

    //enum class ColorDataType:uint8
    //{
    //    UNORM=1,
    //    SNORM,
    //    USCALED,
    //    SSCALE,
    //    UINT,
    //    SINT,
    //    UFLOAT,
    //    SFLOAT,
    //    SRGB,
    //};//

    struct VulkanTextureFormat
    {
#ifdef _DEBUG
        VkFormat format;        ///<Vulkan格式，此值保留仅为参考
#endif//_DEBUG

        uint32_t bytes;         ///<每象素字节数

        char name[16];          ///<名称
    };

    #define VULKAN_TEXTURE_FORMAT_DEFINE(name,size)   {FMT_##name,size,#name}

    constexpr VulkanTextureFormat vulkan_format_list[]=
    {
        VULKAN_TEXTURE_FORMAT_DEFINE(UNDEFINED,     0),

        VULKAN_TEXTURE_FORMAT_DEFINE(RG4UN,         1),

        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA4,         2),
        VULKAN_TEXTURE_FORMAT_DEFINE(BGRA4,         2),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB565,        2),
        VULKAN_TEXTURE_FORMAT_DEFINE(BGR565,        2),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB5A1,        2),
        VULKAN_TEXTURE_FORMAT_DEFINE(BGR5A1,        2),
        VULKAN_TEXTURE_FORMAT_DEFINE(A1RGB5,        2),

        VULKAN_TEXTURE_FORMAT_DEFINE(R8UN,          1),
        VULKAN_TEXTURE_FORMAT_DEFINE(R8SN,          1),
        VULKAN_TEXTURE_FORMAT_DEFINE(R8US,          1),
        VULKAN_TEXTURE_FORMAT_DEFINE(R8SS,          1),
        VULKAN_TEXTURE_FORMAT_DEFINE(R8U,           1),
        VULKAN_TEXTURE_FORMAT_DEFINE(R8I,           1),
        VULKAN_TEXTURE_FORMAT_DEFINE(R8s,           1),

        VULKAN_TEXTURE_FORMAT_DEFINE(RG8UN,         2),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG8SN,         2),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG8US,         2),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG8SS,         2),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG8U,          2),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG8I,          2),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG8s,          2),

        VULKAN_TEXTURE_FORMAT_DEFINE(RGB8UN,        3),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB8SN,        3),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB8US,        3),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB8SS,        3),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB8U,         3),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB8I,         3),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB8s,         3),
        
        VULKAN_TEXTURE_FORMAT_DEFINE(BGR8UN,        3),
        VULKAN_TEXTURE_FORMAT_DEFINE(BGR8SN,        3),
        VULKAN_TEXTURE_FORMAT_DEFINE(BGR8US,        3),
        VULKAN_TEXTURE_FORMAT_DEFINE(BGR8SS,        3),
        VULKAN_TEXTURE_FORMAT_DEFINE(BGR8U,         3),
        VULKAN_TEXTURE_FORMAT_DEFINE(BGR8I,         3),
        VULKAN_TEXTURE_FORMAT_DEFINE(BGR8s,         3),

        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA8UN,       4),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA8SN,       4),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA8US,       4),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA8SS,       4),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA8U,        4),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA8I,        4),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA8s,        4),

        VULKAN_TEXTURE_FORMAT_DEFINE(BGRA8UN,       4),
        VULKAN_TEXTURE_FORMAT_DEFINE(BGRA8SN,       4),
        VULKAN_TEXTURE_FORMAT_DEFINE(BGRA8US,       4),
        VULKAN_TEXTURE_FORMAT_DEFINE(BGRA8SS,       4),
        VULKAN_TEXTURE_FORMAT_DEFINE(BGRA8U,        4),
        VULKAN_TEXTURE_FORMAT_DEFINE(BGRA8I,        4),
        VULKAN_TEXTURE_FORMAT_DEFINE(BGRA8s,        4),

        VULKAN_TEXTURE_FORMAT_DEFINE(ABGR8UN,       4),
        VULKAN_TEXTURE_FORMAT_DEFINE(ABGR8SN,       4),
        VULKAN_TEXTURE_FORMAT_DEFINE(ABGR8US,       4),
        VULKAN_TEXTURE_FORMAT_DEFINE(ABGR8SS,       4),
        VULKAN_TEXTURE_FORMAT_DEFINE(ABGR8U,        4),
        VULKAN_TEXTURE_FORMAT_DEFINE(ABGR8I,        4),
        VULKAN_TEXTURE_FORMAT_DEFINE(ABGR8s,        4),

        VULKAN_TEXTURE_FORMAT_DEFINE(A2RGB10UN,     4),
        VULKAN_TEXTURE_FORMAT_DEFINE(A2RGB10SN,     4),
        VULKAN_TEXTURE_FORMAT_DEFINE(A2RGB10US,     4),
        VULKAN_TEXTURE_FORMAT_DEFINE(A2RGB10SS,     4),
        VULKAN_TEXTURE_FORMAT_DEFINE(A2RGB10U,      4),
        VULKAN_TEXTURE_FORMAT_DEFINE(A2RGB10I,      4),

        VULKAN_TEXTURE_FORMAT_DEFINE(A2BGR10UN,     4),
        VULKAN_TEXTURE_FORMAT_DEFINE(A2BGR10SN,     4),
        VULKAN_TEXTURE_FORMAT_DEFINE(A2BGR10US,     4),
        VULKAN_TEXTURE_FORMAT_DEFINE(A2BGR10SS,     4),
        VULKAN_TEXTURE_FORMAT_DEFINE(A2BGR10U,      4),
        VULKAN_TEXTURE_FORMAT_DEFINE(A2BGR10I,      4),

        VULKAN_TEXTURE_FORMAT_DEFINE(R16UN,         2),
        VULKAN_TEXTURE_FORMAT_DEFINE(R16SN,         2),
        VULKAN_TEXTURE_FORMAT_DEFINE(R16US,         2),
        VULKAN_TEXTURE_FORMAT_DEFINE(R16SS,         2),
        VULKAN_TEXTURE_FORMAT_DEFINE(R16U,          2),
        VULKAN_TEXTURE_FORMAT_DEFINE(R16I,          2),
        VULKAN_TEXTURE_FORMAT_DEFINE(R16F,          2),

        VULKAN_TEXTURE_FORMAT_DEFINE(RG16UN,        4),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG16SN,        4),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG16US,        4),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG16SS,        4),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG16U,         4),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG16I,         4),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG16F,         4),

        VULKAN_TEXTURE_FORMAT_DEFINE(RGB16UN,       6),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB16SN,       6),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB16US,       6),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB16SS,       6),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB16U,        6),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB16I,        6),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB16F,        6),

        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA16UN,      8),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA16SN,      8),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA16US,      8),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA16SS,      8),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA16U,       8),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA16I,       8),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA16F,       8),

        VULKAN_TEXTURE_FORMAT_DEFINE(R32U,          4),
        VULKAN_TEXTURE_FORMAT_DEFINE(R32I,          4),
        VULKAN_TEXTURE_FORMAT_DEFINE(R32F,          4),

        VULKAN_TEXTURE_FORMAT_DEFINE(RG32U,         8),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG32I,         8),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG32F,         8),

        VULKAN_TEXTURE_FORMAT_DEFINE(RGB32U,        12),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB32I,        12),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB32F,        12),

        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA32U,       16),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA32I,       16),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA32F,       16),

        VULKAN_TEXTURE_FORMAT_DEFINE(R64U,          8),
        VULKAN_TEXTURE_FORMAT_DEFINE(R64I,          8),
        VULKAN_TEXTURE_FORMAT_DEFINE(R64F,          8),

        VULKAN_TEXTURE_FORMAT_DEFINE(RG64U,         16),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG64I,         16),
        VULKAN_TEXTURE_FORMAT_DEFINE(RG64F,         16),

        VULKAN_TEXTURE_FORMAT_DEFINE(RGB64U,        24),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB64I,        24),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGB64F,        24),

        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA64U,       32),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA64I,       32),
        VULKAN_TEXTURE_FORMAT_DEFINE(RGBA64F,       32),

        VULKAN_TEXTURE_FORMAT_DEFINE(B10GR11UF,     4),
        VULKAN_TEXTURE_FORMAT_DEFINE(E5BGR9UF,      4),

        VULKAN_TEXTURE_FORMAT_DEFINE(D16UN,         2),
        VULKAN_TEXTURE_FORMAT_DEFINE(X8_D24UN,      4),
        VULKAN_TEXTURE_FORMAT_DEFINE(D32F,          4),
        VULKAN_TEXTURE_FORMAT_DEFINE(S8U,           1),
        VULKAN_TEXTURE_FORMAT_DEFINE(D16UN_S8U,     3),
        VULKAN_TEXTURE_FORMAT_DEFINE(D24UN_S8U,     4),
        VULKAN_TEXTURE_FORMAT_DEFINE(D32F_S8U,      5)
    };

#ifdef _DEBUG
    constexpr size_t TEXTURE_FORMAT_COUNT=sizeof(vulkan_format_list)/sizeof(VulkanTextureFormat);

    uint32_t GetStrideBytesByFormat(const VkFormat &format)
    {
        if(format<=VK_FORMAT_UNDEFINED||format>=TEXTURE_FORMAT_COUNT)return(0);

        if(format<=VK_FORMAT_UNDEFINED
         ||format>=VK_FORMAT_BC1_RGB_UNORM_BLOCK)return 0;

        if(format==VK_FORMAT_R4G4_UNORM_PACK8)return 1;
        if(format<=VK_FORMAT_A1R5G5B5_UNORM_PACK16)return 2;
        if(format<=VK_FORMAT_R8_SRGB)return 1;
        if(format<=VK_FORMAT_R8G8_SRGB)return 2;
        if(format<=VK_FORMAT_B8G8R8_SRGB)return 3;
        if(format<=VK_FORMAT_A2B10G10R10_SINT_PACK32)return 4;
        if(format<=VK_FORMAT_R16_SFLOAT)return 2;
        if(format<=VK_FORMAT_R16G16_SFLOAT)return 4;
        if(format<=VK_FORMAT_R16G16B16_SFLOAT)return 6;
        if(format<=VK_FORMAT_R16G16B16A16_SFLOAT)return 8;
        if(format<=VK_FORMAT_R32_SFLOAT)return 4;
        if(format<=VK_FORMAT_R32G32_SFLOAT)return 8;
        if(format<=VK_FORMAT_R32G32B32_SFLOAT)return 12;
        if(format<=VK_FORMAT_R32G32B32A32_SFLOAT)return 16;
        if(format<=VK_FORMAT_R64_SFLOAT)return 8;
        if(format<=VK_FORMAT_R64G64_SFLOAT)return 16;
        if(format<=VK_FORMAT_R64G64B64_SFLOAT)return 24;
        if(format<=VK_FORMAT_R64G64B64A64_SFLOAT)return 32;
        if(format<=VK_FORMAT_E5B9G9R9_UFLOAT_PACK32)return 4;
        if(format==VK_FORMAT_D16_UNORM)return 2;
        if(format<=VK_FORMAT_D32_SFLOAT)return 4;
        if(format==VK_FORMAT_S8_UINT)return 1;
        if(format==VK_FORMAT_D16_UNORM_S8_UINT)return 3;
        if(format==VK_FORMAT_D24_UNORM_S8_UINT)return 4;
        if(format==VK_FORMAT_D32_SFLOAT_S8_UINT)return 5;

        return(0);
    }
#else
    constexpr uint32_t STRIDE_FORMAT_COUNT=sizeof(stride_list)/sizeof(uint32);
#endif//_DEBUG
}//namespace

#ifdef _DEBUG
bool CheckStrideBytesByFormat()
{
    for(uint32 i=0;i<TEXTURE_FORMAT_COUNT;i++)
    {
        if(vulkan_format_list[i].format!=i)return(false);
        if(vulkan_format_list[i].bytes!=GetStrideBytesByFormat(vulkan_format_list[i].format))return(false);
    }

    return(true);
}
#endif//_DEBUG

uint32_t GetStrideByFormat(const VkFormat &format)
{
    if(format<=VK_FORMAT_UNDEFINED||format>=TEXTURE_FORMAT_COUNT)return(0);

    return vulkan_format_list[format].bytes;
}

const char *GetColorFormatName(const VkFormat &format)
{
    if(format<=VK_FORMAT_UNDEFINED||format>=TEXTURE_FORMAT_COUNT)return(nullptr);
    
    return vulkan_format_list[format].name;
}
VK_NAMESPACE_END
