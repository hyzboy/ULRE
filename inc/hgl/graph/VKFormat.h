#pragma once
#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/VKNamespace.h>

VK_NAMESPACE_BEGIN

#define PF_UNDEFINED   VK_FORMAT_UNDEFINED

#define PF_RG4UN       VK_FORMAT_R4G4_UNORM_PACK8

#define PF_RGBA4       VK_FORMAT_R4G4B4A4_UNORM_PACK16
#define PF_BGRA4       VK_FORMAT_B4G4R4A4_UNORM_PACK16
#define PF_RGB565      VK_FORMAT_R5G6B5_UNORM_PACK16
#define PF_BGR565      VK_FORMAT_B5G6R5_UNORM_PACK16
#define PF_RGB5A1      VK_FORMAT_R5G5B5A1_UNORM_PACK16
#define PF_BGR5A1      VK_FORMAT_B5G5R5A1_UNORM_PACK16
#define PF_A1RGB5      VK_FORMAT_A1R5G5B5_UNORM_PACK16

#define PF_R8UN        VK_FORMAT_R8_UNORM
#define PF_R8SN        VK_FORMAT_R8_SNORM
#define PF_R8US        VK_FORMAT_R8_USCALED
#define PF_R8SS        VK_FORMAT_R8_SSCALED
#define PF_R8U         VK_FORMAT_R8_UINT
#define PF_R8I         VK_FORMAT_R8_SINT
#define PF_R8s         VK_FORMAT_R8_SRGB

#define PF_RG8UN       VK_FORMAT_R8G8_UNORM
#define PF_RG8SN       VK_FORMAT_R8G8_SNORM
#define PF_RG8US       VK_FORMAT_R8G8_USCALED
#define PF_RG8SS       VK_FORMAT_R8G8_SSCALED
#define PF_RG8U        VK_FORMAT_R8G8_UINT
#define PF_RG8I        VK_FORMAT_R8G8_SINT
#define PF_RG8s        VK_FORMAT_R8G8_SRGB

#define PF_RGB8UN      VK_FORMAT_R8G8B8_UNORM      /* no GPU support,don't use */
#define PF_RGB8SN      VK_FORMAT_R8G8B8_SNORM      /* no GPU support,don't use */
#define PF_RGB8US      VK_FORMAT_R8G8B8_USCALED    /* no GPU support,don't use */
#define PF_RGB8SS      VK_FORMAT_R8G8B8_SSCALED    /* no GPU support,don't use */
#define PF_RGB8U       VK_FORMAT_R8G8B8_UINT       /* no GPU support,don't use */
#define PF_RGB8I       VK_FORMAT_R8G8B8_SINT       /* no GPU support,don't use */
#define PF_RGB8s       VK_FORMAT_R8G8B8_SRGB       /* no GPU support,don't use */

#define PF_BGR8UN      VK_FORMAT_B8G8R8_UNORM      /* no GPU support,don't use */
#define PF_BGR8SN      VK_FORMAT_B8G8R8_SNORM      /* no GPU support,don't use */
#define PF_BGR8US      VK_FORMAT_B8G8R8_USCALED    /* no GPU support,don't use */
#define PF_BGR8SS      VK_FORMAT_B8G8R8_SSCALED    /* no GPU support,don't use */
#define PF_BGR8U       VK_FORMAT_B8G8R8_UINT       /* no GPU support,don't use */
#define PF_BGR8I       VK_FORMAT_B8G8R8_SINT       /* no GPU support,don't use */
#define PF_BGR8s       VK_FORMAT_B8G8R8_SRGB       /* no GPU support,don't use */

#define PF_RGBA8UN     VK_FORMAT_R8G8B8A8_UNORM
#define PF_RGBA8SN     VK_FORMAT_R8G8B8A8_SNORM
#define PF_RGBA8US     VK_FORMAT_R8G8B8A8_USCALED
#define PF_RGBA8SS     VK_FORMAT_R8G8B8A8_SSCALED
#define PF_RGBA8U      VK_FORMAT_R8G8B8A8_UINT
#define PF_RGBA8I      VK_FORMAT_R8G8B8A8_SINT
#define PF_RGBA8s      VK_FORMAT_R8G8B8A8_SRGB

#define PF_BGRA8UN     VK_FORMAT_B8G8R8A8_UNORM
#define PF_BGRA8SN     VK_FORMAT_B8G8R8A8_SNORM
#define PF_BGRA8US     VK_FORMAT_B8G8R8A8_USCALED
#define PF_BGRA8SS     VK_FORMAT_B8G8R8A8_SSCALED
#define PF_BGRA8U      VK_FORMAT_B8G8R8A8_UINT
#define PF_BGRA8I      VK_FORMAT_B8G8R8A8_SINT
#define PF_BGRA8s      VK_FORMAT_B8G8R8A8_SRGB

#define PF_ABGR8UN     VK_FORMAT_A8B8G8R8_UNORM_PACK32
#define PF_ABGR8SN     VK_FORMAT_A8B8G8R8_SNORM_PACK32
#define PF_ABGR8US     VK_FORMAT_A8B8G8R8_USCALED_PACK32
#define PF_ABGR8SS     VK_FORMAT_A8B8G8R8_SSCALED_PACK32
#define PF_ABGR8U      VK_FORMAT_A8B8G8R8_UINT_PACK32
#define PF_ABGR8I      VK_FORMAT_A8B8G8R8_SINT_PACK32
#define PF_ABGR8s      VK_FORMAT_A8B8G8R8_SRGB_PACK32

#define PF_A2RGB10UN   VK_FORMAT_A2R10G10B10_UNORM_PACK32
#define PF_A2RGB10SN   VK_FORMAT_A2R10G10B10_SNORM_PACK32
#define PF_A2RGB10US   VK_FORMAT_A2R10G10B10_USCALED_PACK32
#define PF_A2RGB10SS   VK_FORMAT_A2R10G10B10_SSCALED_PACK32
#define PF_A2RGB10U    VK_FORMAT_A2R10G10B10_UINT_PACK32
#define PF_A2RGB10I    VK_FORMAT_A2R10G10B10_SINT_PACK32

#define PF_A2BGR10UN   VK_FORMAT_A2B10G10R10_UNORM_PACK32
#define PF_A2BGR10SN   VK_FORMAT_A2B10G10R10_SNORM_PACK32
#define PF_A2BGR10US   VK_FORMAT_A2B10G10R10_USCALED_PACK32
#define PF_A2BGR10SS   VK_FORMAT_A2B10G10R10_SSCALED_PACK32
#define PF_A2BGR10U    VK_FORMAT_A2B10G10R10_UINT_PACK32
#define PF_A2BGR10I    VK_FORMAT_A2B10G10R10_SINT_PACK32

#define PF_R16UN       VK_FORMAT_R16_UNORM
#define PF_R16SN       VK_FORMAT_R16_SNORM
#define PF_R16US       VK_FORMAT_R16_USCALED
#define PF_R16SS       VK_FORMAT_R16_SSCALED
#define PF_R16U        VK_FORMAT_R16_UINT
#define PF_R16I        VK_FORMAT_R16_SINT
#define PF_R16F        VK_FORMAT_R16_SFLOAT

#define PF_RG16UN      VK_FORMAT_R16G16_UNORM
#define PF_RG16SN      VK_FORMAT_R16G16_SNORM
#define PF_RG16US      VK_FORMAT_R16G16_USCALED
#define PF_RG16SS      VK_FORMAT_R16G16_SSCALED
#define PF_RG16U       VK_FORMAT_R16G16_UINT
#define PF_RG16I       VK_FORMAT_R16G16_SINT
#define PF_RG16F       VK_FORMAT_R16G16_SFLOAT

#define PF_RGB16UN     VK_FORMAT_R16G16B16_UNORM       /* no GPU support,don't use */
#define PF_RGB16SN     VK_FORMAT_R16G16B16_SNORM       /* no GPU support,don't use */
#define PF_RGB16US     VK_FORMAT_R16G16B16_USCALED     /* no GPU support,don't use */
#define PF_RGB16SS     VK_FORMAT_R16G16B16_SSCALED     /* no GPU support,don't use */
#define PF_RGB16U      VK_FORMAT_R16G16B16_UINT        /* no GPU support,don't use */
#define PF_RGB16I      VK_FORMAT_R16G16B16_SINT        /* no GPU support,don't use */
#define PF_RGB16F      VK_FORMAT_R16G16B16_SFLOAT      /* no GPU support,don't use */

#define PF_RGBA16UN    VK_FORMAT_R16G16B16A16_UNORM
#define PF_RGBA16SN    VK_FORMAT_R16G16B16A16_SNORM
#define PF_RGBA16US    VK_FORMAT_R16G16B16A16_USCALED
#define PF_RGBA16SS    VK_FORMAT_R16G16B16A16_SSCALED
#define PF_RGBA16U     VK_FORMAT_R16G16B16A16_UINT
#define PF_RGBA16I     VK_FORMAT_R16G16B16A16_SINT
#define PF_RGBA16F     VK_FORMAT_R16G16B16A16_SFLOAT

#define PF_R32U        VK_FORMAT_R32_UINT
#define PF_R32I        VK_FORMAT_R32_SINT
#define PF_R32F        VK_FORMAT_R32_SFLOAT

#define PF_RG32U       VK_FORMAT_R32G32_UINT
#define PF_RG32I       VK_FORMAT_R32G32_SINT
#define PF_RG32F       VK_FORMAT_R32G32_SFLOAT

#define PF_RGB32U      VK_FORMAT_R32G32B32_UINT
#define PF_RGB32I      VK_FORMAT_R32G32B32_SINT
#define PF_RGB32F      VK_FORMAT_R32G32B32_SFLOAT

#define PF_RGBA32U     VK_FORMAT_R32G32B32A32_UINT
#define PF_RGBA32I     VK_FORMAT_R32G32B32A32_SINT
#define PF_RGBA32F     VK_FORMAT_R32G32B32A32_SFLOAT

#define PF_R64U        VK_FORMAT_R64_UINT
#define PF_R64I        VK_FORMAT_R64_SINT
#define PF_R64F        VK_FORMAT_R64_SFLOAT

#define PF_RG64U       VK_FORMAT_R64G64_UINT
#define PF_RG64I       VK_FORMAT_R64G64_SINT
#define PF_RG64F       VK_FORMAT_R64G64_SFLOAT

#define PF_RGB64U      VK_FORMAT_R64G64B64_UINT
#define PF_RGB64I      VK_FORMAT_R64G64B64_SINT
#define PF_RGB64F      VK_FORMAT_R64G64B64_SFLOAT

#define PF_RGBA64U     VK_FORMAT_R64G64B64A64_UINT
#define PF_RGBA64I     VK_FORMAT_R64G64B64A64_SINT
#define PF_RGBA64F     VK_FORMAT_R64G64B64A64_SFLOAT

#define PF_B10GR11UF   VK_FORMAT_B10G11R11_UFLOAT_PACK32
#define PF_E5BGR9UF    VK_FORMAT_E5B9G9R9_UFLOAT_PACK32

#define PF_D16UN       VK_FORMAT_D16_UNORM
#define PF_X8_D24UN    VK_FORMAT_X8_D24_UNORM_PACK32
#define PF_D32F        VK_FORMAT_D32_SFLOAT
#define PF_S8U         VK_FORMAT_S8_UINT
#define PF_D16UN_S8U   VK_FORMAT_D16_UNORM_S8_UINT
#define PF_D24UN_S8U   VK_FORMAT_D24_UNORM_S8_UINT
#define PF_D32F_S8U    VK_FORMAT_D32_SFLOAT_S8_UINT

constexpr bool IsDepthFormat(const VkFormat fmt)
{
    return(fmt==PF_D16UN
         ||fmt==PF_X8_D24UN
         ||fmt==PF_D32F
         ||fmt==PF_D16UN_S8U
         ||fmt==PF_D24UN_S8U
         ||fmt==PF_D32F_S8U);
}

constexpr bool IsStencilFormat(const VkFormat fmt)
{
    return(fmt==PF_S8U
         ||fmt==PF_D16UN_S8U
         ||fmt==PF_D24UN_S8U
         ||fmt==PF_D32F_S8U);
}

constexpr bool IsDepthStencilFormat(const VkFormat fmt)
{
    return(fmt==PF_D16UN_S8U
         ||fmt==PF_D24UN_S8U
         ||fmt==PF_D32F_S8U);
}

/**
 *                  Type of data      bytes/pixel     Palette size       Line Segments       User For                    Compressed components/Description
 * ---------------+----------------+---------------+-----------------+-------------------+-----------------------------+--------------------------------------
 *  BC1(S3TC1)      RGB5/RGB5A1         0.5             4                 1                  Color maps,Normal maps
 *  BC2(S3TC3)      RGBA4               1               4                 1                                                 BC1+uncompress 4-bit alpha
 *  BC3(S3TC5)      RGBA4               1               4                 1 Color+1Alpha     Color maps                     BC1+BC4 compress alpha
 *  BC4(ATI1/3Dc+)  Grayscale           0.5             8                 1
 *  BC5(ATI2/3Dc)   2xGrayscale         1               8 per channel     1 per channel      Tangent-space normal maps
 *  BC6H            FloatRGB            1               8-16              1-2                HDR images
 *  BC7             RGB/RGBA            1               4-16              1-3                High-quality color maps
 *
 *  ETC             RGB
 *  ETC2            RGB/RGBA1                                                                Color maps,Normals
 *  EAC             Grayscale
 *  2xEAC           RG
 *  ETC2+EAC        RGBA
 *
 *  PVRTC           RGB/RGBA1/RGBA
 *  PVRTC2          RGB/RGBA1/RGBA
 *
 *  ASTC            R/RG/RGB/RGBA
 * ---------------+----------------+---------------+-----------------+-------------------+-----------------------------+--------------------------------------
 */

#define PF_BC1_RGBUN   VK_FORMAT_BC1_RGB_UNORM_BLOCK
#define PF_BC1_RGBs    VK_FORMAT_BC1_RGB_SRGB_BLOCK
#define PF_BC1_RGBAUN  VK_FORMAT_BC1_RGBA_UNORM_BLOCK
#define PF_BC1_RGBAs   VK_FORMAT_BC1_RGBA_SRGB_BLOCK

#define PF_BC2UN       VK_FORMAT_BC2_UNORM_BLOCK
#define PF_BC2s        VK_FORMAT_BC2_SRGB_BLOCK

#define PF_BC3UN       VK_FORMAT_BC3_UNORM_BLOCK
#define PF_BC3s        VK_FORMAT_BC3_SRGB_BLOCK

#define PF_BC4UN       VK_FORMAT_BC4_UNORM_BLOCK
#define PF_BC4SN       VK_FORMAT_BC4_SNORM_BLOCK

#define PF_BC5UN       VK_FORMAT_BC5_UNORM_BLOCK
#define PF_BC5SN       VK_FORMAT_BC5_SNORM_BLOCK

#define PF_BC6UF       VK_FORMAT_BC6H_UFLOAT_BLOCK
#define PF_BC6SF       VK_FORMAT_BC6H_SFLOAT_BLOCK

#define PF_BC7UN       VK_FORMAT_BC7_UNORM_BLOCK
#define PF_BC7s        VK_FORMAT_BC7_SRGB_BLOCK

#define PF_ETC2_RGB8UN     VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK
#define PF_ETC2_RGB8s      VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK
#define PF_ETC2_RGB8A1UN   VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK
#define PF_ETC2_RGB8A1s    VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK
#define PF_ETC2_RGBA8UN    VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK
#define PF_ETC2_RGBA8s     VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK

#define PF_EAC_R11UN       VK_FORMAT_EAC_R11_UNORM_BLOCK
#define PF_EAC_R11SN       VK_FORMAT_EAC_R11_SNORM_BLOCK
#define PF_EAC_RG11UN      VK_FORMAT_EAC_R11G11_UNORM_BLOCK
#define PF_EAC_RG11SN      VK_FORMAT_EAC_R11G11_SNORM_BLOCK

#define PF_ASTC_4x4UN      VK_FORMAT_ASTC_4x4_UNORM_BLOCK
#define PF_ASTC_4x4s       VK_FORMAT_ASTC_4x4_SRGB_BLOCK
#define PF_ASTC_5x4UN      VK_FORMAT_ASTC_5x4_UNORM_BLOCK
#define PF_ASTC_5x4s       VK_FORMAT_ASTC_5x4_SRGB_BLOCK
#define PF_ASTC_5x5UN      VK_FORMAT_ASTC_5x5_UNORM_BLOCK
#define PF_ASTC_5x5s       VK_FORMAT_ASTC_5x5_SRGB_BLOCK
#define PF_ASTC_6x5UN      VK_FORMAT_ASTC_6x5_UNORM_BLOCK
#define PF_ASTC_6x5s       VK_FORMAT_ASTC_6x5_SRGB_BLOCK
#define PF_ASTC_6x6UN      VK_FORMAT_ASTC_6x6_UNORM_BLOCK
#define PF_ASTC_6x6s       VK_FORMAT_ASTC_6x6_SRGB_BLOCK
#define PF_ASTC_8x5UN      VK_FORMAT_ASTC_8x5_UNORM_BLOCK
#define PF_ASTC_8x5s       VK_FORMAT_ASTC_8x5_SRGB_BLOCK
#define PF_ASTC_8x6UN      VK_FORMAT_ASTC_8x6_UNORM_BLOCK
#define PF_ASTC_8x6s       VK_FORMAT_ASTC_8x6_SRGB_BLOCK
#define PF_ASTC_8x8UN      VK_FORMAT_ASTC_8x8_UNORM_BLOCK
#define PF_ASTC_8x8s       VK_FORMAT_ASTC_8x8_SRGB_BLOCK
#define PF_ASTC_10x5UN     VK_FORMAT_ASTC_10x5_UNORM_BLOCK
#define PF_ASTC_10x5s      VK_FORMAT_ASTC_10x5_SRGB_BLOCK
#define PF_ASTC_10x6UN     VK_FORMAT_ASTC_10x6_UNORM_BLOCK
#define PF_ASTC_10x6s      VK_FORMAT_ASTC_10x6_SRGB_BLOCK
#define PF_ASTC_10x8UN     VK_FORMAT_ASTC_10x8_UNORM_BLOCK
#define PF_ASTC_10x8s      VK_FORMAT_ASTC_10x8_SRGB_BLOCK
#define PF_ASTC_10x10UN    VK_FORMAT_ASTC_10x10_UNORM_BLOCK
#define PF_ASTC_10x10s     VK_FORMAT_ASTC_10x10_SRGB_BLOCK
#define PF_ASTC_12x10UN    VK_FORMAT_ASTC_12x10_UNORM_BLOCK
#define PF_ASTC_12x10s     VK_FORMAT_ASTC_12x10_SRGB_BLOCK
#define PF_ASTC_12x12UN    VK_FORMAT_ASTC_12x12_UNORM_BLOCK
#define PF_ASTC_12x12s     VK_FORMAT_ASTC_12x12_SRGB_BLOCK

#define PF_BEGIN_RANGE PF_UNDEFINED
#define PF_END_RANGE   PF_ASTC_12x12s
constexpr size_t PF_RANGE_SIZE=PF_END_RANGE-PF_BEGIN_RANGE+1;

#define PF_YUYV8_422   VK_FORMAT_G8B8G8R8_422_UNORM
#define PF_UYVY8_422   VK_FORMAT_B8G8R8G8_422_UNORM
#define PF_YUV8_420    VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM
#define PF_Y8_UV8_420  VK_FORMAT_G8_B8R8_2PLANE_420_UNORM
#define PF_YUV8_422    VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM
#define PF_Y8_UV8_422  VK_FORMAT_G8_B8R8_2PLANE_422_UNORM
#define PF_YUV8_444    VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM

#define PF_R10X6UN         VK_FORMAT_R10X6_UNORM_PACK16
#define PF_RG10X6UN        VK_FORMAT_R10X6G10X6_UNORM_2PACK16
#define PF_RGBA10X6UN      VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16
#define PF_YUYV10_422      VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16
#define PF_UYVY10_422      VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16
#define PF_YUV10_420       VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16
#define PF_Y10_UV10_420    VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16
#define PF_YUV10_422       VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16
#define PF_Y10_UV10_422    VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16
#define PF_YUV10_444       VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16

#define PF_R12X4UN         VK_FORMAT_R12X4_UNORM_PACK16
#define PF_RG12X4UN        VK_FORMAT_R12X4G12X4_UNORM_2PACK16
#define PF_RGBA12X4UN      VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16
#define PF_YUYV12_422      VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16
#define PF_UYVY12_422      VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16
#define PF_YUV12_420       VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16
#define PF_Y12_UV12_420    VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16
#define PF_YUV12_422       VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16
#define PF_Y12_UV12_422    VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16
#define PF_YUV12_444       VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16

#define PF_YUYV16_422      VK_FORMAT_G16B16G16R16_422_UNORM
#define PF_UYVY16_422      VK_FORMAT_B16G16R16G16_422_UNORM
#define PF_YUV16_420       VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM
#define PF_Y16_UV16_420    VK_FORMAT_G16_B16R16_2PLANE_420_UNORM
#define PF_YUV16_422       VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM
#define PF_Y16_UV16_422    VK_FORMAT_G16_B16R16_2PLANE_422_UNORM
#define PF_YUV16_444       VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM

#define PF_YUV_BEGIN_RANGE PF_YUYV8_422
#define PF_YUV_END_RANGE   PF_YUV16_444
constexpr size_t PF_YUV_RANGE_SIZE=PF_YUV_END_RANGE-PF_YUV_BEGIN_RANGE+1;

#define PF_PVRTC1_2UN  VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG
#define PF_PVRTC1_4UN  VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG
#define PF_PVRTC2_2UN  VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG
#define PF_PVRTC2_4UN  VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG
#define PF_PVRTC1_2s   VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG
#define PF_PVRTC1_4s   VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG
#define PF_PVRTC2_2s   VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG
#define PF_PVRTC2_4s   VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG

#define PF_PVRTC_BEGIN_RANGE   PF_PVRTC1_2UN
#define PF_PVRTC_END_RANGE     PF_PVRTC2_4s
constexpr size_t PF_PVRTC_RANGE_SIZE=PF_PVRTC_END_RANGE-PF_PVRTC_BEGIN_RANGE+1;

#define PF_ASTC_4x4F   VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT
#define PF_ASTC_5x4F   VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT
#define PF_ASTC_5x5F   VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT
#define PF_ASTC_6x5F   VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT
#define PF_ASTC_6x6F   VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT
#define PF_ASTC_8x5F   VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT
#define PF_ASTC_8x6F   VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT
#define PF_ASTC_8x8F   VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT
#define PF_ASTC_10x5F  VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT
#define PF_ASTC_10x6F  VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT
#define PF_ASTC_10x8F  VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT
#define PF_ASTC_10x10F VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT
#define PF_ASTC_12x10F VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT
#define PF_ASTC_12x12F VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT

#define PF_ASTC_SFLOAT_BEGIN_RANGE PF_ASTC_4x4F
#define PF_ASTC_SFLOAT_END_RANGE   PF_ASTC_12x12F
constexpr size_t PF_ASTC_SFLOAT_RANGE_SIZE=PF_ASTC_SFLOAT_END_RANGE-PF_ASTC_SFLOAT_BEGIN_RANGE+1;

inline const bool CheckVulkanFormat(const VkFormat format)
{
    if(format> PF_BEGIN_RANGE              &&format<=PF_END_RANGE             )return(true);//BEGIN是FMT_UNDEFINED，所以这里是>而不是>=
    if(format>=PF_YUV_BEGIN_RANGE          &&format<=PF_YUV_END_RANGE         )return(true);
    if(format>=PF_PVRTC_BEGIN_RANGE        &&format<=PF_PVRTC_END_RANGE       )return(true);
    if(format>=PF_ASTC_SFLOAT_BEGIN_RANGE  &&format<=PF_ASTC_SFLOAT_END_RANGE )return(true);

    return(false);
}

//以下为AMD/NVIDIA/INTEL/QUALCOMM/ARM/POWERVR全部可用Optimal模式的格式
#define UPF_RGB565     PF_RGB565
#define UPF_A1RGB5     PF_A1RGB5
#define UPF_R8         PF_R8UN
#define UPF_RG8        PF_RG8UN
#define UPF_RGBA8      PF_RGBA8UN
#define UPF_RGBA8S     PF_RGBA8SN
#define UPF_RGBA8U     PF_RGBA8U
#define UPF_RGBA8I     PF_RGBA8I
#define UPF_ABGR8      PF_ABGR8UN
#define UPF_A2BGR10    PF_A2BGR10UN
#define UPF_R16        PF_R16UN
#define UPF_R16U       PF_R16U
#define UPF_R16I       PF_R16I
#define UPF_R16F       PF_R16F
#define UPF_RG16       PF_RG16UN
#define UPF_RG16U      PF_RG16U
#define UPF_RG16I      PF_RG16I
#define UPF_RG16F      PF_RG16F
//#define UPF_RGBA16     PF_RGBA16UN      //Adreno 500系不支持，600系可以
//#define UPF_RGBA16S    PF_RGBA16SN
#define UPF_RGBA16U    PF_RGBA16U
#define UPF_RGBA16I    PF_RGBA16I
#define UPF_RGBA16F    PF_RGBA16F
#define UPF_R32U       PF_R32U
#define UPF_R32I       PF_R32I
#define UPF_R32F       PF_R32F
#define UPF_RG32U      PF_RG32U
#define UPF_RG32I      PF_RG32I
#define UPF_RG32F      PF_RG32F
#define UPF_RGBA32U    PF_RGBA32U
#define UPF_RGBA32I    PF_RGBA32I
#define UPF_RGBA32F    PF_RGBA32F
#define UPF_B10GR11UF  PF_B10GR11UF

//顶点格式
#define VF_V1F      PF_R32F
#define VF_V2F      PF_RG32F
#define VF_V3F      PF_RGB32F
#define VF_V4F      PF_RGBA32F

#define VF_V1HF     PF_R16F
#define VF_V2HF     PF_RG16F
#define VF_V3HF     PF_RGB16F
#define VF_V4HF     PF_RGBA16F

#define VF_V1I      PF_R32I
#define VF_V2I      PF_RG32I
#define VF_V3I      PF_RGB32I
#define VF_V4I      PF_RGBA32I

#define VF_V1I16    PF_R16I
#define VF_V2I16    PF_RG16I
#define VF_V3I16    PF_RGB16I
#define VF_V4I16    PF_RGBA16I

#define VF_V1I8     PF_R8I
#define VF_V2I8     PF_RG8I
#define VF_V3I8     PF_RGB8I
#define VF_V4I8     PF_RGBA8I

#define VF_V1U      PF_R32U
#define VF_V2U      PF_RG32U
#define VF_V3U      PF_RGB32U
#define VF_V4U      PF_RGBA32U

#define VF_V1U8     PF_R8U
#define VF_V2U8     PF_RG8U
#define VF_V3U8     PF_RGB8U
#define VF_V4U8     PF_RGBA8U

#define VF_V1U16    PF_R16U
#define VF_V2U16    PF_RG16U
#define VF_V3U16    PF_RGB16U
#define VF_V4U16    PF_RGBA16U

#define VF_V1UN8    PF_R8UN
#define VF_V2UN8    PF_RG8UN
#define VF_V3UN8    PF_RGB8UN
#define VF_V4UN8    PF_RGBA8UN

#define VF_V1UN16   PF_R16UN
#define VF_V2UN16   PF_RG16UN
#define VF_V3UN16   PF_RGB16UN
#define VF_V4UN16   PF_RGBA16UN

#define VF_V1SN8    PF_R8SN
#define VF_V2SN8    PF_RG8SN
#define VF_V3SN8    PF_RGB8SN
#define VF_V4SN8    PF_RGBA8SN

#define VF_V1SN16   PF_R16SN
#define VF_V2SN16   PF_RG16SN
#define VF_V3SN16   PF_RGB16SN
#define VF_V4SN16   PF_RGBA16SN

enum class TextureCompressType
{
    NONE=0,

    S3TC,
    PVRTC,
    ETC1,
    ETC2,
    EAC,
    ATC,
    ASTC,
    YUV,
};//

enum class VulkanBaseType
{
    NONE=0,
    
    UINT,
    SINT,
    UNORM,
    SNORM,
    USCALED,
    SSCALED,
    UFLOAT,
    SFLOAT,
    SRGB,

    ENUM_CLASS_RANGE(UINT,SRGB)
};//

constexpr const char *TextureCompressTypeName[]=
{
    "NONE",
    "S3TC",
    "PVRTC",
    "ETC1",
    "ETC2",
    "EAC",
    "ATC",
    "ASTC",
    "YUV"
};

constexpr const char *VulkanBaseTypeName[]
{
    "NONE",
    "UINT",
    "SINT",
    "UNORM",
    "SNORM",
    "USCALED",
    "SSCALED",
    "UFLOAT",
    "SFLOAT",
    "SRGB"
};//

struct VulkanFormat
{
    VkFormat            format;         ///<Vulkan格式，此值保留仅为参考

    uint32_t            bytes;          ///<每象素字节数

    char                name[16];       ///<名称

    TextureCompressType compress_type;  ///<压缩类型
    VulkanBaseType      color;          ///<颜色数据类型
    VulkanBaseType      depth,stencil;  ///<是否含有depth,stencil数据
};

#ifdef _DEBUG
bool CheckStrideBytesByFormat();        ///<检验所有数据类型长度数组是否符合规则
#endif//_DEBUG

const VulkanFormat *GetVulkanFormat(const VkFormat &format);
const VulkanFormat *GetVulkanFormatList(uint32_t &);

inline uint32_t GetStrideByFormat(const VkFormat &format)
{
    const VulkanFormat *vcf=GetVulkanFormat(format);

    return (vcf?vcf->bytes:0);
}

inline uint32_t GetImageBytes(const VkFormat &format,const uint32_t pixel_total)
{
    const VulkanFormat *vcf=GetVulkanFormat(format);

    if(vcf&&vcf->bytes)
        return vcf->bytes*pixel_total;
        
    if(format>=PF_BC1_RGBUN
     &&format<=PF_BC7s)
    {
        if(format<=PF_BC1_RGBAs
         ||format==PF_BC4UN
         ||format==PF_BC4SN)return(pixel_total>>1);

        return pixel_total;
    }

    return 0;
}

inline const char *GetVulkanFormatName(const VkFormat &format)
{
    const VulkanFormat *vcf=GetVulkanFormat(format);

    return (vcf?vcf->name:nullptr);
}

const VulkanFormat *GetVulkanFormat(const char *fmt_name);

struct VulkanColorSpace
{
    VkColorSpaceKHR cs;
    char name[32];

    bool linear;
};

const VulkanColorSpace *GetVulkanColorSpace(const VkColorSpaceKHR &cs);

VK_NAMESPACE_END
