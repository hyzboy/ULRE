#pragma once

#include<hgl/vk/VKBufferOwner.h>

namespace hgl::graph{

template<typename T>
class IndirectCommandBuffer:public VkBufferOwner
{
protected:

    uint32_t max_count;

public:

    const uint32_t      GetMaxCount     ()const{ return max_count; }                ///<取得最大指令数
    const uint32_t      GetCommandLength()const{ return sizeof(T); }                ///<取得单个指令的长度字节数
    const VkDeviceSize  GetTotalBytes   ()const{ return sizeof(T)*max_count; }      ///<取得缓冲区总计字节数

private:

    friend class VulkanDevice;

    IndirectCommandBuffer(VkDevice d,const DeviceBufferData &vb,const uint32_t mc):VkBufferOwner(d,vb)
    {
        max_count=mc;
    }

public:

    virtual ~IndirectCommandBuffer()=default;

    void *  Map     (VkDeviceSize start,VkDeviceSize size)                  {return GetGPUBuffer()->Map(start*sizeof(T),size*sizeof(T));}
    void    Unmap   ()                                                      {GetGPUBuffer()->Unmap();}
    T *     MapCmd  (VkDeviceSize start,VkDeviceSize size)                  {return (T *)Map(start,size);}
    T *     MapCmd  ()                                                      {return (T *)Map(0,max_count);}

    void    Flush   (VkDeviceSize start,VkDeviceSize size)                  {GetGPUBuffer()->MarkDirty(start*sizeof(T),size*sizeof(T));}
    void    Flush   (VkDeviceSize size)                                     {GetGPUBuffer()->MarkDirty(0,size*sizeof(T));}

    bool    Write   (const void *ptr,uint32_t start,uint32_t size)          {return GetGPUBuffer()->Write(ptr,start*sizeof(T),size*sizeof(T));}
    bool    Write   (const void *ptr,uint32_t size)                         {return GetGPUBuffer()->Write(ptr,0,size*sizeof(T));}

    bool    WriteCmd(const T *ptr,uint32_t start,uint32_t size)             {return GetGPUBuffer()->Write(ptr,start*sizeof(T),size*sizeof(T));}
    bool    WriteCmd(const T *ptr,uint32_t size)                            {return GetGPUBuffer()->Write(ptr,0,size*sizeof(T));}

    /**
     * Returns the underlying VkBuffer handle for vkCmdDrawIndirect / vkCmdDispatchIndirect.
     * Prefer this over GetBuffer() — does not require DeviceBuffer inheritance.
     */
    VkBuffer GetVkBuffer() const { return GetGPUBuffer()->GetVkDeviceBuffer(); }
};//class IndirectCommandBuffer:public VkBufferOwner

class IndirectDrawBuffer:public IndirectCommandBuffer<VkDrawIndirectCommand>
{
    friend class VulkanDevice;

public:

    using IndirectCommandBuffer<VkDrawIndirectCommand>::IndirectCommandBuffer;

    void Draw(VkCommandBuffer cmd_buf,uint32_t cmd_offset,uint32_t draw_count) const
    {
        vkCmdDrawIndirect(cmd_buf,
                          GetVkBuffer(),
                          cmd_offset*sizeof(VkDrawIndirectCommand),
                          draw_count,
                          sizeof(VkDrawIndirectCommand));
    }
};//class IndirectDrawBuffer:public IndirectCommandBuffer<VkDrawIndirectCommand>

class IndirectDispatchBuffer:public IndirectCommandBuffer<VkDispatchIndirectCommand>
{
    friend class VulkanDevice;

public:

    using IndirectCommandBuffer<VkDispatchIndirectCommand>::IndirectCommandBuffer;

    void Dispatch(VkCommandBuffer cmd_buf,uint32_t offset) const
    {
        vkCmdDispatchIndirect(cmd_buf,GetVkBuffer(),offset);
    }
};//class IndirectDispatchBuffer:public IndirectCommandBuffer<VkDispatchIndirectCommand>
}//namespace hgl::graph
