#pragma once

#include<hgl/vk/VK.h>
#include<string>

namespace hgl::graph{

/**
 * Layer2 统一接口
 *
 * StagedBuffer / ReBarBuffer / RingBuffer 均实现此接口
 * Layer3 (Accessor) 和 ECS System 只依赖此接口
 *
 * 规则：
 * - MarkDirty/IsDirty/ClearDirty 只记录状态，不触发任何提交
 * - CopyToDevice 由 ECS System (RenderBufferUploadSystem) 统一调用
 * - 实现类不持有任何队列引用
 */
class IGPUBuffer
{
protected:
    const std::string buffer_name;  // e.g. "PlaneGrid:Position", set once at construction

    IGPUBuffer() = delete;
    explicit IGPUBuffer(const std::string &name) : buffer_name(name) {}

public:
    virtual ~IGPUBuffer() = default;

    const std::string &GetBufferName() const { return buffer_name; }

    // 写入数据（开发者调用）
    virtual bool   Write  (const void *data, VkDeviceSize offset, VkDeviceSize size) = 0;
    virtual void * Map    (VkDeviceSize offset, VkDeviceSize size) = 0;
    virtual void   Unmap  () = 0;

    // 脏标记（只记录，不触发任何提交）
    virtual void   MarkDirty  (VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) = 0;
    virtual bool   IsDirty    () const = 0;
    virtual void   ClearDirty () = 0;

    // ECS System 调用：将 staging 数据复制到 device buffer
    virtual void   CopyToDevice(VkCommandBuffer cmd) = 0;

    // 基本信息
    virtual VkDeviceSize  GetSize() const = 0;
    virtual VkBuffer      GetVkDeviceBuffer() const = 0;
    virtual VkDescriptorBufferInfo GetDescriptorBufferInfo() const = 0;
};//class IGPUBuffer

}//namespace hgl::graph
