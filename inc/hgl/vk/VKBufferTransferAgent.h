#pragma once

#include<hgl/vk/VK.h>

namespace hgl::graph
{
class StagedBuffer;

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

class StagedBufferTransferAgent final : public BufferTransferAgent
{
private:
    StagedBuffer *staged_buffer = nullptr;
    VkDeviceSize staged_map_offset = 0;
    VkDeviceSize staged_map_size = 0;
    bool staged_map_active = false;

public:
    explicit StagedBufferTransferAgent(StagedBuffer *sb);
    ~StagedBufferTransferAgent() override;

    bool HasPendingUpload() const override;
    void *Map(VkDeviceSize start, VkDeviceSize size) override;
    void Unmap() override;
    void Flush(VkDeviceSize start, VkDeviceSize size) override;
    bool Write(const void *ptr, uint32_t start, uint32_t size) override;
};
}//namespace hgl::graph
