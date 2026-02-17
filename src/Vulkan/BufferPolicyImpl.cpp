#include<hgl/vk/BufferPolicyImpl.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<cstring>

namespace hgl::graph{

DevicePolicyAdjustment GetDevicePolicyAdjustment(const VulkanPhyDevice *phy_device)
{
    DevicePolicyAdjustment adj;

    if(!phy_device)
        return adj;

    const char *device_name = phy_device->GetDeviceName();

    // è¯å«ç»ä¸åå­æ¶æ
    if(device_name)
    {
        std::string name_lower(device_name);
        // ç®åè½¬å°åä»¥ä¾¿å¹é
        for(auto &c : name_lower) c = std::tolower(c);

        // AMD Ryzen APU
        if(name_lower.find("ryzen") != std::string::npos &&
           (name_lower.find("apu") != std::string::npos || name_lower.find("radeon") != std::string::npos))
        {
            adj.arch_type = GPUArchType::UnifiedMemory;
            adj.is_amd_apu = true;
        }
        // Intel iGPU
        else if(name_lower.find("intel") != std::string::npos &&
                name_lower.find("iris") != std::string::npos)
        {
            adj.arch_type = GPUArchType::UnifiedMemory;
            adj.is_intel_igpu = true;
        }
        // Apple Mç³»å (éå¸¸æ¾ç¤ºä¸?"Apple M1", "Apple M2" ç­?
        else if(name_lower.find("apple") != std::string::npos)
        {
            adj.arch_type = GPUArchType::UnifiedMemory;
            adj.is_apple_m_series = true;
        }
    }

    // æ£æµReBARæ¯æ
    adj.has_rebar = phy_device->HasReBAR();
    adj.rebar_size = phy_device->GetReBarSize();

    // å¯¹äºç¬ç«æ¾å¡ï¼æ ¹æ®ReBARå¤§å°åç±»
    if(adj.arch_type == GPUArchType::DiscreteSmallReBAR)
    {
        if(adj.has_rebar && adj.rebar_size > 512ull * 1024 * 1024)  // > 512MB
        {
            adj.arch_type = GPUArchType::DiscreteLargeReBAR;
        }
    }

    // è·åæ»æ¾å­å®¹éï¼åæå¤§çDEVICE_LOCALå ï¼
    const VkPhysicalDeviceMemoryProperties &mem_props = phy_device->GetMemoryProperties();
    adj.available_device_mem = 0;

    for(uint32_t i = 0; i < mem_props.memoryHeapCount; i++)
    {
        if(mem_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        {
            adj.available_device_mem = std::max(adj.available_device_mem, mem_props.memoryHeaps[i].size);
        }
    }

    return adj;
}

BufferPolicy MakeCameraUBOPolicy()
{
    BufferPolicy p;
    p.priority = BufferPriority::CRITICAL;
    p.updateRate = BufferUpdateRate::PER_FRAME;
    p.submitTiming = BufferSubmitTiming::SAME_FRAME;
    p.maxLatency = 0;
    p.budgetGroup = "GLOBAL";
    p.budgetLimit = 0;
    p.queueing = true;
    p.splitPolicy = BufferSplitPolicy::NO_SPLIT;
    p.splitChunk = 0;
    p.dropPolicy = BufferDropPolicy::NEVER;
    p.deadlinePolicy = BufferDeadlinePolicy::HARD;
    p.deadline = 0;
    p.promotePolicy = BufferPromotePolicy::FORCE_HIGH;
    p.promoteRule = "always";
    p.memoryPolicy = BufferMemoryPolicy::REBAR;
    p.cpuResident = BufferCpuResident::KEEP;
    p.ringFrameCount = 0;
    p.stagedPersist = BufferCpuResident::AUTO;
    p.commitPolicy = BufferCommitPolicy::Always;
    p.devNotes = "Camera matrix updates every frame; highest priority; prefer ReBAR";
    return p;
}

BufferPolicy MakeStaticTransformPolicy()
{
    BufferPolicy p;
    p.priority = BufferPriority::HIGH;
    p.updateRate = BufferUpdateRate::BURST;
    p.submitTiming = BufferSubmitTiming::SAME_FRAME;
    p.maxLatency = 1;
    p.budgetGroup = "TRANSFORM";
    p.budgetLimit = 32ull * 1024 * 1024;
    p.queueing = true;
    p.splitPolicy = BufferSplitPolicy::NO_SPLIT;
    p.splitChunk = 0;
    p.dropPolicy = BufferDropPolicy::NEVER;
    p.deadlinePolicy = BufferDeadlinePolicy::SOFT;
    p.deadline = 2;
    p.promotePolicy = BufferPromotePolicy::AUTO_RAISE;
    p.promoteRule = "latency>1f";
    p.memoryPolicy = BufferMemoryPolicy::RING;
    p.cpuResident = BufferCpuResident::KEEP;
    p.ringFrameCount = 3;
    p.stagedPersist = BufferCpuResident::AUTO;
    p.commitPolicy = BufferCommitPolicy::Always;
    p.devNotes = "Transform ID/matrix buffer; burst update then stable; ring buffer";
    return p;
}

BufferPolicy MakeMeshVABPolicy()
{
    BufferPolicy p;
    p.priority = BufferPriority::LOW;
    p.updateRate = BufferUpdateRate::RARE;
    p.submitTiming = BufferSubmitTiming::DEFERRED;
    p.maxLatency = 4;
    p.budgetGroup = "MESH";
    p.budgetLimit = 64ull * 1024 * 1024;
    p.queueing = true;
    p.splitPolicy = BufferSplitPolicy::NO_SPLIT;
    p.splitChunk = 0;
    p.dropPolicy = BufferDropPolicy::NEVER;
    p.deadlinePolicy = BufferDeadlinePolicy::NONE;
    p.deadline = 0;
    p.promotePolicy = BufferPromotePolicy::NONE;
    p.memoryPolicy = BufferMemoryPolicy::STAGED;
    p.cpuResident = BufferCpuResident::RELEASE;
    p.ringFrameCount = 0;
    p.stagedPersist = BufferCpuResident::RELEASE;
    p.commitPolicy = BufferCommitPolicy::StagedOnly;
    p.devNotes = "Static mesh VAB; one-time or rare uploads; CPU can release";
    return p;
}

BufferPolicy MakeDynamicMeshVABPolicy()
{
    BufferPolicy p;
    p.priority = BufferPriority::NORMAL;
    p.updateRate = BufferUpdateRate::FREQUENT;
    p.submitTiming = BufferSubmitTiming::NEXT_FRAME_OK;
    p.maxLatency = 1;
    p.budgetGroup = "MESH";
    p.budgetLimit = 64ull * 1024 * 1024;
    p.queueing = true;
    p.splitPolicy = BufferSplitPolicy::NO_SPLIT;
    p.splitChunk = 0;
    p.dropPolicy = BufferDropPolicy::DROP_OLD;
    p.deadlinePolicy = BufferDeadlinePolicy::SOFT;
    p.deadline = 2;
    p.promotePolicy = BufferPromotePolicy::AUTO_RAISE;
    p.promoteRule = "latency>1f";
    p.memoryPolicy = BufferMemoryPolicy::RING;
    p.cpuResident = BufferCpuResident::KEEP;
    p.ringFrameCount = 3;
    p.stagedPersist = BufferCpuResident::AUTO;
    p.commitPolicy = BufferCommitPolicy::StagedOnly;
    p.devNotes = "Dynamic mesh VAB; frequent but scattered; ring buffer; soft deadline";
    return p;
}

BufferPolicy MakeTextureTilePolicy()
{
    BufferPolicy p;
    p.priority = BufferPriority::LOW;
    p.updateRate = BufferUpdateRate::BURST;
    p.submitTiming = BufferSubmitTiming::NEXT_FRAME_OK;
    p.maxLatency = 2;
    p.budgetGroup = "TILE";
    p.budgetLimit = 16ull * 1024 * 1024;
    p.queueing = true;
    p.splitPolicy = BufferSplitPolicy::ALLOW_SPLIT;
    p.splitChunk = 1ull * 1024 * 1024;
    p.dropPolicy = BufferDropPolicy::DROP_OLD;
    p.deadlinePolicy = BufferDeadlinePolicy::SOFT;
    p.deadline = 4;
    p.promotePolicy = BufferPromotePolicy::AUTO_RAISE;
    p.promoteRule = "latency>2f";
    p.memoryPolicy = BufferMemoryPolicy::STAGED;
    p.cpuResident = BufferCpuResident::KEEP;
    p.ringFrameCount = 0;
    p.stagedPersist = BufferCpuResident::KEEP;
    p.commitPolicy = BufferCommitPolicy::StagedOnly;
    p.devNotes = "Texture tiles; split-friendly; drop old if overload; can defer";
    return p;
}

BufferPolicy MakeParticlePolicy()
{
    BufferPolicy p;
    p.priority = BufferPriority::LOW;
    p.updateRate = BufferUpdateRate::FREQUENT;
    p.submitTiming = BufferSubmitTiming::NEXT_FRAME_OK;
    p.maxLatency = 2;
    p.budgetGroup = "PARTICLE";
    p.budgetLimit = 16ull * 1024 * 1024;
    p.queueing = true;
    p.splitPolicy = BufferSplitPolicy::ALLOW_SPLIT;
    p.splitChunk = 256ull * 1024;
    p.dropPolicy = BufferDropPolicy::DROP_OLD;
    p.deadlinePolicy = BufferDeadlinePolicy::SOFT;
    p.deadline = 4;
    p.promotePolicy = BufferPromotePolicy::AUTO_RAISE;
    p.promoteRule = "latency>2f";
    p.memoryPolicy = BufferMemoryPolicy::STAGED;
    p.cpuResident = BufferCpuResident::KEEP;
    p.ringFrameCount = 0;
    p.stagedPersist = BufferCpuResident::KEEP;
    p.commitPolicy = BufferCommitPolicy::StagedOnly;
    p.devNotes = "Particle data; allow split; drop old if over budget; soft deadline";
    return p;
}

BufferPolicy MakeDeferredPolicy()
{
    BufferPolicy p;
    p.priority = BufferPriority::LOW;
    p.updateRate = BufferUpdateRate::SPARSE;
    p.submitTiming = BufferSubmitTiming::DEFERRED;
    p.maxLatency = 4;
    p.budgetGroup = "CUSTOM";
    p.budgetLimit = 0;
    p.queueing = true;
    p.splitPolicy = BufferSplitPolicy::NO_SPLIT;
    p.splitChunk = 0;
    p.dropPolicy = BufferDropPolicy::DROP_OLD;
    p.deadlinePolicy = BufferDeadlinePolicy::NONE;
    p.deadline = 0;
    p.promotePolicy = BufferPromotePolicy::NONE;
    p.memoryPolicy = BufferMemoryPolicy::STAGED;
    p.cpuResident = BufferCpuResident::RELEASE;
    p.ringFrameCount = 0;
    p.stagedPersist = BufferCpuResident::RELEASE;
    p.commitPolicy = BufferCommitPolicy::StagedOnly;
    p.devNotes = "Deferred updates; no hard deadline; can drop; flexible timing";
    return p;
}

BufferPolicy MakeManualPolicy()
{
    BufferPolicy p;
    p.priority = BufferPriority::LOW;
    p.updateRate = BufferUpdateRate::RARE;
    p.submitTiming = BufferSubmitTiming::DEFERRED;
    p.maxLatency = 0;
    p.budgetGroup = "CUSTOM";
    p.budgetLimit = 0;
    p.queueing = false;
    p.splitPolicy = BufferSplitPolicy::NO_SPLIT;
    p.splitChunk = 0;
    p.dropPolicy = BufferDropPolicy::NEVER;
    p.deadlinePolicy = BufferDeadlinePolicy::NONE;
    p.deadline = 0;
    p.promotePolicy = BufferPromotePolicy::NONE;
    p.memoryPolicy = BufferMemoryPolicy::AUTO;
    p.cpuResident = BufferCpuResident::AUTO;
    p.ringFrameCount = 0;
    p.stagedPersist = BufferCpuResident::AUTO;
    p.commitPolicy = BufferCommitPolicy::Manual;
    p.devNotes = "Manual only; no auto-commit; full application control";
    return p;
}

const BufferPolicy *GetPolicyForUpdateClass(BufferUpdateClass update_class)
{
    static const BufferPolicy kCameraUBO = MakeCameraUBOPolicy();
    static const BufferPolicy kStaticTransform = MakeStaticTransformPolicy();
    static const BufferPolicy kMeshVAB = MakeMeshVABPolicy();
    static const BufferPolicy kDynamicMeshVAB = MakeDynamicMeshVABPolicy();
    static const BufferPolicy kTextureTile = MakeTextureTilePolicy();
    static const BufferPolicy kParticle = MakeParticlePolicy();
    static const BufferPolicy kDeferred = MakeDeferredPolicy();
    static const BufferPolicy kManual = MakeManualPolicy();

    switch(update_class)
    {
        case BufferUpdateClass::CriticalPerFrame:  return &kCameraUBO;
        case BufferUpdateClass::TransformData:     return &kStaticTransform;
        case BufferUpdateClass::MeshStatic:        return &kMeshVAB;
        case BufferUpdateClass::MeshDynamic:       return &kDynamicMeshVAB;
        case BufferUpdateClass::TextureTile:       return &kTextureTile;
        case BufferUpdateClass::Particle:          return &kParticle;
        case BufferUpdateClass::Deferred:          return &kDeferred;
        case BufferUpdateClass::Manual:            return &kManual;
        default:                                   return nullptr;
    }
}

// ============================================================================
// è®¾å¤èªéåºç­ç¥çæå½æ°
// ============================================================================

BufferPolicy MakeCameraUBOPolicy(const DevicePolicyAdjustment &adjustment)
{
    BufferPolicy p = MakeCameraUBOPolicy();

    // ç»ä¸åå­æ¶æ - å¨ç¨ä½¿ç¨ReBAR
    if(adjustment.arch_type == GPUArchType::UnifiedMemory)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.priority = BufferPriority::CRITICAL;
        p.commitPolicy = BufferCommitPolicy::Always;
        return p;
    }

    // ç¬ç«æ¾å¡ - å¤§ReBARå¯ä»¥ä½¿ç¨ReBAR
    if(adjustment.arch_type == GPUArchType::DiscreteLargeReBAR)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
    }
    // ç¬ç«æ¾å¡ - å°ReBARææ ReBAR - éçº§å°RING
    else
    {
        p.memoryPolicy = adjustment.has_rebar ? BufferMemoryPolicy::REBAR : BufferMemoryPolicy::RING;
        p.ringFrameCount = 3;
    }

    return p;
}

BufferPolicy MakeStaticTransformPolicy(const DevicePolicyAdjustment &adjustment)
{
    BufferPolicy p = MakeStaticTransformPolicy();

    // ç»ä¸åå­æ¶æ - å¨ç¨ä½¿ç¨ReBARï¼å¯ä»¥æ´æ¿è¿?
    if(adjustment.arch_type == GPUArchType::UnifiedMemory)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 64ull * 1024 * 1024;  // å¢å¤§é¢ç®
        p.commitPolicy = BufferCommitPolicy::Always;
        return p;
    }

    // ç¬ç«æ¾å¡ - å¤§ReBAR
    if(adjustment.arch_type == GPUArchType::DiscreteLargeReBAR)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 64ull * 1024 * 1024;  // è¾å¤§é¢ç®
    }
    // ç¬ç«æ¾å¡ - å°ReBARææ ReBAR
    else
    {
        p.budgetLimit = 16ull * 1024 * 1024;  // ç¼©åé¢ç®
        p.memoryPolicy = BufferMemoryPolicy::RING;
        p.ringFrameCount = 3;
    }

    return p;
}

BufferPolicy MakeMeshVABPolicy(const DevicePolicyAdjustment &adjustment)
{
    BufferPolicy p = MakeMeshVABPolicy();

    // ç»ä¸åå­æ¶æ - å¨ç¨ä½¿ç¨ReBARï¼é¢ç®è¾å®½æ¾
    if(adjustment.arch_type == GPUArchType::UnifiedMemory)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 256ull * 1024 * 1024;  // è¾å®½æ¾çé¢ç®
        return p;
    }

    // ç¬ç«æ¾å¡ - å¤§ReBAR
    if(adjustment.arch_type == GPUArchType::DiscreteLargeReBAR)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 128ull * 1024 * 1024;  // è¾å¤§é¢ç®
    }
    // ç¬ç«æ¾å¡ - å°ReBARææ ReBAR
    else
    {
        p.budgetLimit = 32ull * 1024 * 1024;   // åéçé¢ç®?
        p.memoryPolicy = BufferMemoryPolicy::STAGED;
    }

    return p;
}

BufferPolicy MakeDynamicMeshVABPolicy(const DevicePolicyAdjustment &adjustment)
{
    BufferPolicy p = MakeDynamicMeshVABPolicy();

    // ç»ä¸åå­æ¶æ - å¨ç¨ä½¿ç¨ReBAR
    if(adjustment.arch_type == GPUArchType::UnifiedMemory)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 128ull * 1024 * 1024;
        return p;
    }

    // ç¬ç«æ¾å¡ - å¤§ReBAR
    if(adjustment.arch_type == GPUArchType::DiscreteLargeReBAR)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 128ull * 1024 * 1024;
    }
    // ç¬ç«æ¾å¡ - å°ReBARææ ReBAR
    else
    {
        p.budgetLimit = 32ull * 1024 * 1024;
        p.memoryPolicy = BufferMemoryPolicy::STAGED;
        p.deadline = 3;
    }

    return p;
}

BufferPolicy MakeTextureTilePolicy(const DevicePolicyAdjustment &adjustment)
{
    BufferPolicy p = MakeTextureTilePolicy();

    // ç»ä¸åå­æ¶æ - å¨ç¨ä½¿ç¨ReBARï¼å¯ä»¥æ´æ¿è¿?
    if(adjustment.arch_type == GPUArchType::UnifiedMemory)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 64ull * 1024 * 1024;
        p.splitChunk = 4ull * 1024 * 1024;  // å¯ä»¥ç¨æ´å¤§çåå²å?
        return p;
    }

    // ç¬ç«æ¾å¡ - å¤§ReBAR
    if(adjustment.arch_type == GPUArchType::DiscreteLargeReBAR)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 32ull * 1024 * 1024;
        p.splitChunk = 2ull * 1024 * 1024;
    }
    // ç¬ç«æ¾å¡ - å°ReBARææ ReBAR
    else
    {
        p.budgetLimit = 8ull * 1024 * 1024;
        p.splitChunk = 512ull * 1024;  // è¾å°çåå²å
        p.dropPolicy = BufferDropPolicy::DROP_OLD;
    }

    return p;
}

BufferPolicy MakeParticlePolicy(const DevicePolicyAdjustment &adjustment)
{
    BufferPolicy p = MakeParticlePolicy();

    // ç»ä¸åå­æ¶æ - å¨ç¨ä½¿ç¨ReBAR
    if(adjustment.arch_type == GPUArchType::UnifiedMemory)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 32ull * 1024 * 1024;
        p.splitChunk = 512ull * 1024;  // è¾å¤§çåå²å
        return p;
    }

    // ç¬ç«æ¾å¡ - å¤§ReBAR
    if(adjustment.arch_type == GPUArchType::DiscreteLargeReBAR)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 32ull * 1024 * 1024;
    }
    // ç¬ç«æ¾å¡ - å°ReBARææ ReBAR
    else
    {
        p.budgetLimit = 8ull * 1024 * 1024;
        p.splitChunk = 128ull * 1024;  // è¾å°ã®åå²å
        p.deadline = 4;
    }

    return p;
}

BufferPolicy MakeDeferredPolicy(const DevicePolicyAdjustment &adjustment)
{
    BufferPolicy p = MakeDeferredPolicy();

    // ç»ä¸åå­æ¶æ - å¯ä»¥æ´æ¿è¿?
    if(adjustment.arch_type == GPUArchType::UnifiedMemory)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.deadline = 4;  // è¾å®½æ¾çæé
        return p;
    }

    // ç¬ç«æ¾å¡ - å°ReBARææ ReBAR - æ´å®½æ¾çæé
    if(adjustment.arch_type == GPUArchType::DiscreteSmallReBAR)
    {
        p.deadline = 6;
    }

    return p;
}

BufferPolicy MakeManualPolicy(const DevicePolicyAdjustment &adjustment)
{
    // Manualç­ç¥å¯¹ææè®¾å¤ç¸å?- å®å¨ç±åºç¨æ§å?
    return MakeManualPolicy();
}

// è®¾å¤æç¥çGetPolicyForUpdateClass
const BufferPolicy *GetPolicyForUpdateClass(BufferUpdateClass update_class, const DevicePolicyAdjustment &adjustment)
{
    // å¯¹äºæ¯ä¸ªæ´æ°ç±»å«ï¼çæè®¾å¤ééçç­ç?
    // ä½¿ç¨çº¿ç¨æ¬å°å­å¨æ¥é¿åéå¤åé?

    thread_local static BufferPolicy tls_policies[8];
    thread_local static bool tls_initialized = false;

    if(!tls_initialized)
    {
        // åå§åçº¿ç¨æ¬å°ç­ç¥ç¼å­?
        tls_policies[0] = MakeCameraUBOPolicy(adjustment);
        tls_policies[1] = MakeStaticTransformPolicy(adjustment);
        tls_policies[2] = MakeMeshVABPolicy(adjustment);
        tls_policies[3] = MakeDynamicMeshVABPolicy(adjustment);
        tls_policies[4] = MakeTextureTilePolicy(adjustment);
        tls_policies[5] = MakeParticlePolicy(adjustment);
        tls_policies[6] = MakeDeferredPolicy(adjustment);
        tls_policies[7] = MakeManualPolicy(adjustment);
        tls_initialized = true;
    }

    switch(update_class)
    {
        case BufferUpdateClass::CriticalPerFrame:  return &tls_policies[0];
        case BufferUpdateClass::TransformData:     return &tls_policies[1];
        case BufferUpdateClass::MeshStatic:        return &tls_policies[2];
        case BufferUpdateClass::MeshDynamic:       return &tls_policies[3];
        case BufferUpdateClass::TextureTile:       return &tls_policies[4];
        case BufferUpdateClass::Particle:          return &tls_policies[5];
        case BufferUpdateClass::Deferred:          return &tls_policies[6];
        case BufferUpdateClass::Manual:            return &tls_policies[7];
        default:                                   return nullptr;
    }
}

void ApplyPolicy(DeviceBuffer *buf, const BufferPolicy *policy)
{
    if(!buf || !policy)
        return;

    buf->SetPolicy(*policy);

    BufferCommitPolicy commit_policy = policy->commitPolicy;
    if(commit_policy == BufferCommitPolicy::Auto)
    {
        // Let the default selection handle it
    }

    buf->SetCommitPolicy(commit_policy);
}

// ä»ç©çè®¾å¤çæå®æ´çç¼å²ç­ç¥é?
AllDeviceBufferPolicies GenerateAllDeviceBufferPolicies(const VulkanPhyDevice *phy_device)
{
    AllDeviceBufferPolicies policies;

    if(!phy_device)
    {
        // ä½¿ç¨é»è®¤ç­ç¥
        policies.camera_ubo = MakeCameraUBOPolicy();
        policies.static_transform = MakeStaticTransformPolicy();
        policies.mesh_static = MakeMeshVABPolicy();
        policies.mesh_dynamic = MakeDynamicMeshVABPolicy();
        policies.texture_tile = MakeTextureTilePolicy();
        policies.particle = MakeParticlePolicy();
        policies.deferred = MakeDeferredPolicy();
        policies.manual = MakeManualPolicy();
        return policies;
    }

    // è·åè®¾å¤ç¹æ§ï¼çæèªéåºç­ç¥
    DevicePolicyAdjustment adj = GetDevicePolicyAdjustment(phy_device);

    policies.camera_ubo = MakeCameraUBOPolicy(adj);
    policies.static_transform = MakeStaticTransformPolicy(adj);
    policies.mesh_static = MakeMeshVABPolicy(adj);
    policies.mesh_dynamic = MakeDynamicMeshVABPolicy(adj);
    policies.texture_tile = MakeTextureTilePolicy(adj);
    policies.particle = MakeParticlePolicy(adj);
    policies.deferred = MakeDeferredPolicy(adj);
    policies.manual = MakeManualPolicy(adj);

    return policies;
}

}//namespace hgl::graph
