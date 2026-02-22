#include<hgl/vk/VKBufferCommitQueue.h>
#include<hgl/vk/VKBufferAccessBase.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/BufferPolicyImpl.h>
#include<algorithm>

namespace hgl::graph{

BufferCommitQueue::BufferCommitQueue(const AllDeviceBufferPolicies *policies)
    : device_policies(policies)
{
}

void BufferCommitQueue::SetPolicies(const AllDeviceBufferPolicies *policies)
{
    device_policies = policies;
}

void BufferCommitQueue::Add(BufferAccessBase *accessor)
{
    if(!accessor)
        return;

    for(int i = 0; i < pending_buffers.GetCount(); ++i)
    {
        if(pending_buffers[i] == accessor)
            return;
    }

    // Automatically apply appropriate policy from device policies if available
    if(device_policies)
    {
        DeviceBuffer *buf = accessor->GetBuffer();
        if(buf)
        {
            BufferUpdateClass update_class = buf->GetUpdateClass();
            const BufferPolicy *policy = device_policies->GetPolicy(update_class);
            if(policy)
            {
                buf->SetPolicy(*policy);
            }
        }
    }

    pending_buffers.Add(accessor);
}

void BufferCommitQueue::BeginFrame(uint32_t frame_number)
{
    current_frame_number = frame_number;
    current_frame_bytes = 0;  // Reset per-frame budget

    last_pending_count = 0;
    last_committed_count = 0;
    last_skipped_budget = 0;
    last_deadline_forced = 0;

    ResetGroupFrameBudgets();
}

const std::string &BufferCommitQueue::GetBudgetGroupName(const BufferAccessBase *accessor) const
{
    static const std::string kDefaultGroup = "GLOBAL";

    if(!accessor)
        return kDefaultGroup;

    const DeviceBuffer *buf = accessor->GetBuffer();
    if(!buf)
        return kDefaultGroup;

    const std::string &group = buf->GetBudgetGroup();
    return group.empty() ? kDefaultGroup : group;
}

void BufferCommitQueue::ResetGroupFrameBudgets()
{
    for(auto &kv : budget_groups)
    {
        kv.second.frame_bytes = 0;
    }
}

bool BufferCommitQueue::GetBudgetGroupStats(const std::string &name, BudgetGroupSnapshot &out) const
{
    auto it = budget_groups.find(name);
    if(it == budget_groups.end())
        return false;

    out.frame_bytes = it->second.frame_bytes;
    out.total_bytes = it->second.total_bytes;
    out.frame_limit = it->second.frame_limit;
    out.total_limit = it->second.total_limit;
    return true;
}

void BufferCommitQueue::SortByPriority()
{
    if(pending_buffers.GetCount() <= 1)
        return;

    // Bubble sort by priority (Critical=0 first, Background=4 last)
    for(int i = 0; i < pending_buffers.GetCount() - 1; ++i)
    {
        for(int j = 0; j < pending_buffers.GetCount() - i - 1; ++j)
        {
            BufferAccessBase *a = pending_buffers[j];
            BufferAccessBase *b = pending_buffers[j + 1];

            if(!a || !b)
                continue;

            DeviceBuffer *buf_a = a->GetBuffer();
            DeviceBuffer *buf_b = b->GetBuffer();

            if(!buf_a || !buf_b)
                continue;

            // Lower priority value = higher urgency (Critical=0, Background=4)
            if(static_cast<int>(buf_a->GetPriority()) > static_cast<int>(buf_b->GetPriority()))
            {
                pending_buffers[j] = b;
                pending_buffers[j + 1] = a;
            }
        }
    }
}

bool BufferCommitQueue::CheckBudget(const BufferAccessBase *accessor)
{
    if(!accessor)
        return false;

    const DeviceBuffer *buf = accessor->GetBuffer();
    if(!buf)
        return false;

    const uint64_t buf_size = buf->GetSize();
    const VkDeviceSize budget_limit = buf->GetBudgetLimit();

    const std::string &group_name = GetBudgetGroupName(accessor);
    BudgetGroupState &group = budget_groups[group_name];

    if(group.frame_limit == 0 && budget_limit > 0)
    {
        group.frame_limit = budget_limit;
    }
    else if(group.frame_limit > 0 && budget_limit > 0 && group.frame_limit != budget_limit)
    {
        group.frame_limit = std::min(group.frame_limit, static_cast<uint64_t>(budget_limit));
    }

    // Check buffer-level budget (0 = unlimited)
    if(budget_limit > 0)
    {
        if(buf_size > static_cast<uint64_t>(budget_limit))
            return false;
    }

    // Check group frame budget (0 = unlimited)
    if(group.frame_limit > 0)
    {
        if(group.frame_bytes + buf_size > group.frame_limit)
            return false;
    }

    // Check group total budget (0 = unlimited)
    if(group.total_limit > 0)
    {
        if(group.total_bytes + buf_size > group.total_limit)
            return false;
    }

    return true;
}

bool BufferCommitQueue::ShouldCommitByDeadline(const BufferAccessBase *accessor) const
{
    if(!accessor)
        return false;

    const DeviceBuffer *buf = accessor->GetBuffer();
    if(!buf)
        return false;

    const uint32_t max_latency = buf->GetMaxLatency();
    if(max_latency == 0)  // 0 = no deadline
        return false;

    const uint32_t frames_since_update = buf->GetFramesSinceUpdate();

    // Check deadline policy
    const BufferDeadlinePolicy deadline_policy = buf->GetDeadlinePolicy();

    switch(deadline_policy)
    {
        case BufferDeadlinePolicy::HARD:
            // Must commit if deadline reached
            return frames_since_update >= max_latency;

        case BufferDeadlinePolicy::SOFT:
            // Commit if deadline reached and budget allows
            return frames_since_update >= max_latency;

        case BufferDeadlinePolicy::NONE:
            // No deadline enforcement
            return false;

        default:
            return false;
    }
}

bool BufferCommitQueue::ShouldPromote(const BufferAccessBase *accessor) const
{
    if(!accessor)
        return false;

    const DeviceBuffer *buf = accessor->GetBuffer();
    if(!buf)
        return false;

    const BufferPromotePolicy promote_policy = buf->GetPromotePolicy();

    switch(promote_policy)
    {
        case BufferPromotePolicy::NONE:
            return false;

        case BufferPromotePolicy::AUTO_RAISE:
            // Promote if system appears idle (current frame usage is low)
            // For now, always allow promotion when auto-raise policy is set
            return true;

        case BufferPromotePolicy::FORCE_HIGH:
            return true;

        default:
            return false;
    }
}

void BufferCommitQueue::CommitAll()
{
    if(pending_buffers.GetCount() <= 0)
        return;

    last_pending_count = pending_buffers.GetCount();
    last_committed_count = 0;
    last_skipped_budget = 0;
    last_deadline_forced = 0;

    // Step 1: Sort by priority
    SortByPriority();

    // Step 2: Commit buffers in priority order with budget/deadline checks
    for(int i = 0; i < pending_buffers.GetCount(); ++i)
    {
        BufferAccessBase *accessor = pending_buffers[i];
        if(!accessor)
            continue;

        DeviceBuffer *buf = accessor->GetBuffer();
        if(!buf)
            continue;

        const std::string &group_name = GetBudgetGroupName(accessor);
        BudgetGroupState &group = budget_groups[group_name];

        const bool has_pending_upload = buf->HasStagedDirty();
        if(has_pending_upload)
        {
            accessor->Update();

            const uint64_t buf_size = buf->GetSize();
            current_frame_bytes += buf_size;
            total_committed_bytes += buf_size;

            group.frame_bytes += buf_size;
            group.total_bytes += buf_size;

            last_committed_count++;
            continue;
        }

        // Check if buffer has reached deadline
        const bool deadline_reached = ShouldCommitByDeadline(accessor);
        const BufferDeadlinePolicy deadline_policy = buf->GetDeadlinePolicy();

        // For Hard deadline, bypass budget check
        if(deadline_reached && deadline_policy == BufferDeadlinePolicy::HARD)
        {
            accessor->Update();

            const uint64_t buf_size = buf->GetSize();
            current_frame_bytes += buf_size;
            total_committed_bytes += buf_size;

            group.frame_bytes += buf_size;
            group.total_bytes += buf_size;

            last_committed_count++;
            last_deadline_forced++;

            // Frame counter reset is handled by Update()
            continue;
        }

        // Check promotion rules
        const bool should_promote = ShouldPromote(accessor);

        // Check budget limits
        const bool budget_ok = CheckBudget(accessor);

        // Decide whether to commit
        bool should_commit = false;

        if(deadline_reached)
        {
            // Deadline reached - commit if budget allows (Flexible/BestEffort)
            should_commit = budget_ok;
        }
        else if(should_promote)
        {
            // Promotion rule suggests commit
            should_commit = budget_ok;
        }
        else
        {
            // Normal priority - commit if Critical/High priority and budget allows
            const BufferPriority priority = buf->GetPriority();
            should_commit = (priority == BufferPriority::CRITICAL ||
                           priority == BufferPriority::HIGH) && budget_ok;
        }

        // Perform commit if conditions are met
        if(should_commit)
        {
            accessor->Update();

            const uint64_t buf_size = buf->GetSize();
            current_frame_bytes += buf_size;
            total_committed_bytes += buf_size;

            group.frame_bytes += buf_size;
            group.total_bytes += buf_size;

            last_committed_count++;

            // Frame counter reset is handled by Update()
        }
        else
        {
            // Increment frame counter if buffer was skipped
            buf->IncrementFramesSinceUpdate();

            if(!budget_ok)
                last_skipped_budget++;
        }
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

}//namespace hgl::graph
