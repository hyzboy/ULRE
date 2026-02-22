#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKBufferWriteAgent.h>

namespace hgl::graph
{
class StagedBuffer;
class DeviceBuffer;

class BufferTransferAgent
{
public:
    virtual ~BufferTransferAgent() = default;

    virtual bool HasPendingUpload() const = 0;
    virtual void *Map(VkDeviceSize start, VkDeviceSize size) = 0;
    virtual void Unmap() = 0;
    virtual void Flush(VkDeviceSize start, VkDeviceSize size) = 0;
    virtual bool Write(const void *ptr, uint32_t start, uint32_t size) = 0;
};

class StagedBufferTransferAgent final : public BufferTransferAgent, public BufferWriteAgent
{
private:
    StagedBuffer *staged_buffer = nullptr;
    DeviceBuffer *device_buffer = nullptr;
    VkDeviceSize staged_map_offset = 0;
    VkDeviceSize staged_map_size = 0;
    bool staged_map_active = false;
    bool is_dirty = false;

public:
    // Legacy constructor for backward compatibility
    explicit StagedBufferTransferAgent(StagedBuffer *sb);
    // New constructor with DeviceBuffer reference
    explicit StagedBufferTransferAgent(StagedBuffer *sb, DeviceBuffer *db);
    ~StagedBufferTransferAgent() override;

    // Set device buffer reference (for late binding)
    void SetDeviceBuffer(DeviceBuffer *db) { device_buffer = db; }

    // BufferTransferAgent interface
    bool HasPendingUpload() const override;
    void *Map(VkDeviceSize start, VkDeviceSize size) override;
    void Unmap() override;
    void Flush(VkDeviceSize start, VkDeviceSize size) override;
    bool Write(const void *ptr, uint32_t start, uint32_t size) override;

    // BufferWriteAgent interface (delegated from BufferTransferAgent)
    void* MapRange(VkDeviceSize offset, VkDeviceSize count) override;
    bool WriteRange(const void *data, VkDeviceSize offset, VkDeviceSize count) override;
    void MarkDirty() override;
    bool IsDirty() const override;
    bool CommitInternal() override;
    DeviceBuffer* GetBuffer() override;
    const DeviceBuffer* GetBuffer() const override;
};
}//namespace hgl::graph
