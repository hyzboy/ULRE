#pragma once

#include<hgl/vk/VKBuffer.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>

namespace hgl::graph{

class VulkanDevice;

/**
 * Buffer access base class (Layer 3)
 *
 * 只持有 DeviceBuffer* 引用，提供统一的 Write 接口。
 * 不含任何提交逻辑 —— flush/submit 完全由 ECS System 负责。
 *
 * 注意：此层不维护 dirty 标记。GPU 上传 dirty 状态由底层 StagedBuffer::is_dirty
 * 自动管理（Write/Unmap 路径均会自动置脏），ECS RenderBufferUploadSystem 负责轮询。
 * 上层代码无需也不应在 Accessor 层手动追踪 dirty。
 */
class BufferAccessBase
{
protected:
    DeviceBuffer *buffer = nullptr;

    DescriptorSetType desc_set_type = DescriptorSetType::PerMaterial;
    AnsiString ubo_name;

protected:

    void SetBuffer(DeviceBuffer *buf);

    void SetUBOMeta(const DescriptorSetType &dst, const AnsiString &name)
    {
        desc_set_type = dst;
        ubo_name = name;
    }

    void MoveFrom(BufferAccessBase &&other)
    {
        buffer = other.buffer;
        desc_set_type = other.desc_set_type;
        ubo_name      = other.ubo_name;

        other.buffer = nullptr;
    }

public:
    virtual ~BufferAccessBase() = default;

    BufferAccessBase() = default;
    BufferAccessBase(const BufferAccessBase &) = delete;
    BufferAccessBase &operator=(const BufferAccessBase &) = delete;

    DeviceBuffer *GetBuffer()             { return buffer; }
    const DeviceBuffer *GetBuffer() const { return buffer; }

    bool Write(const void *ptr, uint32_t offset, uint32_t size)
    {
        if(!buffer)
            return false;

        return buffer->Write(ptr, offset, size);
    }

    void Flush(uint32_t size)
    {
        if(buffer)
            buffer->Flush(size);
    }

    // Optional update hook for structured accessors.
    virtual void Update() const {}

    // ===== UBO metadata access =====
    const DescriptorSetType &set_type() const { return desc_set_type; }
    const AnsiString &name()            const { return ubo_name; }
    DeviceBuffer *ubo()                 const { return buffer; }
};//class BufferAccessBase

}//namespace hgl::graph