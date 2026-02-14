#pragma once

#include<hgl/vk/VK.h>
#include<hgl/type/ValueArray.h>

#include<string>
#include<unordered_map>

VK_NAMESPACE_BEGIN

class BufferAccessBase;
struct AllDeviceBufferPolicies;

/**
 * Unified buffer commit queue with priority ordering, budget control, and deadline enforcement
 *
 * CN: ç»ä¸ç¼å­æäº¤éåï¼æ¯æä¼åçº§æåºãé¢ç®æ§å¶ãDeadline æ§è¡ã?
 * EN: Collects BufferAccessBase instances and commits them with priority ordering, budget limits, and deadline checks.
 */
class BufferCommitQueue
{
    ValueArray<BufferAccessBase *> pending_buffers;

    const AllDeviceBufferPolicies *device_policies = nullptr;

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
    BufferCommitQueue(const AllDeviceBufferPolicies *policies);
    ~BufferCommitQueue() = default;

    /**
     * Set buffer policies for automatic policy application.
     * CN: è®¾ç½®ç¼å²ç­ç¥ä»¥èªå¨åºç¨ã?
     */
    void SetPolicies(const AllDeviceBufferPolicies *policies);

    /**
     * Add a buffer accessor to the queue (deduplicated).
     * Automatically applies the appropriate BufferPolicy based on buffer's UpdateClass.
     * CN: å°ç¼å²è®¿é®å¨æ·»å å°éåï¼å»éï¼ãæ ¹æ®ç¼å²çUpdateClassèªå¨åºç¨ç¸åºçBufferPolicyã?
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
     * CN: å¼å§æ°å¸?- éç½®åå¸§é¢ç®è®¡æ°å¨ã?
     */
    void BeginFrame(uint32_t frame_number);

    /**
     * Commit all pending buffers with priority ordering, budget control, and deadline enforcement.
     * CN: æäº¤ææå¾å¤çç¼å²åºï¼åºç¨ä¼åçº§æåºãé¢ç®æ§å¶å Deadline æ£æ¥ã?
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
