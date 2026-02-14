#pragma once

#include<hgl/vk/BufferPolicy.h>

VK_NAMESPACE_BEGIN

class VulkanPhyDevice;  // Forward declaration

// GPUæ¶æç±»å
enum class GPUArchType
{
    UnifiedMemory,      // ç»ä¸åå­æ¶æ (AMD APU, Intel iGPU, Apple Mç³»å)
    DiscreteSmallReBAR, // ç¬ç«æ¾å¡ - å°?æ ReBAR (èæ¾å?
    DiscreteLargeReBAR  // ç¬ç«æ¾å¡ - è¶å¤§ReBAR (æ°æ¾å?
};

// è®¾å¤å±æ§ç¸å³çç­ç¥è°æ´å å­
struct DevicePolicyAdjustment
{
    GPUArchType         arch_type;              // GPUæ¶æç±»å
    bool                is_amd_apu;             // æ¯å¦æ¯AMD Ryzen APU
    bool                is_intel_igpu;          // æ¯å¦æ¯Intel iGPU
    bool                is_apple_m_series;      // æ¯å¦æ¯Apple Mç³»å
    bool                has_rebar;              // æ¯å¦æ¯æReBAR
    VkDeviceSize        rebar_size;             // ReBARå¯ç¨å®¹é
    VkDeviceSize        available_device_mem;   // æ»è®¾å¤åå­?

    DevicePolicyAdjustment()
        : arch_type(GPUArchType::DiscreteSmallReBAR),
          is_amd_apu(false), is_intel_igpu(false), is_apple_m_series(false),
          has_rebar(false), rebar_size(0), available_device_mem(0) {}
};

// ä»ç©çè®¾å¤è·åç­ç¥è°æ´åæ?
DevicePolicyAdjustment GetDevicePolicyAdjustment(const VulkanPhyDevice *phy_device);

// é¢è®¾ç­ç¥çæå½æ° - åºç¡çæ¬
BufferPolicy MakeCameraUBOPolicy();
BufferPolicy MakeStaticTransformPolicy();
BufferPolicy MakeMeshVABPolicy();
BufferPolicy MakeDynamicMeshVABPolicy();
BufferPolicy MakeTextureTilePolicy();
BufferPolicy MakeParticlePolicy();
BufferPolicy MakeDeferredPolicy();
BufferPolicy MakeManualPolicy();

// è®¾å¤èªéåºç­ç¥çæå½æ°
BufferPolicy MakeCameraUBOPolicy(const DevicePolicyAdjustment &adjustment);
BufferPolicy MakeStaticTransformPolicy(const DevicePolicyAdjustment &adjustment);
BufferPolicy MakeMeshVABPolicy(const DevicePolicyAdjustment &adjustment);
BufferPolicy MakeDynamicMeshVABPolicy(const DevicePolicyAdjustment &adjustment);
BufferPolicy MakeTextureTilePolicy(const DevicePolicyAdjustment &adjustment);
BufferPolicy MakeParticlePolicy(const DevicePolicyAdjustment &adjustment);
BufferPolicy MakeDeferredPolicy(const DevicePolicyAdjustment &adjustment);
BufferPolicy MakeManualPolicy(const DevicePolicyAdjustment &adjustment);

// ç­ç¥åºç¨å½æ°
const BufferPolicy *GetPolicyForUpdateClass(BufferUpdateClass update_class);
const BufferPolicy *GetPolicyForUpdateClass(BufferUpdateClass update_class, const DevicePolicyAdjustment &adjustment);
void ApplyPolicy(DeviceBuffer *buf, const BufferPolicy *policy);

// ææè®¾å¤ç¼å²ç­ç¥çç»ä¸ç»æä½?
struct AllDeviceBufferPolicies
{
    BufferPolicy camera_ubo;            // CriticalPerFrame - ç¸æº/å¸¸æ°buffer
    BufferPolicy static_transform;      // TransformData - éæåæ¢æ°æ?
    BufferPolicy mesh_static;           // MeshStatic - éæç½æ ?
    BufferPolicy mesh_dynamic;          // MeshDynamic - å¨æç½æ ?
    BufferPolicy texture_tile;          // TextureTile - ç¦ç/æµå¼çº¹ç
    BufferPolicy particle;              // Particle - ç²å­æ°æ®
    BufferPolicy deferred;              // Deferred - å¯å»¶è¿æ°æ?
    BufferPolicy manual;                // Manual - æå¨æ§å¶

    // æ ¹æ®æ´æ°ç±»åè·åå¯¹åºçç­ç?
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

// ä»ç©çè®¾å¤çæå®æ´çç¼å²ç­ç¥é?
AllDeviceBufferPolicies GenerateAllDeviceBufferPolicies(const VulkanPhyDevice *phy_device);

VK_NAMESPACE_END
