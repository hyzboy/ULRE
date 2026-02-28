#pragma once

#include<hgl/vk/VK.h>
#include<cstddef>
#include<string>

namespace hgl::graph{

/**
 * Layer2 统一接口 —— 外部开发者面向此接口，不直接接触底层实现
 *
 * 实现类（各自独立，互不依赖）：
 *   StagedBuffer  — staging(CPU) + device(GPU) 两块内存，Write/Unmap 自动置脏，
 *                   构造时注册到 VulkanDevice::gpu_buffer_registry，
 *                   ECS RenderBufferUploadSystem 每帧轮询，调用 CopyToDevice 完成上传。
 *   ReBarBuffer   — 单块 CPU-visible + Device-local 内存（需硬件 ReBAR 支持），
 *                   Write 直接可见，CopyToDevice 为 no-op，同样注册 registry。
 *   RingBuffer    — 多帧轮转的 CPU-visible buffer，不注册 registry，
 *                   由 TransformAssignmentBuffer 等外部逻辑每帧直接驱动，
 *                   不经过 RenderBufferUploadSystem。
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
    struct DirtyRange
    {
        VkDeviceSize offset = 0;
        VkDeviceSize size   = 0;
    };

    virtual ~IGPUBuffer() = default;

    const std::string &GetBufferName() const { return buffer_name; }

    // 写入数据（开发者调用）
    virtual bool   Write  (const void *data, VkDeviceSize offset, VkDeviceSize size) = 0;
    virtual void * Map    (VkDeviceSize offset, VkDeviceSize size) = 0;
    virtual void   Unmap  () = 0;

    // 脏标记（只记录，不触发任何提交）
    virtual void   MarkDirty  (VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) = 0;
    virtual void   MarkDirtyRanges(const DirtyRange *ranges, size_t count)
    {
        if (!ranges || count == 0)
            return;

        for (size_t i = 0; i < count; ++i)
            MarkDirty(ranges[i].offset, ranges[i].size);
    }
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
