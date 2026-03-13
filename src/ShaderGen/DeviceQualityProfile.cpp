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

    // ---- Platform 判断 ----
    // 简化版：仅区分 PC vs 移动端；Apple 需额外端外信息，暂归 PC
    profile.platform = PlatformBackend::PC;

    // ---- 基础能力检测 ----
    profile.support_compute         = true;   // Vulkan 1.0 保证 compute queue
    profile.support_indirect_draw   = feat10.multiDrawIndirect == VK_TRUE;
    profile.support_ssbo_vertex     = (limits.maxStorageBufferRange >= 128 * 1024 * 1024);  // 128 MB
    profile.support_meshlet         = false;  // 需要 VK_EXT_mesh_shader，后续检测扩展

    // HZB 需要 compute + image load/store
    profile.support_hzb = profile.support_compute;

    // VBuffer 需要 SSBO + compute + indirect draw
    profile.support_vbuffer = profile.support_ssbo_vertex
                           && profile.support_compute
                           && profile.support_indirect_draw;

    // Clustered Shading 需要 compute
    profile.support_clustered = profile.support_compute;

    // D32_SFLOAT 支持 — 由调用方通过 vkGetPhysicalDeviceFormatProperties 检查后覆盖
    // 此处默认 true (大多数桌面 GPU 支持)
    profile.support_d32_sfloat = true;

    // ---- 纹理/阴影参数 ----
    profile.max_texture_size    = limits.maxImageDimension2D;
    profile.max_shadow_cascade  = 4;
    profile.max_point_lights    = 64;

    // ---- QualityTier 推断 ----
    const bool is_discrete = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
    const uint32_t ssbo_range = limits.maxStorageBufferRange;

    if(is_discrete && ssbo_range >= 1024u * 1024 * 1024)           // 1 GB SSBO range
    {
        profile.quality_tier = QualityTier::Ultra;
        profile.render_scale = 1.0f;
    }
    else if(is_discrete && ssbo_range >= 128u * 1024 * 1024)       // 128 MB
    {
        profile.quality_tier = QualityTier::High;
        profile.render_scale = 1.0f;
    }
    else if(ssbo_range >= 64u * 1024 * 1024)                       // 64 MB (集成 GPU 等)
    {
        profile.quality_tier = QualityTier::Medium;
        profile.render_scale = 0.75f;
        profile.max_shadow_cascade = 2;
        profile.max_point_lights   = 32;
    }
    else
    {
        profile.quality_tier = QualityTier::Low;
        profile.render_scale = 0.5f;
        profile.max_shadow_cascade = 1;
        profile.max_point_lights   = 16;
        profile.support_hzb        = false;
        profile.support_vbuffer    = false;
        profile.support_clustered  = false;
    }

    // ---- GeometryFetchMode ----
    profile.geometry_fetch = profile.support_ssbo_vertex ? GeometryFetchMode::SSBO
                                                        : GeometryFetchMode::VBO;

    return profile;
}

}//namespace hgl::graph
