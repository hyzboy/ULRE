#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/ValueArray.h>

VK_NAMESPACE_BEGIN

class BufferAccessBase;

/**
 * Unified buffer commit queue
 *
 * CN: 统一缓存提交队列，收集 BufferAccessBase 并批量提交。
 * EN: Collects BufferAccessBase instances and commits them in batch.
 */
class BufferCommitQueue
{
    ValueArray<BufferAccessBase *> pending_buffers;

public:
    BufferCommitQueue() = default;
    ~BufferCommitQueue() = default;

    /**
     * Add a buffer accessor to the queue (deduplicated).
     */
    void Add(BufferAccessBase *accessor);

    /**
     * Remove a buffer accessor from the queue.
     */
    void Remove(BufferAccessBase *accessor);

    /**
     * Check if there are pending entries.
     */
    bool HasPending() const { return pending_buffers.GetCount() > 0; }

    /**
     * Commit all pending buffers (calls Update on each accessor).
     */
    void CommitAll();

    /**
     * Clear the queue without committing.
     */
    void Clear();
};

VK_NAMESPACE_END
