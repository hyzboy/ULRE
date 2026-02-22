#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/BufferPolicy.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/vk/VKBufferTransferAgent.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>
#include<hgl/log/Log.h>

#include<string>

namespace hgl::graph{
class VulkanDevice;
class RawBufferAccessor;
struct DeviceBufferData;

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
    BufferTransferAgent *transfer_agent=nullptr;
    RawBufferAccessor *auto_commit_proxy=nullptr;
    BufferCommitPolicy commit_policy=BufferCommitPolicy::Auto;
    BufferUpdateClass update_class=BufferUpdateClass::Default;

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
        transfer_agent=new StagedBufferTransferAgent(sb);
    }

    DeviceBuffer(VulkanDevice *owner,VkDevice d,const DeviceBufferData &b,BufferTransferAgent *agent)
    {
        owner_device=owner;
        device=d;
        buf=b;
        transfer_agent=agent;
    }

public:

    virtual ~DeviceBuffer();

            VkBuffer                    GetBuffer       ()const{return buf.buffer;}
            DeviceMemory *              GetMemory       ()const{return buf.memory;}
            VkDeviceMemory              GetVkMemory     ()const{return buf.memory->operator VkDeviceMemory();}
    const   VkDescriptorBufferInfo *    GetBufferInfo   ()const{return &buf.info;}
            VkDeviceSize                GetSize         ()const{return buf.info.range;}
            VulkanDevice *              GetOwnerDevice  ()const{return owner_device;}
            bool                        HasStagedDirty  ()const{return transfer_agent ? transfer_agent->HasPendingUpload() : false;}
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
                if(transfer_agent)
                    return transfer_agent->Map(0, VK_WHOLE_SIZE);

                return buf.memory?buf.memory->Map():nullptr;
            }
    virtual void *  Map     (VkDeviceSize start,VkDeviceSize size)
            {
                if(transfer_agent)
                    return transfer_agent->Map(start, size);

                return buf.memory?buf.memory->Map(start,size):nullptr;
            }
            void    Unmap   ()
            {
                if(transfer_agent)
                {
                    transfer_agent->Unmap();
                    return;
                }

                if(buf.memory)
                    buf.memory->Unmap();
            }
    virtual void    Flush   (VkDeviceSize start,VkDeviceSize size)
            {
                if(transfer_agent)
                {
                    transfer_agent->Flush(start, size);
                    return;
                }

                if(buf.memory)
                {
//#ifdef _DEBUG
//                    if(size==0)
//                    {
//                        GLogWarning("[DeviceBuffer::Flush] size=0 (interpreted as whole) buffer=%p vkBuffer=%p memory=%p memSize=%llu",
//                                    (void *)this,
//                                    (void *)buf.buffer,
//                                    (void *)static_cast<VkDeviceMemory>(*buf.memory),
//                                    static_cast<unsigned long long>(buf.memory->GetSize()));
//                    }
//                    else if(start + size > buf.memory->GetSize())
//                    {
//                        GLogWarning("[DeviceBuffer::Flush] range overflow buffer=%p vkBuffer=%p memory=%p start=%llu size=%llu memSize=%llu",
//                                    (void *)this,
//                                    (void *)buf.buffer,
//                                    (void *)static_cast<VkDeviceMemory>(*buf.memory),
//                                    static_cast<unsigned long long>(start),
//                                    static_cast<unsigned long long>(size),
//                                    static_cast<unsigned long long>(buf.memory->GetSize()));
//                    }
//#endif//_DEBUG
                    buf.memory->Flush(start,size);
                }
            }
    virtual void    Flush   (VkDeviceSize size)
            {
                if(transfer_agent)
                {
                    transfer_agent->Flush(0, size);
                    return;
                }

                if(buf.memory)
                {
//#ifdef _DEBUG
//                    if(size==0)
//                    {
//                        GLogWarning("[DeviceBuffer::Flush] size=0 (interpreted as whole) buffer=%p vkBuffer=%p memory=%p memSize=%llu",
//                                    (void *)this,
//                                    (void *)buf.buffer,
//                                    (void *)static_cast<VkDeviceMemory>(*buf.memory),
//                                    static_cast<unsigned long long>(buf.memory->GetSize()));
//                    }
//                    else if(size > buf.memory->GetSize())
//                    {
//                        GLogWarning("[DeviceBuffer::Flush] size overflow buffer=%p vkBuffer=%p memory=%p size=%llu memSize=%llu",
//                                    (void *)this,
//                                    (void *)buf.buffer,
//                                    (void *)static_cast<VkDeviceMemory>(*buf.memory),
//                                    static_cast<unsigned long long>(size),
//                                    static_cast<unsigned long long>(buf.memory->GetSize()));
//                    }
//#endif//_DEBUG
                    buf.memory->Flush(size);
                }
            }

    virtual bool    Write   (const void *ptr,uint32_t start,uint32_t size)
            {
                if(transfer_agent)
                    return transfer_agent->Write(ptr,start,size);

                return buf.memory?buf.memory->Write(ptr,start,size):false;
            }
    virtual bool    Write   (const void *ptr,uint32_t size)
            {
                if(transfer_agent)
                    return transfer_agent->Write(ptr,0,size);

                return buf.memory?buf.memory->Write(ptr,0,size):false;
            }
            bool    Write   (const void *ptr)
            {
                if(transfer_agent)
                    return transfer_agent->Write(ptr,0,static_cast<uint32_t>(buf.info.range));

                return buf.memory?buf.memory->Write(ptr):false;
            }
};//class DeviceBuffer

}//namespace hgl::graph
