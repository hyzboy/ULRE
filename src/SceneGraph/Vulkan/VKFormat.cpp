#include<hgl/graph/VKFormat.h>
#include<hgl/type/StrChar.h>

VK_NAMESPACE_BEGIN
namespace
{
    //enum class ColorDataTypeEnum:uint8
    //{
    //    X=0,
    //    R,G,B,A,DEPTH,STENCIL
    //};

    #define VULKAN_FORMAT_DEFINE(id,size,name,compress,color,depth,stencil) {id,size,name,TextureCompressType::compress,VulkanBaseType::color,VulkanBaseType::depth,VulkanBaseType::stencil}

    #define COLOR_FORMAT_DEFINE(name,size,color)            VULKAN_FORMAT_DEFINE(PF_##name,size,#name,NONE,    color,NONE,NONE)

    #define COLOR_6_FORMAT_DEFINE(type,byte)                COLOR_FORMAT_DEFINE(type##UN,          byte,UNORM), \
                                                            COLOR_FORMAT_DEFINE(type##SN,          byte,SNORM), \
                                                            COLOR_FORMAT_DEFINE(type##US,          byte,USCALED), \
                                                            COLOR_FORMAT_DEFINE(type##SS,          byte,SSCALED), \
                                                            COLOR_FORMAT_DEFINE(type##U,           byte,UINT), \
                                                            COLOR_FORMAT_DEFINE(type##I,           byte,SINT), \

    #define COLOR_7s_FORMAT_DEFINE(type,byte)               COLOR_6_FORMAT_DEFINE(type,byte)                    \
                                                            COLOR_FORMAT_DEFINE(type##s,           byte,SRGB),

    #define COLOR_7F_FORMAT_DEFINE(type,byte)               COLOR_6_FORMAT_DEFINE(type,byte)                    \
                                                            COLOR_FORMAT_DEFINE(type##F,           byte,SFLOAT),

    #define COLOR_UIF_FORMAT_DEFINE(type,byte)              COLOR_FORMAT_DEFINE(type##U,           byte,UINT), \
                                                            COLOR_FORMAT_DEFINE(type##I,           byte,SINT), \
                                                            COLOR_FORMAT_DEFINE(type##F,           byte,SFLOAT),

    #define COMPRESS_FORMAT_DEFINE(id,name,compress,color)  VULKAN_FORMAT_DEFINE(id,0,name,compress,color,NONE,NONE)
    
    #define DEPTH_FORMAT_DEFINE(name,size,type)             VULKAN_FORMAT_DEFINE(PF_##name,size,#name,NONE,NONE,type,  NONE)
    #define STENCIL_FORMAT_DEFINE(name,size,type)           VULKAN_FORMAT_DEFINE(PF_##name,size,#name,NONE,NONE,NONE,  type)
    #define DEPTH_STENCIL_FORMAT_DEFINE(name,size,dt,st)    VULKAN_FORMAT_DEFINE(PF_##name,size,#name,NONE,NONE,dt,    st  )


    #define S3TC_Us_FORMAT_DEFINE(name)         COMPRESS_FORMAT_DEFINE(PF_##name##UN,#name "UN",S3TC,UNORM),   \
                                                COMPRESS_FORMAT_DEFINE(PF_##name##s,#name "s",S3TC,SRGB)

    #define S3TC_NORM_FORMAT_DEFINE(name)       COMPRESS_FORMAT_DEFINE(PF_##name##UN,#name "UN",S3TC,UNORM),   \
                                                COMPRESS_FORMAT_DEFINE(PF_##name##SN,#name "SN",S3TC,SNORM)

    #define S3TC_FLOAT_FORMAT_DEFINE(name)      COMPRESS_FORMAT_DEFINE(PF_##name##UF,#name "UF",S3TC,UFLOAT),   \
                                                COMPRESS_FORMAT_DEFINE(PF_##name##SF,#name "SF",S3TC,SFLOAT)

    #define ETC2_FORMAT_DEFINE(name)            COMPRESS_FORMAT_DEFINE(PF_ETC2_##name##UN,"ETC2" #name "UN",ETC2,UNORM),   \
                                                COMPRESS_FORMAT_DEFINE(PF_ETC2_##name##s,"ETC2" #name "s",ETC2,SRGB)

    #define EAC_FORMAT_DEFINE(name)             COMPRESS_FORMAT_DEFINE(PF_EAC_##name##UN,"EAC" #name "UN",EAC,UNORM),   \
                                                COMPRESS_FORMAT_DEFINE(PF_EAC_##name##SN,"EAC" #name "SN",EAC,SNORM)

    #define ASTC_FORMAT_DEFINE(mat)             COMPRESS_FORMAT_DEFINE(PF_ASTC_##mat##UN,"ASTC" #mat "UN",ASTC,UNORM),  \
                                                COMPRESS_FORMAT_DEFINE(PF_ASTC_##mat##s,"ASTC" #mat "s",ASTC,SRGB)

    #define YUV_FORMAT_DEFINE(name)             COMPRESS_FORMAT_DEFINE(PF_##name,#name,YUV,UNORM)
    
    #define YUV_FORMAT_COMBO(bit)               YUV_FORMAT_DEFINE(YUYV##bit##_422),   \
                                                YUV_FORMAT_DEFINE(UYVY##bit##_422),   \
                                                YUV_FORMAT_DEFINE(YUV##bit##_420),    \
                                                YUV_FORMAT_DEFINE(Y##bit##_UV##bit##_420),  \
                                                YUV_FORMAT_DEFINE(YUV##bit##_422),    \
                                                YUV_FORMAT_DEFINE(Y##bit##_UV##bit##_422),  \
                                                YUV_FORMAT_DEFINE(YUV##bit##_444)

    #define PVRTC_FORMAT_DEFINE(level,bpp)      COMPRESS_FORMAT_DEFINE(PF_PVRTC##level##_##bpp##UN,"PVRTC" #level "_" #bpp "UN",PVRTC,UNORM),    \
                                                COMPRESS_FORMAT_DEFINE(PF_PVRTC##level##_##bpp##s,"PVRTC" #level "_" #bpp "s",PVRTC,SRGB)

    #define ASTC_SFLOAT_FORMAT_DEFINE(mat)      COMPRESS_FORMAT_DEFINE(PF_ASTC_##mat##UN,"ASTC" #mat "F",ASTC,SFLOAT)  

    constexpr VulkanFormat vulkan_color_format_list[]=
    {
        COLOR_FORMAT_DEFINE(UNDEFINED,     0,UNORM),

        COLOR_FORMAT_DEFINE(RG4UN,         1,UNORM),

        COLOR_FORMAT_DEFINE(RGBA4,         2,UNORM),
        COLOR_FORMAT_DEFINE(BGRA4,         2,UNORM),
        COLOR_FORMAT_DEFINE(RGB565,        2,UNORM),
        COLOR_FORMAT_DEFINE(BGR565,        2,UNORM),
        COLOR_FORMAT_DEFINE(RGB5A1,        2,UNORM),
        COLOR_FORMAT_DEFINE(BGR5A1,        2,UNORM),
        COLOR_FORMAT_DEFINE(A1RGB5,        2,UNORM),

        COLOR_7s_FORMAT_DEFINE(R8,1)
        COLOR_7s_FORMAT_DEFINE(RG8,2)
        COLOR_7s_FORMAT_DEFINE(RGB8,3)
        COLOR_7s_FORMAT_DEFINE(BGR8,3)
        COLOR_7s_FORMAT_DEFINE(RGBA8,4)
        COLOR_7s_FORMAT_DEFINE(BGRA8,4)
        COLOR_7s_FORMAT_DEFINE(ABGR8,4)

        COLOR_6_FORMAT_DEFINE(A2RGB10,4)
        COLOR_6_FORMAT_DEFINE(A2BGR10,4)

        COLOR_7F_FORMAT_DEFINE(R16,2)
        COLOR_7F_FORMAT_DEFINE(RG16,4)
        COLOR_7F_FORMAT_DEFINE(RGB16,6)
        COLOR_7F_FORMAT_DEFINE(RGBA16,8)

        COLOR_UIF_FORMAT_DEFINE(R32,4)
        COLOR_UIF_FORMAT_DEFINE(RG32,8)
        COLOR_UIF_FORMAT_DEFINE(RGB32,12)
        COLOR_UIF_FORMAT_DEFINE(RGBA32,16)

        COLOR_UIF_FORMAT_DEFINE(R64,8)
        COLOR_UIF_FORMAT_DEFINE(RG64,16)
        COLOR_UIF_FORMAT_DEFINE(RGB64,24)
        COLOR_UIF_FORMAT_DEFINE(RGBA64,32)

        COLOR_FORMAT_DEFINE(B10GR11UF,     4,UFLOAT),
        COLOR_FORMAT_DEFINE(E5BGR9UF,      4,UFLOAT),

        DEPTH_FORMAT_DEFINE(D16UN,         2,UNORM),
        DEPTH_FORMAT_DEFINE(X8_D24UN,      4,UNORM),
        DEPTH_FORMAT_DEFINE(D32F,          4,SFLOAT),
        STENCIL_FORMAT_DEFINE(S8U,           1,UINT),
        DEPTH_STENCIL_FORMAT_DEFINE(D16UN_S8U,     3,UNORM,UINT),
        DEPTH_STENCIL_FORMAT_DEFINE(D24UN_S8U,     4,UNORM,UINT),
        DEPTH_STENCIL_FORMAT_DEFINE(D32F_S8U,      5,SFLOAT,UINT),

        S3TC_Us_FORMAT_DEFINE(     BC1_RGB),
        S3TC_Us_FORMAT_DEFINE(     BC1_RGBA),
        S3TC_Us_FORMAT_DEFINE(     BC2),
        S3TC_Us_FORMAT_DEFINE(     BC3),
        S3TC_NORM_FORMAT_DEFINE(   BC4),
        S3TC_NORM_FORMAT_DEFINE(   BC5),
        S3TC_FLOAT_FORMAT_DEFINE(  BC6),
        S3TC_Us_FORMAT_DEFINE(     BC7),
          
        ETC2_FORMAT_DEFINE(RGB8),
        ETC2_FORMAT_DEFINE(RGB8A1),
        ETC2_FORMAT_DEFINE(RGBA8),

        EAC_FORMAT_DEFINE(R11),
        EAC_FORMAT_DEFINE(RG11),

        ASTC_FORMAT_DEFINE(4x4),
        ASTC_FORMAT_DEFINE(5x4),
        ASTC_FORMAT_DEFINE(5x5),
        ASTC_FORMAT_DEFINE(6x5),
        ASTC_FORMAT_DEFINE(6x6),
        ASTC_FORMAT_DEFINE(8x5),
        ASTC_FORMAT_DEFINE(8x6),
        ASTC_FORMAT_DEFINE(8x8),
        ASTC_FORMAT_DEFINE(10x5),
        ASTC_FORMAT_DEFINE(10x6),
        ASTC_FORMAT_DEFINE(10x8),
        ASTC_FORMAT_DEFINE(10x10),
        ASTC_FORMAT_DEFINE(12x10),
        ASTC_FORMAT_DEFINE(12x12),

        YUV_FORMAT_COMBO(8),

        COLOR_FORMAT_DEFINE(R10X6UN,2,UNORM),
        COLOR_FORMAT_DEFINE(RG10X6UN,4,UNORM),
        COLOR_FORMAT_DEFINE(RGBA10X6UN,8,UNORM),

        YUV_FORMAT_COMBO(10),

        COLOR_FORMAT_DEFINE(R12X4UN,2,UNORM),
        COLOR_FORMAT_DEFINE(RG12X4UN,4,UNORM),
        COLOR_FORMAT_DEFINE(RGBA12X4UN,8,UNORM),

        YUV_FORMAT_COMBO(12),
        YUV_FORMAT_COMBO(16),

        PVRTC_FORMAT_DEFINE(1,2),
        PVRTC_FORMAT_DEFINE(1,4),
        PVRTC_FORMAT_DEFINE(2,2),
        PVRTC_FORMAT_DEFINE(2,4),
        
        ASTC_SFLOAT_FORMAT_DEFINE(4x4),
        ASTC_SFLOAT_FORMAT_DEFINE(5x4),
        ASTC_SFLOAT_FORMAT_DEFINE(5x5),
        ASTC_SFLOAT_FORMAT_DEFINE(6x5),
        ASTC_SFLOAT_FORMAT_DEFINE(6x6),
        ASTC_SFLOAT_FORMAT_DEFINE(8x5),
        ASTC_SFLOAT_FORMAT_DEFINE(8x6),
        ASTC_SFLOAT_FORMAT_DEFINE(8x8),
        ASTC_SFLOAT_FORMAT_DEFINE(10x5),
        ASTC_SFLOAT_FORMAT_DEFINE(10x6),
        ASTC_SFLOAT_FORMAT_DEFINE(10x8),
        ASTC_SFLOAT_FORMAT_DEFINE(10x10),
        ASTC_SFLOAT_FORMAT_DEFINE(12x10),
        ASTC_SFLOAT_FORMAT_DEFINE(12x12)
    };

    const VulkanFormat *vulkan_yuv_format_list          =vulkan_color_format_list+PF_RANGE_SIZE;
    const VulkanFormat *vulkan_pvrtc_format_list        =vulkan_color_format_list+PF_RANGE_SIZE+PF_YUV_RANGE_SIZE;
    const VulkanFormat *vulkan_astc_sfloat_format_list  =vulkan_color_format_list+PF_RANGE_SIZE+PF_YUV_RANGE_SIZE+PF_PVRTC_RANGE_SIZE;
    
    constexpr size_t TEXTURE_FORMAT_COUNT=sizeof(vulkan_color_format_list)/sizeof(VulkanFormat);

#ifdef _DEBUG
    uint32_t GetStrideBytesByFormat(const VkFormat &format)
    {
        if(format<=PF_BEGIN_RANGE)
            return 0;
        
        if(format>PF_END_RANGE)
        {
            if(format==VK_FORMAT_R10X6_UNORM_PACK16)return 2;
            if(format==VK_FORMAT_R10X6G10X6_UNORM_2PACK16)return 4;
            if(format==VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16)return 8;

            if(format==VK_FORMAT_R12X4_UNORM_PACK16)return 2;
            if(format==VK_FORMAT_R12X4G12X4_UNORM_2PACK16)return 4;
            if(format==VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16)return 8;

            return 0;
        }

        if(format>=VK_FORMAT_BC1_RGB_UNORM_BLOCK)return 0;

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
#endif//_DEBUG
}//namespace

#ifdef _DEBUG
bool CheckStrideBytesByFormat()
{
    const VulkanFormat *vcf=vulkan_color_format_list;

    for(uint32_t i=PF_BEGIN_RANGE;i<=PF_END_RANGE;i++)
    {
        if(vcf->format!=i)return(false);
        if(vcf->bytes!=GetStrideBytesByFormat((VkFormat)i))return(false);

        ++vcf;
    }

    return(true);
}
#endif//_DEBUG

const VulkanFormat *GetVulkanFormatList(uint32_t &count)
{
    count=sizeof(vulkan_color_format_list)/sizeof(VulkanFormat);

    return vulkan_color_format_list;
}

const VulkanFormat *GetVulkanFormat(const VkFormat &format)
{
    if(format<=PF_BEGIN_RANGE)return(nullptr);

    if(format<=PF_END_RANGE)
        return vulkan_color_format_list+format;
        
    if(format>=PF_YUV_BEGIN_RANGE&&format<=PF_YUV_END_RANGE)
        return vulkan_yuv_format_list+format-PF_YUV_BEGIN_RANGE;

    if(format>=PF_PVRTC_BEGIN_RANGE&&format<=PF_PVRTC_END_RANGE)
        return vulkan_pvrtc_format_list+format-PF_PVRTC_BEGIN_RANGE;

    if(format>=PF_ASTC_SFLOAT_BEGIN_RANGE&&format<=PF_ASTC_SFLOAT_END_RANGE)
        return vulkan_astc_sfloat_format_list+format-PF_ASTC_SFLOAT_BEGIN_RANGE;

    return nullptr;
}

const VulkanFormat *GetVulkanFormat(const char *fmt_name)
{
    if(!fmt_name||!*fmt_name)return(nullptr);
    
    const VulkanFormat *vcf=vulkan_color_format_list;

    for(uint32_t i=PF_BEGIN_RANGE;i<=PF_END_RANGE;i++)
    {
        if(hgl::strcmp(fmt_name,vcf->name)==0)
            return vcf;

        ++vcf;
    }

    return(nullptr);
}

namespace
{
    constexpr VulkanColorSpace vk_color_space_list[]=
    {
        {VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,         "sRGB Nonlinear",           false},
        {VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT,   "Display P3 Nonlinear",     false},
        {VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT,   "Extended sRGB Linear",     true},
        {VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT,      "Display P3 Linear",        true},
        {VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT,       "DCI P3 Nonlinear",         false},
        {VK_COLOR_SPACE_BT709_LINEAR_EXT,           "BT709 Linear",             true},
        {VK_COLOR_SPACE_BT709_NONLINEAR_EXT,        "BT709 Nonlinear",          false},
        {VK_COLOR_SPACE_BT2020_LINEAR_EXT,          "BT2020 Linear",            true},
        {VK_COLOR_SPACE_HDR10_ST2084_EXT,           "HDR10 ST2084",             false},
        {VK_COLOR_SPACE_DOLBYVISION_EXT,            "Dolby Vision",             false},
        {VK_COLOR_SPACE_HDR10_HLG_EXT,              "HDR10 HLG",                false},
        {VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT,        "AdobeRGB Linear",          true},
        {VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT,     "AdobeRGB Nonlinear",       false},
        {VK_COLOR_SPACE_PASS_THROUGH_EXT,           "Pass through",             false},
        {VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT,"Extended sRGB Nonlinear",  false},
        {VK_COLOR_SPACE_DISPLAY_NATIVE_AMD,         "Display Native AMD",       false}
    };
}//namespace

const VulkanColorSpace *GetVulkanColorSpace(const VkColorSpaceKHR &cs)
{
    for(const VulkanColorSpace &vcs:vk_color_space_list)
        if(vcs.cs==cs)
            return &vcs;

    return nullptr;
}
VK_NAMESPACE_END
