#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/ValueArray.h>

#include<string>
#include<unordered_map>

VK_NAMESPACE_BEGIN

class BufferAccessBase;

/**
 * Unified buffer commit queue with priority ordering, budget control, and deadline enforcement
 *
 * CN: 统一缓存提交队列，支持优先级排序、预算控制、Deadline 执行。
 * EN: Collects BufferAccessBase instances and commits them with priority ordering, budget limits, and deadline checks.
 */
class BufferCommitQueue
{
    ValueArray<BufferAccessBase *> pending_buffers;

    struct BudgetGroupState
    {
        uint64_t frame_bytes = 0;
        uint64_t total_bytes = 0;
        uint64_t frame_limit = 0;  // 0 = unlimited
        uint64_t total_limit = 0;  // 0 = unlimited
    };

    std::unordered_map<std::string, BudgetGroupState> budget_groups;
    
    // Budget tracking
    uint64_t current_frame_bytes = 0;       // Bytes committed in current frame
    uint64_t total_committed_bytes = 0;      // Total bytes committed since creation
    uint32_t current_frame_number = 0;       // Current frame number

    // Per-frame stats
    uint32_t last_pending_count = 0;
    uint32_t last_committed_count = 0;
    uint32_t last_skipped_budget = 0;
    uint32_t last_deadline_forced = 0;

public:
    struct BudgetGroupSnapshot
    {
        uint64_t frame_bytes = 0;
        uint64_t total_bytes = 0;
        uint64_t frame_limit = 0;
        uint64_t total_limit = 0;
    };

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
     * Begin a new frame - resets per-frame budget counters.
     * CN: 开始新帧 - 重置单帧预算计数器。
     */
    void BeginFrame(uint32_t frame_number);

    /**
     * Commit all pending buffers with priority ordering, budget control, and deadline enforcement.
     * CN: 提交所有待处理缓冲区，应用优先级排序、预算控制和 Deadline 检查。
     * 
     * Execution order:
     * 1. Sort by priority (Critical -> High -> Normal -> Low -> Background)
     * 2. For each buffer in priority order:
     *    - Check deadline (FramesSinceUpdate vs maxFrameLatency)
     *    - Apply promotion rules if deadline reached
     *    - Check budget limits (single frame + total)
     *    - Commit if all checks pass
     */
    void CommitAll();

    /**
     * Clear the queue without committing.
     */
    void Clear();

    // Statistics
    uint64_t GetCurrentFrameBytes() const { return current_frame_bytes; }
    uint64_t GetTotalCommittedBytes() const { return total_committed_bytes; }
    uint32_t GetCurrentFrameNumber() const { return current_frame_number; }
    uint32_t GetLastPendingCount() const { return last_pending_count; }
    uint32_t GetLastCommittedCount() const { return last_committed_count; }
    uint32_t GetLastSkippedBudget() const { return last_skipped_budget; }
    uint32_t GetLastDeadlineForced() const { return last_deadline_forced; }

    uint32_t GetBudgetGroupCount() const { return static_cast<uint32_t>(budget_groups.size()); }
    bool GetBudgetGroupStats(const std::string &name, BudgetGroupSnapshot &out) const;

private:
    /**
     * Sort pending buffers by priority (Critical first, Background last).
     */
    void SortByPriority();

    /**
     * Check if committing this buffer would exceed budget limits.
     */
    bool CheckBudget(const BufferAccessBase *accessor);

    /**
     * Check if buffer has reached its deadline (maxFrameLatency exceeded).
     */
    bool ShouldCommitByDeadline(const BufferAccessBase *accessor) const;

    /**
     * Check if buffer should be promoted based on PromotePolicy and system state.
     */
    bool ShouldPromote(const BufferAccessBase *accessor) const;

    const std::string &GetBudgetGroupName(const BufferAccessBase *accessor) const;
    void ResetGroupFrameBudgets();
};

VK_NAMESPACE_END
