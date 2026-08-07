/// DeviceQualityProfile.cpp — 根据 VulkanPhyDevice 自动检测设备质量档位

#include<hgl/mtl/new/DeviceQualityProfile.h>
#include<hgl/vk/VKPhysicalDevice.h>

namespace hgl::graph{

DeviceQualityProfile DetectDeviceQuality(const VulkanPhyDevice &phy_device)
{
    DeviceQualityProfile profile;

    const auto &props   = phy_device.GetProperties();
    const auto &limits  = props.limits;
    const auto &feat10  = phy_device.GetFeatures10();

    // ---- 基础能力检测 ----
    profile.support_compute         = true;   // Vulkan 1.0 保证 compute queue
    profile.support_indirect_draw   = feat10.multiDrawIndirect == VK_TRUE;
    profile.support_meshlet         = false;  // 需要 VK_EXT_mesh_shader，后续检测扩展

    // HZB 需要 compute + image load/store
    profile.support_hzb = profile.support_compute;

    // Clustered Shading 需要 compute
    profile.support_clustered = profile.support_compute;

    // D32_SFLOAT 支持 — 由调用方通过 vkGetPhysicalDeviceFormatProperties 检查后覆盖
    // 此处默认 true (大多数桌面 GPU 支持)
    profile.support_d32_sfloat = true;

    // ---- 纹理/阴影参数 ----
    profile.max_texture_size    = limits.maxImageDimension2D;
    profile.max_shadow_cascade  = 4;
    profile.max_point_lights    = 64;

    return profile;
}

}//namespace hgl::graph
