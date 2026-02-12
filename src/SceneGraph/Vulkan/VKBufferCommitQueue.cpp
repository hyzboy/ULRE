#include<hgl/graph/VKBufferCommitQueue.h>
#include<hgl/graph/VKBufferAccessBase.h>

VK_NAMESPACE_BEGIN

void BufferCommitQueue::Add(BufferAccessBase *accessor)
{
    if(!accessor)
        return;

    for(int i = 0; i < pending_buffers.GetCount(); ++i)
    {
        if(pending_buffers[i] == accessor)
            return;
    }

    pending_buffers.Add(accessor);
}

void BufferCommitQueue::CommitAll()
{
    if(pending_buffers.GetCount() <= 0)
        return;

    for(int i = 0; i < pending_buffers.GetCount(); ++i)
    {
        BufferAccessBase *accessor = pending_buffers[i];
        if(accessor)
            accessor->Update();
    }

}

void BufferCommitQueue::Remove(BufferAccessBase *accessor)
{
    if(!accessor || pending_buffers.GetCount() <= 0)
        return;

    for(int i = 0; i < pending_buffers.GetCount(); ++i)
    {
        if(pending_buffers[i] == accessor)
        {
            pending_buffers.Delete(i);
            return;
        }
    }
}

void BufferCommitQueue::Clear()
{
    pending_buffers.Clear();
}

VK_NAMESPACE_END
