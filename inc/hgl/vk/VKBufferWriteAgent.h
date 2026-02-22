#pragma once

#include<hgl/vk/VK.h>

namespace hgl::graph
{
/**
 * CN: 统一缓冲写入代理接口
 * 抽象不同底层实现（StagedBuffer、RingBuffer、ReBar等）的写入差异
 * EN: Unified buffer write agent interface
 * Abstract differences between various backend implementations (StagedBuffer, RingBuffer, ReBar, etc.)
 *
 * 用途 / Usage:
 * - `BufferAccessor<T>` 和 `StructuredBufferAccessor<T>` 通过此接口访问底层存储
 * - 开发者写数据时无感知底层是什么机制
 * - 系统框架负责自动 dirty 追踪和周期性 commit
 *
 * 实现方 / Implementers:
 * - `StagedBufferTransferAgent`: staging buffer + explicit flush
 * - `RingBufferWrapper`: ring buffer for dynamic data
 * - `ReBarDirectWriter`: direct CPU write (future)
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
