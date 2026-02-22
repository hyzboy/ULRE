#pragma once

#include<hgl/vk/VK.h>

namespace hgl::graph
{
/**
 * CN: 缓冲写入代理接口（当前为 dead abstraction，保留待评估）
 * EN: Buffer write agent interface (currently a dead abstraction, kept for evaluation)
 *
 * 现状 / Current status:
 * - 唯一实现者: `RingBufferWrapper`
 * - 所有持有方（TransformAssignmentBuffer、MaterialInstanceAssignmentBuffer）均持有
 *   具体类型 `RingBufferWrapper`，而非此接口指针——多态性从未被实际使用
 * - `StagedBuffer` 路径完全绕过此接口，直接通过 DeviceBuffer::Write/Unmap 路径写入
 * - `BufferAccessor<T>` / `StructuredBufferAccessor<T>` 不使用此接口
 *
 * 结论 / Conclusion:
 * 此接口当前没有任何接口调度发生。若第二个实现者（如 StagedBufferWriteAgent）
 * 始终不出现，应在 Phase 3 架构重构时将其删除，RingBufferWrapper 改为普通类。
 *
 * 原始设计意图 / Original design intent:
 * 抽象 StagedBuffer/RingBuffer/ReBar 的写入差异，由 BufferAccessor 通过此接口访问底层存储
 */
class BufferWriteAgent
{
public:
    virtual ~BufferWriteAgent() = default;

    /**
     * Map a range of buffer for CPU writing
     * @param offset Element offset (0-based)
     * @param count Element count to map
     * @return Mapped pointer, or nullptr if failed
     */
    virtual void* MapRange(VkDeviceSize offset, VkDeviceSize count) = 0;

    /**
     * Unmap a previously mapped range
     * Triggers appropriate backend flush/staging logic
     */
    virtual void Unmap() = 0;

    /**
     * Write bulk data to buffer
     * @param data Source data pointer
     * @param offset Element offset
     * @param count Element count
     * @return Whether write succeeded
     */
    virtual bool WriteRange(const void *data, VkDeviceSize offset, VkDeviceSize count) = 0;

    /**
     * Check if buffer has pending data to flush (staging buffers)
     * @return true if staging buffer is dirty
     */
    virtual bool HasPendingUpload() const = 0;

    /**
     * Mark buffer as needing commit to GPU
     * Used by StructuredBufferAccessor-like wrappers
     */
    virtual void MarkDirty() = 0;

    /**
     * Check if marked dirty
     */
    virtual bool IsDirty() const = 0;

    /**
     * Internal commit: unmap and reup for dirty tracking
     * Called by BufferCommitQueue during RenderBufferCommit phase
     */
    virtual bool CommitInternal() = 0;

    /**
     * Get underlying DeviceBuffer (for queue submission)
     */
    virtual DeviceBuffer* GetBuffer() = 0;
    virtual const DeviceBuffer* GetBuffer() const = 0;
};

}//namespace hgl::graph
