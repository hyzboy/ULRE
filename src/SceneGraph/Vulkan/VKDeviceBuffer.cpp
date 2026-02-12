#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKBufferAccessBase.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<iostream>

VK_NAMESPACE_BEGIN
static BufferAllocPolicy ResolvePolicy(VulkanDevice *device, BufferAllocPolicy policy)
{
    if(policy!=BufferAllocPolicy::Auto)
        return policy;

    if(device->GetPhyDevice()->HasReBAR())
        return BufferAllocPolicy::CPUVisible;

    return BufferAllocPolicy::StagedUpload;
}

static BufferCommitPolicy SelectCommitPolicy(BufferUpdateClass update_class, VkBufferUsageFlags usage, BufferAllocPolicy policy)
{
    if(update_class == BufferUpdateClass::Manual)
        return BufferCommitPolicy::Manual;

    if(update_class == BufferUpdateClass::CriticalPerFrame || update_class == BufferUpdateClass::TransformData)
        return BufferCommitPolicy::Always;

    if(update_class == BufferUpdateClass::MeshStatic || update_class == BufferUpdateClass::MeshDynamic ||
       update_class == BufferUpdateClass::TextureTile || update_class == BufferUpdateClass::Particle ||
       update_class == BufferUpdateClass::Deferred)
        return BufferCommitPolicy::StagedOnly;

    if(policy == BufferAllocPolicy::StagedUpload || policy == BufferAllocPolicy::GPUOnly)
        return BufferCommitPolicy::StagedOnly;

    if((usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) || (usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
        return BufferCommitPolicy::Always;

    return BufferCommitPolicy::Auto;
}

static void ApplyUpdateClass(DeviceBuffer *buf, BufferUpdateClass update_class, VkBufferUsageFlags usage, BufferAllocPolicy policy)
{
    if(!buf)
        return;

    buf->SetUpdateClass(update_class);
    buf->SetCommitPolicy(SelectCommitPolicy(update_class, usage, policy));
}

static BufferPolicy MakeCameraUBOPolicy()
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

static BufferPolicy MakeStaticTransformPolicy()
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

static BufferPolicy MakeMeshVABPolicy()
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

static BufferPolicy MakeDynamicMeshVABPolicy()
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

static BufferPolicy MakeTextureTilePolicy()
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

static BufferPolicy MakeParticlePolicy()
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

static BufferPolicy MakeDeferredPolicy()
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

static BufferPolicy MakeManualPolicy()
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

static const BufferPolicy *GetPolicyForUpdateClass(BufferUpdateClass update_class)
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

static void ApplyPolicy(DeviceBuffer *buf, const BufferPolicy *policy)
{
    if(!buf || !policy)
        return;

    buf->SetPolicy(*policy);

    BufferCommitPolicy commit_policy = policy->commitPolicy;
    if(commit_policy == BufferCommitPolicy::Auto)
        commit_policy = SelectCommitPolicy(buf->GetUpdateClass(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, BufferAllocPolicy::Auto);

    buf->SetCommitPolicy(commit_policy);
}

static void ApplyUpdateClassWithPolicy(VulkanDevice *device, DeviceBuffer *buf, BufferUpdateClass update_class,
                                       VkBufferUsageFlags usage, BufferAllocPolicy policy)
{
    ApplyUpdateClass(buf, update_class, usage, policy);

    if(!buf)
        return;

    const BufferPolicy *hardcoded = GetPolicyForUpdateClass(update_class);
    if(hardcoded)
        ApplyPolicy(buf, hardcoded);
}

const VkDeviceSize VulkanDevice::GetUBOAlign   (){return attr->physical_device->GetUBOAlign();}
const VkDeviceSize VulkanDevice::GetSSBOAlign  (){return attr->physical_device->GetSSBOAlign();}
const VkDeviceSize VulkanDevice::GetUBORange   (){return attr->physical_device->GetUBORange();}
const VkDeviceSize VulkanDevice::GetSSBORange  (){return attr->physical_device->GetSSBORange();}

bool VulkanDevice::CreateBuffer(DeviceBufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode)
{
    return CreateBuffer(buf,buf_usage,range,size,data,sharing_mode,MemoryUsage::CPUOnly);
}

bool VulkanDevice::CreateBuffer(DeviceBufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode,MemoryUsage mem_usage)
{
    if(size<=0)return(false);

    BufferCreateInfo buf_info;

    buf_info.usage                  = buf_usage;
    buf_info.size                   = size;
    buf_info.queueFamilyIndexCount  = 0;
    buf_info.pQueueFamilyIndices    = nullptr;
    buf_info.sharingMode            = VkSharingMode(sharing_mode);

    if(vkCreateBuffer(attr->device,&buf_info,nullptr,&buf->buffer)!=VK_SUCCESS)
        return(false);

    VkMemoryRequirements mem_reqs;

    vkGetBufferMemoryRequirements(attr->device,buf->buffer,&mem_reqs);

    DeviceMemory *dm=CreateMemory(mem_reqs,mem_usage);

    if(dm&&dm->BindBuffer(buf->buffer))
    {
        buf->info.buffer  =buf->buffer;
        buf->info.offset  =0;
        buf->info.range   =range;

        buf->memory       =dm;

        if(!data)
            return(true);

        dm->Write(data,0,size);
        return(true);
    }

    delete dm;

    vkDestroyBuffer(attr->device,buf->buffer,nullptr);
    return(false);
}

VAB *VulkanDevice::CreateVAB(VkFormat format,uint32_t count,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode,BufferUpdateClass update_class)
{
    if(count==0)return(nullptr);

    const uint32_t stride=GetStrideByFormat(format);

    if(stride==0)
    {
        std::cerr<<"format["<<format<<"] stride length is 0,please use \"VulkanDevice::CreateBuffer(VkBufferUsageFlags,VkDeviceSize,VkSharingMode)\" function.";
        return(nullptr);
    }

    const VkDeviceSize size=stride*count;

    policy = ResolvePolicy(this, policy);

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,size,data,sharing_mode);
        if(!staged)
            return(nullptr);

        DeviceBufferData buf;
        buf.buffer=staged->GetDeviceBuffer();
        buf.memory=staged->GetDeviceMemory();
        buf.info.buffer=buf.buffer;
        buf.info.offset=0;
        buf.info.range=size;

            VertexAttribBuffer *vab = new VertexAttribBuffer(this,attr->device,buf,format,stride,count,staged);
            ApplyUpdateClassWithPolicy(this, vab, update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, policy);
            vab->SetAutoCommitProxy(new RawBufferAccessor(vab));
            return vab;
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    DeviceBufferData buf;
    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,size,size,data,sharing_mode,mem_usage))
        return(nullptr);

        VertexAttribBuffer *vab = new VertexAttribBuffer(this,attr->device,buf,format,stride,count);
        ApplyUpdateClassWithPolicy(this, vab, update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class,
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, policy);
        vab->SetAutoCommitProxy(new RawBufferAccessor(vab));
        return vab;
}

const bool VulkanDevice::IsSupport(const IndexType &type)const
{
    if(type==IndexType::U16)return(true);
    if(type==IndexType::U8 &&attr->uint8_index_type)return(true);
    if(type==IndexType::U32&&attr->uint32_index_type)return(true);
    return(false);
}

const IndexType VulkanDevice::ChooseIndexType(const VkDeviceSize &vertex_count)const
{
    if(vertex_count<=0)return(IndexType::ERR);

    if(attr->uint8_index_type&& vertex_count<=0xFF  )return IndexType::U8;  else
    if(                         vertex_count<=0xFFFF)return IndexType::U16; else
    if(attr->uint32_index_type  )return IndexType::U32; else

    return IndexType::ERR;
}

const bool VulkanDevice::CheckIndexType(const IndexType it,const VkDeviceSize &vertex_count)const
{
    if(vertex_count<=0)return(false);

    if(it==IndexType::U16&&vertex_count<=0xFFFF)return(true);

    if(it==IndexType::U32&&                     attr->uint32_index_type)return(true);

    if(it==IndexType::U8 &&vertex_count<=0xFF&& attr->uint8_index_type)return(true);

    return(false);
}

IndexBuffer *VulkanDevice::CreateIBO(IndexType index_type,uint32_t count,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode,BufferUpdateClass update_class)
{
    if(count==0)return(nullptr);

    uint32_t stride;

    if(index_type==IndexType::U8 )stride=1;else
    if(index_type==IndexType::U16)stride=2;else
    if(index_type==IndexType::U32)stride=4;else
        return(nullptr);

    const VkDeviceSize size=stride*count;

    policy = ResolvePolicy(this, policy);

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT,size,data,sharing_mode);
        if(!staged)
            return(nullptr);

        DeviceBufferData buf;
        buf.buffer=staged->GetDeviceBuffer();
        buf.memory=staged->GetDeviceMemory();
        buf.info.buffer=buf.buffer;
        buf.info.offset=0;
        buf.info.range=size;

            IndexBuffer *ibo = new IndexBuffer(this,attr->device,buf,index_type,count,staged);
            ApplyUpdateClassWithPolicy(this, ibo, update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class,
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, policy);
            ibo->SetAutoCommitProxy(new RawBufferAccessor(ibo));
            return ibo;
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    DeviceBufferData buf;
    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_INDEX_BUFFER_BIT,size,size,data,sharing_mode,mem_usage))
        return(nullptr);

        IndexBuffer *ibo = new IndexBuffer(this,attr->device,buf,index_type,count);
        ApplyUpdateClassWithPolicy(this, ibo, update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class,
                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, policy);
        ibo->SetAutoCommitProxy(new RawBufferAccessor(ibo));
        return ibo;
}

DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode)
{
    return CreateBuffer(buf_usage,range,size,data,BufferAllocPolicy::Auto,sharing_mode);
}

DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode)
{
    if(size<=0)return(nullptr);

    policy = ResolvePolicy(this, policy);

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(buf_usage,size,data,sharing_mode);
        if(!staged)
            return(nullptr);

        DeviceBufferData buf;
        buf.buffer=staged->GetDeviceBuffer();
        buf.memory=staged->GetDeviceMemory();
        buf.info.buffer=buf.buffer;
        buf.info.offset=0;
        buf.info.range=range;

            DeviceBuffer *dev_buf = new DeviceBuffer(this,attr->device,buf,staged);
            ApplyUpdateClassWithPolicy(this, dev_buf, BufferUpdateClass::Default, buf_usage, policy);
        dev_buf->SetAutoCommitProxy(new RawBufferAccessor(dev_buf));
        return dev_buf;
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    DeviceBufferData buf;

    if(!CreateBuffer(&buf,buf_usage,range,size,data,sharing_mode,mem_usage))
        return(nullptr);

        DeviceBuffer *dev_buf = new DeviceBuffer(this,attr->device,buf);
        ApplyUpdateClassWithPolicy(this, dev_buf, BufferUpdateClass::Default, buf_usage, policy);
    dev_buf->SetAutoCommitProxy(new RawBufferAccessor(dev_buf));
    return dev_buf;
}

    DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode,BufferUpdateClass update_class)
    {
        DeviceBuffer *buf = CreateBuffer(buf_usage,range,size,data,policy,sharing_mode);
        ApplyUpdateClassWithPolicy(this, buf, update_class, buf_usage, ResolvePolicy(this, policy));
        return buf;
    }

    DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode,BufferUpdateClass update_class)
    {
        return CreateBuffer(buf_usage,range,size,data,BufferAllocPolicy::Auto,sharing_mode,update_class);
    }
VK_NAMESPACE_END
