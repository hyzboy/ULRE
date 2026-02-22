#pragma once

#include<hgl/vk/IGPUBuffer.h>
#include<hgl/vk/VKMemory.h>

namespace hgl::graph{

/**
 * ReBarBuffer — IGPUBuffer implementation for ReBAR / CPUVisible memory.
 *
 * Wraps a single VkBuffer backed by HOST_VISIBLE + DEVICE_LOCAL memory
 * (Resizable BAR / CPU-visible VRAM).  Writes are directly visible to the GPU
 * without any staging copy.  CopyToDevice() is therefore a no-op.
 *
 * Ownership: ReBarBuffer owns both the VkBuffer and the DeviceMemory*.
 * DeviceBuffer holds aliases (buf.buffer / buf.memory) that are valid for
 * the lifetime of the owning ReBarBuffer.
 *
 * Lifecycle: created by VulkanDevice factory functions (VKDeviceBuffer.cpp)
 * and installed via DeviceBuffer::SetStagedSource().  DeviceBuffer destructor
 * deletes the staged_source (this object) which in turn frees the VkBuffer
 * and DeviceMemory — the same pattern as StagedBuffer.
 */
class ReBarBuffer : public IGPUBuffer
{
    VkDevice     device;
    VkBuffer     buffer;
    DeviceMemory *memory;
    VkDeviceSize buf_size;

private:
    friend class VulkanDevice;

    ReBarBuffer(const std::string &name,
                VkDevice          dev,
                VkBuffer          buf,
                DeviceMemory     *mem,
                VkDeviceSize      size)
        : IGPUBuffer(name)
        , device(dev)
        , buffer(buf)
        , memory(mem)
        , buf_size(size)
    {}

public:
    virtual ~ReBarBuffer() override
    {
        // ReBarBuffer owns both the VkBuffer and DeviceMemory
        delete memory;
        if(buffer)
            vkDestroyBuffer(device, buffer, nullptr);
    }

    // ---- IGPUBuffer interface ----

    bool Write(const void *data, VkDeviceSize offset, VkDeviceSize size) override
    {
        return memory ? memory->Write(data, offset, size) : false;
    }

    void *Map(VkDeviceSize offset, VkDeviceSize size) override
    {
        return memory ? memory->Map(offset, size) : nullptr;
    }

    void Unmap() override
    {
        if(memory) memory->Unmap();
    }

    // ReBAR memory is HOST_COHERENT: writes are always GPU-visible.
    // Dirty tracking is a no-op — CopyToDevice() never needs to be called.
    void MarkDirty (VkDeviceSize = 0, VkDeviceSize = VK_WHOLE_SIZE) override {}
    bool IsDirty   () const override { return false; }
    void ClearDirty() override {}

    /** No-op: CPU-visible memory IS device memory — no copy needed. */
    void CopyToDevice(VkCommandBuffer /*cmd*/) override {}

    VkDeviceSize GetSize()           const override { return buf_size; }
    VkBuffer     GetVkDeviceBuffer() const override { return buffer; }

    /** Returns the DeviceMemory owned by this buffer (alias for DeviceBuffer). */
    DeviceMemory *GetDeviceMemory() const { return memory; }

    VkDescriptorBufferInfo GetDescriptorBufferInfo() const override
    {
        VkDescriptorBufferInfo info{};
        info.buffer = buffer;
        info.offset = 0;
        info.range  = buf_size;
        return info;
    }
};//class ReBarBuffer

}//namespace hgl::graph
