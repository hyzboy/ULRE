#pragma once

#include<hgl/graph/BufferPolicy.h>

VK_NAMESPACE_BEGIN

class VulkanPhyDevice;  // Forward declaration

// GPU架构类型
enum class GPUArchType
{
    UnifiedMemory,      // 统一内存架构 (AMD APU, Intel iGPU, Apple M系列)
    DiscreteSmallReBAR, // 独立显卡 - 小/无ReBAR (老显卡)
    DiscreteLargeReBAR  // 独立显卡 - 超大ReBAR (新显卡)
};

// 设备属性相关的策略调整因子
struct DevicePolicyAdjustment
{
    GPUArchType         arch_type;              // GPU架构类型
    bool                is_amd_apu;             // 是否是AMD Ryzen APU
    bool                is_intel_igpu;          // 是否是Intel iGPU
    bool                is_apple_m_series;      // 是否是Apple M系列
    bool                has_rebar;              // 是否支持ReBAR
    VkDeviceSize        rebar_size;             // ReBAR可用容量
    VkDeviceSize        available_device_mem;   // 总设备内存

    DevicePolicyAdjustment()
        : arch_type(GPUArchType::DiscreteSmallReBAR),
          is_amd_apu(false), is_intel_igpu(false), is_apple_m_series(false),
          has_rebar(false), rebar_size(0), available_device_mem(0) {}
};

// 从物理设备获取策略调整参数
DevicePolicyAdjustment GetDevicePolicyAdjustment(const VulkanPhyDevice *phy_device);

// 预设策略生成函数 - 基础版本
BufferPolicy MakeCameraUBOPolicy();
BufferPolicy MakeStaticTransformPolicy();
BufferPolicy MakeMeshVABPolicy();
BufferPolicy MakeDynamicMeshVABPolicy();
BufferPolicy MakeTextureTilePolicy();
BufferPolicy MakeParticlePolicy();
BufferPolicy MakeDeferredPolicy();
BufferPolicy MakeManualPolicy();

// 设备自适应策略生成函数
BufferPolicy MakeCameraUBOPolicy(const DevicePolicyAdjustment &adjustment);
BufferPolicy MakeStaticTransformPolicy(const DevicePolicyAdjustment &adjustment);
BufferPolicy MakeMeshVABPolicy(const DevicePolicyAdjustment &adjustment);
BufferPolicy MakeDynamicMeshVABPolicy(const DevicePolicyAdjustment &adjustment);
BufferPolicy MakeTextureTilePolicy(const DevicePolicyAdjustment &adjustment);
BufferPolicy MakeParticlePolicy(const DevicePolicyAdjustment &adjustment);
BufferPolicy MakeDeferredPolicy(const DevicePolicyAdjustment &adjustment);
BufferPolicy MakeManualPolicy(const DevicePolicyAdjustment &adjustment);

// 策略应用函数
const BufferPolicy *GetPolicyForUpdateClass(BufferUpdateClass update_class);
const BufferPolicy *GetPolicyForUpdateClass(BufferUpdateClass update_class, const DevicePolicyAdjustment &adjustment);
void ApplyPolicy(DeviceBuffer *buf, const BufferPolicy *policy);

// 所有设备缓冲策略的统一结构体
struct AllDeviceBufferPolicies
{
    BufferPolicy camera_ubo;            // CriticalPerFrame - 相机/常数buffer
    BufferPolicy static_transform;      // TransformData - 静态变换数据
    BufferPolicy mesh_static;           // MeshStatic - 静态网格
    BufferPolicy mesh_dynamic;          // MeshDynamic - 动态网格
    BufferPolicy texture_tile;          // TextureTile - 瓦片/流式纹理
    BufferPolicy particle;              // Particle - 粒子数据
    BufferPolicy deferred;              // Deferred - 可延迟数据
    BufferPolicy manual;                // Manual - 手动控制

    // 根据更新类型获取对应的策略
    const BufferPolicy *GetPolicy(BufferUpdateClass update_class) const
    {
        switch(update_class)
        {
            case BufferUpdateClass::CriticalPerFrame:   return &camera_ubo;
            case BufferUpdateClass::TransformData:      return &static_transform;
            case BufferUpdateClass::MeshStatic:         return &mesh_static;
            case BufferUpdateClass::MeshDynamic:        return &mesh_dynamic;
            case BufferUpdateClass::TextureTile:        return &texture_tile;
            case BufferUpdateClass::Particle:           return &particle;
            case BufferUpdateClass::Deferred:           return &deferred;
            case BufferUpdateClass::Manual:             return &manual;
            default:                                    return &manual;
        }
    }
};

// 从物理设备生成完整的缓冲策略集
AllDeviceBufferPolicies GenerateAllDeviceBufferPolicies(const VulkanPhyDevice *phy_device);

VK_NAMESPACE_END
