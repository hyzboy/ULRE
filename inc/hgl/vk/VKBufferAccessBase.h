#pragma once

#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/VKBufferCommitQueue.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>

namespace hgl::graph{

class VulkanDevice;

/**
 * Buffer access base class
 *
 * CN: æ½è±¡å¯?DeviceBuffer çç»ä¸ç®¡çï¼è´è´£ï¼
 * 1. èµæºæææç®¡ç
 * 2. Dirty æ è®°
 * 3. éç¨ Write/Flush ä»£ç
 * 4. UBO åæ°æ?åç§°/DescriptorSetType)
 *
 * EN: Unified DeviceBuffer management, handling:
 * 1. Ownership
 * 2. Dirty tracking
 * 3. Common Write/Flush proxy
 * 4. UBO metadata (name/DescriptorSetType)
 */
class BufferAccessBase
{
protected:
    DeviceBuffer *buffer = nullptr;
    bool dirty = false;
    bool owns_buffer = false;
    bool auto_commit = true;

    BufferCommitQueue *commit_queue = nullptr;
    DescriptorSetType desc_set_type = DescriptorSetType::PerMaterial;
    AnsiString ubo_name;

protected:

    void RegisterAutoCommit()
    {
        if(auto_commit && commit_queue)
            commit_queue->Add(this);
    }

    void UnregisterAutoCommit()
    {
        if(commit_queue)
            commit_queue->Remove(this);
    }

    void SetBuffer(DeviceBuffer *buf, bool take_ownership);

    void SetUBOMeta(const DescriptorSetType &dst, const AnsiString &name)
    {
        desc_set_type = dst;
        ubo_name = name;
    }

    void MoveFrom(BufferAccessBase &&other)
    {
        other.UnregisterAutoCommit();

        buffer = other.buffer;
        dirty = other.dirty;
        owns_buffer = other.owns_buffer;
        auto_commit = other.auto_commit;
        commit_queue = other.commit_queue;
        desc_set_type = other.desc_set_type;
        ubo_name = other.ubo_name;

        RegisterAutoCommit();

        other.buffer = nullptr;
        other.dirty = false;
        other.owns_buffer = false;
        other.auto_commit = true;
        other.commit_queue = nullptr;
    }

public:
    virtual ~BufferAccessBase()
    {
        UnregisterAutoCommit();
        if(owns_buffer && buffer)
            delete buffer;
    }

    BufferAccessBase() = default;
    BufferAccessBase(const BufferAccessBase &) = delete;
    BufferAccessBase &operator=(const BufferAccessBase &) = delete;

    DeviceBuffer *GetBuffer() { return buffer; }
    const DeviceBuffer *GetBuffer() const { return buffer; }

    void SetAutoCommit(bool enabled)
    {
        if(auto_commit == enabled)
            return;

        auto_commit = enabled;
        if(auto_commit)
            RegisterAutoCommit();
        else
            UnregisterAutoCommit();
    }

    void MarkDirty() { dirty = true; }
    bool IsDirty() const { return dirty; }

    bool Write(const void *ptr, uint32_t offset, uint32_t size)
    {
        if(!buffer)
            return false;

        return buffer->Write(ptr, offset, size);
    }

    // Optional update hook for structured accessors.
    virtual void Update() const {}

    void Flush(uint32_t size)
    {
        if(buffer)
            buffer->Flush(size);
    }

    // ===== UBO metadata access (compatibility with UBOInstance) =====
    const DescriptorSetType &set_type() const { return desc_set_type; }
    const AnsiString &name() const { return ubo_name; }
    DeviceBuffer *ubo() const { return buffer; }
};//class BufferAccessBase


/**
 * Raw buffer accessor for automatic commit of DeviceBuffer
 *
 * CN: ä»ç¨äºèªå¨æäº¤éåï¼ä¸æä¾æ°æ®è®¿é®æ¥å£ã?
 * EN: Used only for auto-commit queue, no data access.
 */
class RawBufferAccessor : public BufferAccessBase
{
public:
    RawBufferAccessor(DeviceBuffer *buf)
    {
        SetBuffer(buf, false);
    }

    void Update() const override
    {
        if (!buffer)
            return;

        const BufferCommitPolicy policy = buffer->GetCommitPolicy();
        const BufferUpdateClass update_class = buffer->GetUpdateClass();
        if(policy == BufferCommitPolicy::Manual)
            return;

        if(buffer->HasStagedDirty())
        {
            buffer->Flush(0);
            const_cast<DeviceBuffer*>(buffer)->ResetFramesSinceUpdate();
            return;
        }

        if(policy == BufferCommitPolicy::Always || update_class == BufferUpdateClass::CriticalPerFrame)
        {
            buffer->Flush(0);
            const_cast<DeviceBuffer*>(buffer)->ResetFramesSinceUpdate();
        }
    }
};//class RawBufferAccessor

}//namespace hgl::graph

