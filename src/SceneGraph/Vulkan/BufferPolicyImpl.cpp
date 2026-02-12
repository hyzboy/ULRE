#include<hgl/graph/BufferPolicyImpl.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<cstring>

VK_NAMESPACE_BEGIN

DevicePolicyAdjustment GetDevicePolicyAdjustment(const VulkanPhyDevice *phy_device)
{
    DevicePolicyAdjustment adj;
    
    if(!phy_device)
        return adj;
    
    const char *device_name = phy_device->GetDeviceName();
    
    // 识别统一内存架构
    if(device_name)
    {
        std::string name_lower(device_name);
        // 简单转小写以便匹配
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
        // Apple M系列 (通常显示为 "Apple M1", "Apple M2" 等)
        else if(name_lower.find("apple") != std::string::npos)
        {
            adj.arch_type = GPUArchType::UnifiedMemory;
            adj.is_apple_m_series = true;
        }
    }
    
    // 检测ReBAR支持
    adj.has_rebar = phy_device->HasReBAR();
    adj.rebar_size = phy_device->GetReBarSize();
    
    // 对于独立显卡，根据ReBAR大小分类
    if(adj.arch_type == GPUArchType::DiscreteSmallReBAR)
    {
        if(adj.has_rebar && adj.rebar_size > 512ull * 1024 * 1024)  // > 512MB
        {
            adj.arch_type = GPUArchType::DiscreteLargeReBAR;
        }
    }
    
    // 获取总显存容量（取最大的DEVICE_LOCAL堆）
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
// 设备自适应策略生成函数
// ============================================================================

BufferPolicy MakeCameraUBOPolicy(const DevicePolicyAdjustment &adjustment)
{
    BufferPolicy p = MakeCameraUBOPolicy();
    
    // 统一内存架构 - 全程使用ReBAR
    if(adjustment.arch_type == GPUArchType::UnifiedMemory)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.priority = BufferPriority::CRITICAL;
        p.commitPolicy = BufferCommitPolicy::Always;
        return p;
    }
    
    // 独立显卡 - 大ReBAR可以使用ReBAR
    if(adjustment.arch_type == GPUArchType::DiscreteLargeReBAR)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
    }
    // 独立显卡 - 小ReBAR或无ReBAR - 降级到RING
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
    
    // 统一内存架构 - 全程使用ReBAR，可以更激进
    if(adjustment.arch_type == GPUArchType::UnifiedMemory)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 64ull * 1024 * 1024;  // 增大预算
        p.commitPolicy = BufferCommitPolicy::Always;
        return p;
    }
    
    // 独立显卡 - 大ReBAR
    if(adjustment.arch_type == GPUArchType::DiscreteLargeReBAR)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 64ull * 1024 * 1024;  // 较大预算
    }
    // 独立显卡 - 小ReBAR或无ReBAR
    else
    {
        p.budgetLimit = 16ull * 1024 * 1024;  // 缩减预算
        p.memoryPolicy = BufferMemoryPolicy::RING;
        p.ringFrameCount = 3;
    }
    
    return p;
}

BufferPolicy MakeMeshVABPolicy(const DevicePolicyAdjustment &adjustment)
{
    BufferPolicy p = MakeMeshVABPolicy();
    
    // 统一内存架构 - 全程使用ReBAR，预算较宽松
    if(adjustment.arch_type == GPUArchType::UnifiedMemory)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 256ull * 1024 * 1024;  // 较宽松的预算
        return p;
    }
    
    // 独立显卡 - 大ReBAR
    if(adjustment.arch_type == GPUArchType::DiscreteLargeReBAR)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 128ull * 1024 * 1024;  // 较大预算
    }
    // 独立显卡 - 小ReBAR或无ReBAR
    else
    {
        p.budgetLimit = 32ull * 1024 * 1024;   // 受限的预算
        p.memoryPolicy = BufferMemoryPolicy::STAGED;
    }
    
    return p;
}

BufferPolicy MakeDynamicMeshVABPolicy(const DevicePolicyAdjustment &adjustment)
{
    BufferPolicy p = MakeDynamicMeshVABPolicy();
    
    // 统一内存架构 - 全程使用ReBAR
    if(adjustment.arch_type == GPUArchType::UnifiedMemory)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 128ull * 1024 * 1024;
        return p;
    }
    
    // 独立显卡 - 大ReBAR
    if(adjustment.arch_type == GPUArchType::DiscreteLargeReBAR)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 128ull * 1024 * 1024;
    }
    // 独立显卡 - 小ReBAR或无ReBAR
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
    
    // 统一内存架构 - 全程使用ReBAR，可以更激进
    if(adjustment.arch_type == GPUArchType::UnifiedMemory)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 64ull * 1024 * 1024;
        p.splitChunk = 4ull * 1024 * 1024;  // 可以用更大的分割块
        return p;
    }
    
    // 独立显卡 - 大ReBAR
    if(adjustment.arch_type == GPUArchType::DiscreteLargeReBAR)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 32ull * 1024 * 1024;
        p.splitChunk = 2ull * 1024 * 1024;
    }
    // 独立显卡 - 小ReBAR或无ReBAR
    else
    {
        p.budgetLimit = 8ull * 1024 * 1024;
        p.splitChunk = 512ull * 1024;  // 较小的分割块
        p.dropPolicy = BufferDropPolicy::DROP_OLD;
    }
    
    return p;
}

BufferPolicy MakeParticlePolicy(const DevicePolicyAdjustment &adjustment)
{
    BufferPolicy p = MakeParticlePolicy();
    
    // 统一内存架构 - 全程使用ReBAR
    if(adjustment.arch_type == GPUArchType::UnifiedMemory)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 32ull * 1024 * 1024;
        p.splitChunk = 512ull * 1024;  // 较大的分割块
        return p;
    }
    
    // 独立显卡 - 大ReBAR
    if(adjustment.arch_type == GPUArchType::DiscreteLargeReBAR)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.budgetLimit = 32ull * 1024 * 1024;
    }
    // 独立显卡 - 小ReBAR或无ReBAR
    else
    {
        p.budgetLimit = 8ull * 1024 * 1024;
        p.splitChunk = 128ull * 1024;  // 较小の分割块
        p.deadline = 4;
    }
    
    return p;
}

BufferPolicy MakeDeferredPolicy(const DevicePolicyAdjustment &adjustment)
{
    BufferPolicy p = MakeDeferredPolicy();
    
    // 统一内存架构 - 可以更激进
    if(adjustment.arch_type == GPUArchType::UnifiedMemory)
    {
        p.memoryPolicy = BufferMemoryPolicy::REBAR;
        p.deadline = 4;  // 较宽松的期限
        return p;
    }
    
    // 独立显卡 - 小ReBAR或无ReBAR - 更宽松的期限
    if(adjustment.arch_type == GPUArchType::DiscreteSmallReBAR)
    {
        p.deadline = 6;
    }
    
    return p;
}

BufferPolicy MakeManualPolicy(const DevicePolicyAdjustment &adjustment)
{
    // Manual策略对所有设备相同 - 完全由应用控制
    return MakeManualPolicy();
}

// 设备感知的GetPolicyForUpdateClass
const BufferPolicy *GetPolicyForUpdateClass(BufferUpdateClass update_class, const DevicePolicyAdjustment &adjustment)
{
    // 对于每个更新类别，生成设备适配的策略
    // 使用线程本地存储来避免重复分配
    
    thread_local static BufferPolicy tls_policies[8];
    thread_local static bool tls_initialized = false;
    
    if(!tls_initialized)
    {
        // 初始化线程本地策略缓存
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

// 从物理设备生成完整的缓冲策略集
AllDeviceBufferPolicies GenerateAllDeviceBufferPolicies(const VulkanPhyDevice *phy_device)
{
    AllDeviceBufferPolicies policies;
    
    if(!phy_device)
    {
        // 使用默认策略
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
    
    // 获取设备特性，生成自适应策略
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

VK_NAMESPACE_END
