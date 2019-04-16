#include"VK.h"

VK_NAMESPACE_BEGIN

namespace
{
#ifdef _DEBUG
    struct StrideByFormat
    {
        VkFormat format;        //格式，此值保留仅为参考
        uint32_t bytes;
    };

    #define STRIDE_BY_FORMAT(vktype,size)   {VK_FORMAT_##vktype,size}

    constexpr StrideByFormat stride_list[]=
#else
    
    #define STRIDE_BY_FORMAT(vktype,size)   size

    constexpr uint32_t stride_list[]=
#endif//
    {
        STRIDE_BY_FORMAT(UNDEFINED,                   0),

        STRIDE_BY_FORMAT(R4G4_UNORM_PACK8,            1),

        STRIDE_BY_FORMAT(R4G4B4A4_UNORM_PACK16,       2),
        STRIDE_BY_FORMAT(B4G4R4A4_UNORM_PACK16,       2),
        STRIDE_BY_FORMAT(R5G6B5_UNORM_PACK16,         2),
        STRIDE_BY_FORMAT(B5G6R5_UNORM_PACK16,         2),
        STRIDE_BY_FORMAT(R5G5B5A1_UNORM_PACK16,       2),
        STRIDE_BY_FORMAT(B5G5R5A1_UNORM_PACK16,       2),
        STRIDE_BY_FORMAT(A1R5G5B5_UNORM_PACK16,       2),

        STRIDE_BY_FORMAT(R8_UNORM,                    1),
        STRIDE_BY_FORMAT(R8_SNORM,                    1),
        STRIDE_BY_FORMAT(R8_USCALED,                  1),
        STRIDE_BY_FORMAT(R8_SSCALED,                  1),
        STRIDE_BY_FORMAT(R8_UINT,                     1),
        STRIDE_BY_FORMAT(R8_SINT,                     1),
        STRIDE_BY_FORMAT(R8_SRGB,                     1),

        STRIDE_BY_FORMAT(R8G8_UNORM,                  2),
        STRIDE_BY_FORMAT(R8G8_SNORM,                  2),
        STRIDE_BY_FORMAT(R8G8_USCALED,                2),
        STRIDE_BY_FORMAT(R8G8_SSCALED,                2),
        STRIDE_BY_FORMAT(R8G8_UINT,                   2),
        STRIDE_BY_FORMAT(R8G8_SINT,                   2),
        STRIDE_BY_FORMAT(R8G8_SRGB,                   2),

        STRIDE_BY_FORMAT(R8G8B8_UNORM,                3),
        STRIDE_BY_FORMAT(R8G8B8_SNORM,                3),
        STRIDE_BY_FORMAT(R8G8B8_USCALED,              3),
        STRIDE_BY_FORMAT(R8G8B8_SSCALED,              3),
        STRIDE_BY_FORMAT(R8G8B8_UINT,                 3),
        STRIDE_BY_FORMAT(R8G8B8_SINT,                 3),
        STRIDE_BY_FORMAT(R8G8B8_SRGB,                 3),
        STRIDE_BY_FORMAT(B8G8R8_UNORM,                3),
        STRIDE_BY_FORMAT(B8G8R8_SNORM,                3),
        STRIDE_BY_FORMAT(B8G8R8_USCALED,              3),
        STRIDE_BY_FORMAT(B8G8R8_SSCALED,              3),
        STRIDE_BY_FORMAT(B8G8R8_UINT,                 3),
        STRIDE_BY_FORMAT(B8G8R8_SINT,                 3),
        STRIDE_BY_FORMAT(B8G8R8_SRGB,                 3),

        STRIDE_BY_FORMAT(R8G8B8A8_UNORM,              4),
        STRIDE_BY_FORMAT(R8G8B8A8_SNORM,              4),
        STRIDE_BY_FORMAT(R8G8B8A8_USCALED,            4),
        STRIDE_BY_FORMAT(R8G8B8A8_SSCALED,            4),
        STRIDE_BY_FORMAT(R8G8B8A8_UINT,               4),
        STRIDE_BY_FORMAT(R8G8B8A8_SINT,               4),
        STRIDE_BY_FORMAT(R8G8B8A8_SRGB,               4),
        STRIDE_BY_FORMAT(B8G8R8A8_UNORM,              4),
        STRIDE_BY_FORMAT(B8G8R8A8_SNORM,              4),
        STRIDE_BY_FORMAT(B8G8R8A8_USCALED,            4),
        STRIDE_BY_FORMAT(B8G8R8A8_SSCALED,            4),
        STRIDE_BY_FORMAT(B8G8R8A8_UINT,               4),
        STRIDE_BY_FORMAT(B8G8R8A8_SINT,               4),
        STRIDE_BY_FORMAT(B8G8R8A8_SRGB,               4),
        STRIDE_BY_FORMAT(A8B8G8R8_UNORM_PACK32,       4),
        STRIDE_BY_FORMAT(A8B8G8R8_SNORM_PACK32,       4),
        STRIDE_BY_FORMAT(A8B8G8R8_USCALED_PACK32,     4),
        STRIDE_BY_FORMAT(A8B8G8R8_SSCALED_PACK32,     4),
        STRIDE_BY_FORMAT(A8B8G8R8_UINT_PACK32,        4),
        STRIDE_BY_FORMAT(A8B8G8R8_SINT_PACK32,        4),
        STRIDE_BY_FORMAT(A8B8G8R8_SRGB_PACK32,        4),
        STRIDE_BY_FORMAT(A2R10G10B10_UNORM_PACK32,    4),
        STRIDE_BY_FORMAT(A2R10G10B10_SNORM_PACK32,    4),
        STRIDE_BY_FORMAT(A2R10G10B10_USCALED_PACK32,  4),
        STRIDE_BY_FORMAT(A2R10G10B10_SSCALED_PACK32,  4),
        STRIDE_BY_FORMAT(A2R10G10B10_UINT_PACK32,     4),
        STRIDE_BY_FORMAT(A2R10G10B10_SINT_PACK32,     4),
        STRIDE_BY_FORMAT(A2B10G10R10_UNORM_PACK32,    4),
        STRIDE_BY_FORMAT(A2B10G10R10_SNORM_PACK32,    4),
        STRIDE_BY_FORMAT(A2B10G10R10_USCALED_PACK32,  4),
        STRIDE_BY_FORMAT(A2B10G10R10_SSCALED_PACK32,  4),
        STRIDE_BY_FORMAT(A2B10G10R10_UINT_PACK32,     4),
        STRIDE_BY_FORMAT(A2B10G10R10_SINT_PACK32,     4),

        STRIDE_BY_FORMAT(R16_UNORM,                   2),
        STRIDE_BY_FORMAT(R16_SNORM,                   2),
        STRIDE_BY_FORMAT(R16_USCALED,                 2),
        STRIDE_BY_FORMAT(R16_SSCALED,                 2),
        STRIDE_BY_FORMAT(R16_UINT,                    2),
        STRIDE_BY_FORMAT(R16_SINT,                    2),
        STRIDE_BY_FORMAT(R16_SFLOAT,                  2),

        STRIDE_BY_FORMAT(R16G16_UNORM,                4),
        STRIDE_BY_FORMAT(R16G16_SNORM,                4),
        STRIDE_BY_FORMAT(R16G16_USCALED,              4),
        STRIDE_BY_FORMAT(R16G16_SSCALED,              4),
        STRIDE_BY_FORMAT(R16G16_UINT,                 4),
        STRIDE_BY_FORMAT(R16G16_SINT,                 4),
        STRIDE_BY_FORMAT(R16G16_SFLOAT,               4),

        STRIDE_BY_FORMAT(R16G16B16_UNORM,             6),
        STRIDE_BY_FORMAT(R16G16B16_SNORM,             6),
        STRIDE_BY_FORMAT(R16G16B16_USCALED,           6),
        STRIDE_BY_FORMAT(R16G16B16_SSCALED,           6),
        STRIDE_BY_FORMAT(R16G16B16_UINT,              6),
        STRIDE_BY_FORMAT(R16G16B16_SINT,              6),
        STRIDE_BY_FORMAT(R16G16B16_SFLOAT,            6),

        STRIDE_BY_FORMAT(R16G16B16A16_UNORM,          8),
        STRIDE_BY_FORMAT(R16G16B16A16_SNORM,          8),
        STRIDE_BY_FORMAT(R16G16B16A16_USCALED,        8),
        STRIDE_BY_FORMAT(R16G16B16A16_SSCALED,        8),
        STRIDE_BY_FORMAT(R16G16B16A16_UINT,           8),
        STRIDE_BY_FORMAT(R16G16B16A16_SINT,           8),
        STRIDE_BY_FORMAT(R16G16B16A16_SFLOAT,         8),

        STRIDE_BY_FORMAT(R32_UINT,                    4),
        STRIDE_BY_FORMAT(R32_SINT,                    4),
        STRIDE_BY_FORMAT(R32_SFLOAT,                  4),

        STRIDE_BY_FORMAT(R32G32_UINT,                 8),
        STRIDE_BY_FORMAT(R32G32_SINT,                 8),
        STRIDE_BY_FORMAT(R32G32_SFLOAT,               8),

        STRIDE_BY_FORMAT(R32G32B32_UINT,              12),
        STRIDE_BY_FORMAT(R32G32B32_SINT,              12),
        STRIDE_BY_FORMAT(R32G32B32_SFLOAT,            12),

        STRIDE_BY_FORMAT(R32G32B32A32_UINT,           16),
        STRIDE_BY_FORMAT(R32G32B32A32_SINT,           16),
        STRIDE_BY_FORMAT(R32G32B32A32_SFLOAT,         16),

        STRIDE_BY_FORMAT(R64_UINT,                    8),
        STRIDE_BY_FORMAT(R64_SINT,                    8),
        STRIDE_BY_FORMAT(R64_SFLOAT,                  8),

        STRIDE_BY_FORMAT(R64G64_UINT,                 16),
        STRIDE_BY_FORMAT(R64G64_SINT,                 16),
        STRIDE_BY_FORMAT(R64G64_SFLOAT,               16),

        STRIDE_BY_FORMAT(R64G64B64_UINT,              24),
        STRIDE_BY_FORMAT(R64G64B64_SINT,              24),
        STRIDE_BY_FORMAT(R64G64B64_SFLOAT,            24),

        STRIDE_BY_FORMAT(R64G64B64A64_UINT,           32),
        STRIDE_BY_FORMAT(R64G64B64A64_SINT,           32),
        STRIDE_BY_FORMAT(R64G64B64A64_SFLOAT,         32),

        STRIDE_BY_FORMAT(B10G11R11_UFLOAT_PACK32,     4),
        STRIDE_BY_FORMAT(E5B9G9R9_UFLOAT_PACK32,      4),

        STRIDE_BY_FORMAT(D16_UNORM,                   2),
        STRIDE_BY_FORMAT(X8_D24_UNORM_PACK32,         4),
        STRIDE_BY_FORMAT(D32_SFLOAT,                  4),
        STRIDE_BY_FORMAT(S8_UINT,                     1),
        STRIDE_BY_FORMAT(D16_UNORM_S8_UINT,           3),
        STRIDE_BY_FORMAT(D24_UNORM_S8_UINT,           4),
        STRIDE_BY_FORMAT(D32_SFLOAT_S8_UINT,          5)
    };    

#ifdef _DEBUG
    constexpr uint32_t STRIDE_FORMAT_COUNT=sizeof(stride_list)/sizeof(StrideByFormat);

    uint32_t GetStrideBytesByFormat(const VkFormat &format)
    {
        if(format<=VK_FORMAT_UNDEFINED||format>=STRIDE_FORMAT_COUNT)return(0);

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
    for(uint32 i=0;i<STRIDE_FORMAT_COUNT;i++)
    {
        if(stride_list[i].format!=i)return(false);
        if(stride_list[i].bytes!=GetStrideBytesByFormat(stride_list[i].format))return(false);
    }

    return(true);
}
#endif//_DEBUG

uint32_t GetStrideByFormat(const VkFormat &format)
{
    if(format<=VK_FORMAT_UNDEFINED||format>=STRIDE_FORMAT_COUNT)return(0);

    #ifdef _DEBUG
        return stride_list[format].bytes;
    #else
        return stride_list[format];
    #endif//_DEBUG
}
VK_NAMESPACE_END
