#pragma once

#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/type/ArrayList.h>

VK_NAMESPACE_BEGIN

constexpr const uint32_t ERROR_FAMILY_INDEX=UINT32_MAX;

using VkSurfaceFormatList=ArrayList<VkSurfaceFormatKHR>;
using VkPresentModeList=ArrayList<VkPresentModeKHR>;

class VulkanSurface
{
    const VulkanPhyDevice *         physical_device=nullptr;
    VkSurfaceKHR                    surface=VK_NULL_HANDLE;

    VkSurfaceCapabilitiesKHR        capabilities; //当前表面支持的能力

    VkPresentModeList               present_mode_list;  //当前表面支持的呈现模式列表
    VkSurfaceFormatList             surface_format_list; //当前表面支持的格式列表

    VkSurfaceTransformFlagBitsKHR   preTransform    =VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR     compositeAlpha  =VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    VkBool32 *                      supports_present=nullptr;

    uint32_t graphics_family     =ERROR_FAMILY_INDEX;
    uint32_t compute_family      =ERROR_FAMILY_INDEX;
    uint32_t transfer_family     =ERROR_FAMILY_INDEX;

    uint32_t video_decode_family =ERROR_FAMILY_INDEX;
    uint32_t video_encode_family =ERROR_FAMILY_INDEX;

public:

    VulkanSurface(const VulkanPhyDevice *pd,VkSurfaceKHR surface);
    virtual ~VulkanSurface();

    const VkInstance        GetInstance         ()const{return physical_device->GetVulkanInstance();}
    const VkPhysicalDevice  GetPhysicalDevice   ()const{return physical_device->GetVulkanDevice();}
    const VkSurfaceKHR      GetSurface          ()const{return surface;}

    const VkSurfaceTransformFlagBitsKHR GetPreTransform()const{return preTransform;}
    const VkCompositeAlphaFlagBitsKHR   GetCompositeAlpha()const{return compositeAlpha;}

    const uint32_t GetGraphicsFamilyIndex()const { return graphics_family; }       //图形队列族索引
    const uint32_t GetComputeFamilyIndex()const { return compute_family; }         //计算队列族索引
    const uint32_t GetTransferFamilyIndex()const { return transfer_family; }       //传输队列族索引
    const uint32_t GetVideoDecodeFamilyIndex()const { return video_decode_family; } //视频解码队列族索引
    const uint32_t GetVideoEncodeFamilyIndex()const { return video_encode_family; } //视频编码队列族索引

public: //Vulkan API

    const VkSurfaceCapabilitiesKHR &GetCapabilities  ()const{return capabilities;}
    const VkPresentModeList &       GetPresentModes  ()const{return present_mode_list;}
    const VkSurfaceFormatList &     GetFormats       ()const{return surface_format_list;}

public:

    void RefreshCaps();
};//class VulkanSurface

VK_NAMESPACE_END
