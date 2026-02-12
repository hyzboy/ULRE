#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VKMemory.h>
#include<hgl/graph/VKStagedBuffer.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>

#include<string>

VK_NAMESPACE_BEGIN
class VulkanDevice;
class RawBufferAccessor;
struct DeviceBufferData;

enum class BufferCommitPolicy
{
    Auto,       // Decide by buffer usage / memory type
    StagedOnly, // Only flush staged buffers when dirty
    Always,     // Always flush on Update
    Manual      // Never auto flush
};

enum class BufferUpdateClass
{
    Default,
    CriticalPerFrame, // camera, per-frame UBO
    TransformData,    // transform ID/data buffers
    MeshStatic,       // static VBO/IBO
    MeshDynamic,      // dynamic VBO/IBO
    TextureTile,      // tile/streaming textures
    Particle,         // particle positions
    Deferred,         // can be applied next frame
    Manual            // caller controls manually
};

enum class BufferPriority
{
    CRITICAL = 0,     // Must submit earliest
    HIGH = 1,         // High priority
    NORMAL = 2,       // Normal priority
    LOW = 3           // Low priority, can defer
};

enum class BufferUpdateRate
{
    PER_FRAME = 0,    // Update every frame
    FREQUENT = 1,     // Update frequently
    BURST = 2,        // Burst updates then stable
    SPARSE = 3,       // Sparse updates
    RARE = 4          // Rarely updated
};

enum class BufferSubmitTiming
{
    IMMEDIATE = 0,    // Submit immediately
    SAME_FRAME = 1,   // Must submit in same frame
    NEXT_FRAME_OK = 2,// Can defer to next frame
    DEFERRED = 3      // Arbitrary deferral OK
};

enum class BufferDropPolicy
{
    NEVER = 0,        // Never drop data
    DROP_OLD = 1,     // Drop old pending data
    DROP_NEW = 2      // Drop new incoming data
};

enum class BufferDeadlinePolicy
{
    NONE = 0,         // No hard deadline
    SOFT = 1,         // Soft deadline, promote priority
    HARD = 2          // Hard deadline, force immediate
};

enum class BufferPromotePolicy
{
    NONE = 0,         // No auto-promotion
    AUTO_RAISE = 1,   // Raise priority one level
    FORCE_HIGH = 2    // Force to HIGH priority
};

enum class BufferMemoryPolicy
{
    REBAR = 0,        // Resizable BAR (or fallback)
    RING = 1,         // Ring buffer (N-frame cycle)
    STAGED = 2,       // Staged buffer (CPU->GPU)
    AUTO = 3          // Auto-select by usage
};

enum class BufferCpuResident
{
    KEEP = 0,         // Keep CPU data alive
    RELEASE = 1,      // Can release after submit
    AUTO = 2          // System decides
};

enum class BufferSplitPolicy
{
    NO_SPLIT = 0,     // Never split
    ALLOW_SPLIT = 1,  // Allow splitting
    PREFER_SPLIT = 2  // Prefer splitting
};

struct BufferPolicy
{
    BufferPriority priority = BufferPriority::NORMAL;
    BufferUpdateRate updateRate = BufferUpdateRate::RARE;
    BufferSubmitTiming submitTiming = BufferSubmitTiming::DEFERRED;
    uint32_t maxLatency = 2;                          // 0 = AUTO
    std::string budgetGroup = "GLOBAL";
    VkDeviceSize budgetLimit = 0;                     // 0 = AUTO
    bool queueing = true;                             // Enable queue submission
    BufferSplitPolicy splitPolicy = BufferSplitPolicy::NO_SPLIT;
    VkDeviceSize splitChunk = 0;                      // 0 = AUTO
    BufferDropPolicy dropPolicy = BufferDropPolicy::NEVER;
    BufferDeadlinePolicy deadlinePolicy = BufferDeadlinePolicy::NONE;
    uint32_t deadline = 0;                            // 0 = AUTO
    BufferPromotePolicy promotePolicy = BufferPromotePolicy::NONE;
    std::string promoteRule;
    BufferMemoryPolicy memoryPolicy = BufferMemoryPolicy::AUTO;
    BufferCpuResident cpuResident = BufferCpuResident::AUTO;
    uint32_t ringFrameCount = 3;                      // For RING memory policy
    BufferCpuResident stagedPersist = BufferCpuResident::AUTO;  // For STAGED policy
    BufferCommitPolicy commitPolicy = BufferCommitPolicy::Auto;
    std::string devNotes;
};

struct DeviceBufferData
{
    VkBuffer                buffer=nullptr;
    DeviceMemory *          memory=nullptr;
    VkDescriptorBufferInfo  info;
};//struct DeviceBufferData

class DeviceBuffer
{
protected:

    VulkanDevice *owner_device=nullptr;
    VkDevice device;
    DeviceBufferData buf;
    StagedBuffer *staged_buffer=nullptr;
    RawBufferAccessor *auto_commit_proxy=nullptr;
    BufferCommitPolicy commit_policy=BufferCommitPolicy::Auto;
    BufferUpdateClass update_class=BufferUpdateClass::Default;
    VkDeviceSize staged_map_offset=0;
    VkDeviceSize staged_map_size=0;
    bool staged_map_active=false;

    BufferPolicy policy;
    
    // Frame tracking for deadline enforcement
    uint32_t frames_since_update = 0;               // Frames since last commit

private:

    friend class VulkanDevice;
    friend class VertexAttribBuffer;
    friend class IndexBuffer;
    template<typename T> friend class IndirectCommandBuffer;

    DeviceBuffer(VulkanDevice *owner,VkDevice d,const DeviceBufferData &b)
    {
        owner_device=owner;
        device=d;
        buf=b;
    }

    DeviceBuffer(VulkanDevice *owner,VkDevice d,const DeviceBufferData &b,StagedBuffer *sb)
    {
        owner_device=owner;
        device=d;
        buf=b;
        staged_buffer=sb;
    }

public:

    virtual ~DeviceBuffer();

            VkBuffer                    GetBuffer       ()const{return buf.buffer;}
            DeviceMemory *              GetMemory       ()const{return buf.memory;}
            VkDeviceMemory              GetVkMemory     ()const{return buf.memory->operator VkDeviceMemory();}
    const   VkDescriptorBufferInfo *    GetBufferInfo   ()const{return &buf.info;}
            VkDeviceSize                GetSize         ()const{return buf.info.range;}
                VulkanDevice *              GetOwnerDevice  ()const{return owner_device;}
                bool                        HasStagedDirty  ()const{return staged_buffer ? staged_buffer->IsDirty() : false;}
                void                        SetAutoCommitProxy(RawBufferAccessor *proxy){auto_commit_proxy=proxy;}
                void                        SetCommitPolicy(BufferCommitPolicy policy){commit_policy=policy;}
                BufferCommitPolicy          GetCommitPolicy()const{return commit_policy;}
                void                        SetUpdateClass(BufferUpdateClass cls){update_class=cls;}
                BufferUpdateClass           GetUpdateClass()const{return update_class;}
                
                // Policy accessors
                void                        SetPriority(BufferPriority p){policy.priority=p;}
                BufferPriority              GetPriority()const{return policy.priority;}
                void                        SetUpdateRate(BufferUpdateRate r){policy.updateRate=r;}
                BufferUpdateRate            GetUpdateRate()const{return policy.updateRate;}
                void                        SetSubmitTiming(BufferSubmitTiming t){policy.submitTiming=t;}
                BufferSubmitTiming          GetSubmitTiming()const{return policy.submitTiming;}
                void                        SetMaxLatency(uint32_t f){policy.maxLatency=f;}
                uint32_t                    GetMaxLatency()const{return policy.maxLatency;}
                void                        SetBudgetGroup(const std::string &g){policy.budgetGroup=g;}
                const std::string&          GetBudgetGroup()const{return policy.budgetGroup;}
                void                        SetBudgetLimit(VkDeviceSize limit){policy.budgetLimit=limit;}
                VkDeviceSize                GetBudgetLimit()const{return policy.budgetLimit;}
                void                        SetQueueing(bool q){policy.queueing=q;}
                bool                        GetQueueing()const{return policy.queueing;}
                void                        SetSplitPolicy(BufferSplitPolicy p){policy.splitPolicy=p;}
                BufferSplitPolicy           GetSplitPolicy()const{return policy.splitPolicy;}
                void                        SetSplitChunk(VkDeviceSize size){policy.splitChunk=size;}
                VkDeviceSize                GetSplitChunk()const{return policy.splitChunk;}
                void                        SetDropPolicy(BufferDropPolicy p){policy.dropPolicy=p;}
                BufferDropPolicy            GetDropPolicy()const{return policy.dropPolicy;}
                void                        SetDeadlinePolicy(BufferDeadlinePolicy p){policy.deadlinePolicy=p;}
                BufferDeadlinePolicy        GetDeadlinePolicy()const{return policy.deadlinePolicy;}
                void                        SetDeadline(uint32_t d){policy.deadline=d;}
                uint32_t                    GetDeadline()const{return policy.deadline;}
                void                        SetPromotePolicy(BufferPromotePolicy p){policy.promotePolicy=p;}
                BufferPromotePolicy         GetPromotePolicy()const{return policy.promotePolicy;}
                void                        SetPromoteRule(const std::string &r){policy.promoteRule=r;}
                const std::string&          GetPromoteRule()const{return policy.promoteRule;}
                void                        SetMemoryPolicy(BufferMemoryPolicy p){policy.memoryPolicy=p;}
                BufferMemoryPolicy          GetMemoryPolicy()const{return policy.memoryPolicy;}
                void                        SetCpuResident(BufferCpuResident r){policy.cpuResident=r;}
                BufferCpuResident           GetCpuResident()const{return policy.cpuResident;}
                void                        SetRingFrameCount(uint32_t n){policy.ringFrameCount=n;}
                uint32_t                    GetRingFrameCount()const{return policy.ringFrameCount;}
                void                        SetStagedPersist(BufferCpuResident p){policy.stagedPersist=p;}
                BufferCpuResident           GetStagedPersist()const{return policy.stagedPersist;}
                void                        SetDevNotes(const std::string &notes){policy.devNotes=notes;}
                const std::string&          GetDevNotes()const{return policy.devNotes;}
                void                        SetPolicy(const BufferPolicy &p){policy=p;}
                const BufferPolicy&         GetPolicy()const{return policy;}
                
                // Frame tracking for deadline enforcement
                uint32_t                    GetFramesSinceUpdate()const{return frames_since_update;}
                void                        IncrementFramesSinceUpdate(){frames_since_update++;}
                void                        ResetFramesSinceUpdate(){frames_since_update=0;}

            void *  Map     ()
            {
                if(staged_buffer)
                {
                    staged_map_offset=0;
                    staged_map_size=VK_WHOLE_SIZE;
                    staged_map_active=true;
                    return staged_buffer->Map();
                }

                return buf.memory?buf.memory->Map():nullptr;
            }
    virtual void *  Map     (VkDeviceSize start,VkDeviceSize size)
            {
                if(staged_buffer)
                {
                    staged_map_offset=start;
                    staged_map_size=(size==0)?VK_WHOLE_SIZE:size;
                    staged_map_active=true;
                    return staged_buffer->Map(start,size);
                }

                return buf.memory?buf.memory->Map(start,size):nullptr;
            }
            void    Unmap   ()
            {
                if(staged_buffer)
                {
                    staged_buffer->Unmap();
                    if(staged_map_active)
                    {
                        staged_buffer->MarkDirty(staged_map_offset,staged_map_size);
                        staged_map_active=false;
                    }
                    return;
                }

                if(buf.memory)
                    buf.memory->Unmap();
            }
    virtual void    Flush   (VkDeviceSize start,VkDeviceSize size)
            {
                if(staged_buffer)
                {
                    staged_buffer->MarkDirty(start,(size==0)?VK_WHOLE_SIZE:size);
                    return;
                }

                if(buf.memory)
                    buf.memory->Flush(start,size);
            }
    virtual void    Flush   (VkDeviceSize size)
            {
                if(staged_buffer)
                {
                    staged_buffer->MarkDirty(0,(size==0)?VK_WHOLE_SIZE:size);
                    return;
                }

                if(buf.memory)
                    buf.memory->Flush(size);
            }

    virtual bool    Write   (const void *ptr,uint32_t start,uint32_t size)
            {
                if(staged_buffer)
                    return staged_buffer->Write(ptr,start,size);

                return buf.memory?buf.memory->Write(ptr,start,size):false;
            }
    virtual bool    Write   (const void *ptr,uint32_t size)
            {
                if(staged_buffer)
                    return staged_buffer->Write(ptr,0,size);

                return buf.memory?buf.memory->Write(ptr,0,size):false;
            }
            bool    Write   (const void *ptr)
            {
                if(staged_buffer)
                    return staged_buffer->Write(ptr);

                return buf.memory?buf.memory->Write(ptr):false;
            }
};//class DeviceBuffer

VK_NAMESPACE_END
