#pragma once

#include<hgl/vk/VKBufferOwner.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>

namespace hgl::graph{

class VulkanDevice;

/**
 * Buffer access base class (Layer 3)
 *
 * 持有 DeviceBuffer* 和独立缓存的 IGPUBuffer* 两个指针。
 * DeviceBuffer* 用于 GetBuffer()/GetBufferInfo()/descriptor binding / static_cast<VAB*>。
 * IGPUBuffer*  用于所有 CPU 写操作（Map/Write/MarkDirty），直接访问，不经 DeviceBuffer 转发器。
 *
 * 不含任何提交逻辑 —— flush/submit 完全由 ECS RenderBufferUploadSystem 负责。
 */
class BufferAccessBase
{
protected:
    VkBufferOwner *buffer  = nullptr;  // descriptor / GetBuffer() / static_cast — 保留不变
    IGPUBuffer   *gpu_buf = nullptr;  // 写路径专用，SetBuffer() 时同步赋值，直接持有，无需跨层查找

    DescriptorSetType desc_set_type = DescriptorSetType::PerMaterial;
    AnsiString ubo_name;

protected:

    void SetBuffer(VkBufferOwner *buf);

    // Backward-compat overload: stale OBJ files compiled before the VkBufferOwner
    // migration still emit calls to SetBuffer(DeviceBuffer*).  Remove once all
    // dependent libraries have been rebuilt against the new headers.
    void SetBuffer(DeviceBuffer *buf);

    void SetUBOMeta(const DescriptorSetType &dst, const AnsiString &name)
    {
        desc_set_type = dst;
        ubo_name = name;
    }

    void MoveFrom(BufferAccessBase &&other)
    {
        buffer        = other.buffer;
        gpu_buf       = other.gpu_buf;
        desc_set_type = other.desc_set_type;
        ubo_name      = other.ubo_name;

        other.buffer  = nullptr;
        other.gpu_buf = nullptr;
    }

public:
    virtual ~BufferAccessBase() = default;

    BufferAccessBase() = default;
    BufferAccessBase(const BufferAccessBase &) = delete;
    BufferAccessBase &operator=(const BufferAccessBase &) = delete;

    VkBufferOwner *GetBuffer()             { return buffer; }
    const VkBufferOwner *GetBuffer() const { return buffer; }

    /**
     * Returns the cached IGPUBuffer* for CPU writes.
     * Populated by SetBuffer(); nullptr for pure device-local buffers (no upload path).
     */
    IGPUBuffer       *GetGPUBuffer()       { return gpu_buf; }
    const IGPUBuffer *GetGPUBuffer() const { return gpu_buf; }

    bool Write(const void *ptr, uint32_t offset, uint32_t size)
    {
        if(!gpu_buf) return false;
        return gpu_buf->Write(ptr, (VkDeviceSize)offset, (VkDeviceSize)size);
    }

    void Flush(uint32_t size)
    {
        if(gpu_buf)
            gpu_buf->MarkDirty(0, static_cast<VkDeviceSize>(size));
    }

    // Optional update hook for structured accessors.
    virtual void Update() const {}

    // ===== UBO metadata access =====
    const DescriptorSetType &set_type() const { return desc_set_type; }
    const AnsiString &name()            const { return ubo_name; }
    IGPUBuffer *ubo()                   const { return gpu_buf; }
};//class BufferAccessBase

}//namespace hgl::graph