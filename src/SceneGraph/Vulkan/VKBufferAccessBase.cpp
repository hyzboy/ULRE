#include<hgl/graph/VKBufferAccessBase.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN

void BufferAccessBase::SetBuffer(DeviceBuffer *buf, bool take_ownership)
{
    UnregisterAutoCommit();

    if(owns_buffer && buffer)
        delete buffer;

    buffer = buf;
    owns_buffer = take_ownership;
    dirty = false;

    commit_queue = nullptr;
    if(buffer && auto_commit)
    {
        VulkanDevice *owner = buffer->GetOwnerDevice();
        if(owner)
            commit_queue = owner->GetBufferCommitQueue();
    }

    RegisterAutoCommit();
}

VK_NAMESPACE_END
