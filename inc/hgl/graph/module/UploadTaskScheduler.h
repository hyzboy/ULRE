#pragma once

#include<hgl/graph/module/GpuUploadTask.h>
#include<hgl/type/OrderedSet.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/type/ValueArray.h>

namespace hgl::graph
{
    /**
     * 上传任务的调度与状态机（`GpuUploadQueue` 私有，不含任何 Vulkan 调用）。
     *
     * 只管三张表与一笔预算账：
     * - **就绪队列**（按四级优先级分桶）、**在飞表**、**完成表**；
     * - **任务表**（id → 任务）与**已完成 id 历史**（任务对象被提取/回收后，
     *   `State(id)` 仍要能答 "Completed"）；
     * - **staging 预算**（`current <= max` 的背压判据）。
     *
     * 任务对象本身的**生命周期**（delete、staging 缓冲/环形区归还）不在本类：
     * 任务带着 GPU 资源，只能由持有设备的 `GpuUploadQueue` 释放。
     * 本类只负责"它在哪张表里""它是什么状态""预算记了多少"。
     */
    class UploadTaskScheduler
    {
    public:
        static constexpr size_t kPriorityCount = static_cast<size_t>(UploadPriority::Count);

    private:
        ValueArray<GpuUploadTask *> queued_tasks_[kPriorityCount];
        ValueArray<GpuUploadTask *> in_flight_tasks;
        ValueArray<GpuUploadTask *> completed_tasks;

        UnorderedMap<uint64_t, GpuUploadTask *> all_tasks;
        OrderedSet<uint64_t> completed_task_ids;

        uint64_t next_task_id = 1;

        uint64_t max_staging_bytes = 128ULL * 1024 * 1024;
        uint64_t current_staging_bytes = 0;

        bool shutting_down = false;

        static size_t BucketOf(UploadPriority priority)
        {
            const size_t index = static_cast<size_t>(priority);
            return index < kPriorityCount ? index : static_cast<size_t>(UploadPriority::Normal);
        }

    public:
        // ── 关机标记：置位后拒绝新入队 ──
        bool IsShuttingDown() const { return shutting_down; }
        void BeginShutdown() { shutting_down = true; }

        uint64_t NextTaskID() { return next_task_id++; }

        /// 登记进任务表（入队/派发前调用一次）
        void Register(GpuUploadTask *task)
        {
            if (task)
                all_tasks.Add(task->task_id, task);
        }

        /// 放进对应优先级的就绪队列尾部
        void PushQueued(GpuUploadTask *task)
        {
            if (task)
                queued_tasks_[BucketOf(task->priority)].Add(task);
        }

        /// 就绪队列该优先级的队首（空 ⇒ nullptr）
        GpuUploadTask *QueuedHead(size_t priority_index)
        {
            if (priority_index >= kPriorityCount || queued_tasks_[priority_index].IsEmpty())
                return nullptr;

            return queued_tasks_[priority_index][0];
        }

        /// 弹出就绪队列该优先级的队首
        void PopQueued(size_t priority_index)
        {
            if (priority_index < kPriorityCount && !queued_tasks_[priority_index].IsEmpty())
                queued_tasks_[priority_index].Delete(0);
        }

        int QueuedCount(size_t priority_index) const
        {
            return priority_index < kPriorityCount ? queued_tasks_[priority_index].GetCount() : 0;
        }

        /// 从所有优先级队列里摘掉该任务（同步派发路径用）；找到并摘掉 ⇒ true
        bool RemoveFromQueues(GpuUploadTask *task)
        {
            if (!task)
                return false;

            for (size_t p = 0; p < kPriorityCount; ++p)
            {
                const int index = queued_tasks_[p].Find(task);
                if (index >= 0)
                {
                    queued_tasks_[p].Delete(index);
                    return true;
                }
            }

            return false;
        }

        /// 放回队首（保持同优先级内的原始先后次序，下一轮必定重试）
        void DeferToHead(GpuUploadTask *task)
        {
            if (task)
                queued_tasks_[BucketOf(task->priority)].Insert(0, task);
        }

        void MarkInFlight(GpuUploadTask *task)
        {
            if (task)
                in_flight_tasks.Add(task);
        }

        /// 进完成表（`on_complete` 是否已调用由调用方决定）
        void PushCompleted(GpuUploadTask *task)
        {
            if (task)
                completed_tasks.Add(task);
        }

        /// 从在飞表摘掉该任务（按对象查找，保序）；未找到 ⇒ false
        bool RemoveFromInFlight(GpuUploadTask *task)
        {
            if (!task)
                return false;

            const int index = in_flight_tasks.Find(task);
            if (index < 0)
                return false;

            in_flight_tasks.Delete(index);
            return true;
        }

        /// 摘出在飞表（按下标；调用方负责倒序遍历）
        void RemoveInFlight(int index)
        {
            if (index >= 0 && index < in_flight_tasks.GetCount())
                in_flight_tasks.Delete(index);
        }

        /// 从任务表注销（任务对象由调用方随后销毁）
        void Unregister(GpuUploadTask *task)
        {
            if (task)
                all_tasks.DeleteByKey(task->task_id);
        }

        /// 按 id 查任务（不含已完成历史的回收对象）
        GpuUploadTask *Find(uint64_t task_id)
        {
            GpuUploadTask **slot = all_tasks.GetValuePointer(task_id);
            return (slot && *slot) ? *slot : nullptr;
        }

        /// 状态查询：任务表里问对象；已提取/已回收的 id 从完成历史答 Completed；其余 Cancelled
        UploadTaskState State(uint64_t task_id)
        {
            if (GpuUploadTask *task = Find(task_id))
                return task->state;

            if (completed_task_ids.Contains(task_id))
                return UploadTaskState::Completed;

            return UploadTaskState::Cancelled;
        }

        /// 撤销：就绪 ⇒ Cancelled；在飞 ⇒ Discarded（GPU 完成后静默丢弃）；其余 ⇒ false
        bool Cancel(uint64_t task_id)
        {
            GpuUploadTask *task = Find(task_id);
            if (!task)
                return false;

            if (task->state == UploadTaskState::Queued)
            {
                task->state = UploadTaskState::Cancelled;
                return true;
            }

            if (task->state == UploadTaskState::InFlight)
            {
                task->state = UploadTaskState::Discarded;
                return true;
            }

            return false;
        }

        // ── staging 预算 ──
        void SetMaxStagingBytes(uint64_t bytes) { max_staging_bytes = bytes; }
        uint64_t GetMaxStagingBytes() const { return max_staging_bytes; }
        uint64_t GetCurrentStagingBytes() const { return current_staging_bytes; }

        void AddStagingBytes(uint64_t bytes) { current_staging_bytes += bytes; }

        void ReleaseStagingBytes(uint64_t bytes)
        {
            current_staging_bytes = (current_staging_bytes >= bytes)
                                  ? current_staging_bytes - bytes : 0;
        }

        /// 背压判据：预算已用且再加该任务会超限 ⇒ 暂缓派发
        bool WouldExceedBudget(const GpuUploadTask *task) const
        {
            return task
                && current_staging_bytes > 0
                && current_staging_bytes + task->staging_bytes > max_staging_bytes;
        }

        // ── 完成提取 ──
        void ExtractCompleted(ValueArray<GpuUploadTask *> &out_completed)
        {
            out_completed.Clear();

            for (int i = 0; i < completed_tasks.GetCount(); ++i)
            {
                GpuUploadTask *task = completed_tasks[i];
                if (!task)
                    continue;

                out_completed.Add(task);
                completed_task_ids.Add(task->task_id);
                all_tasks.DeleteByKey(task->task_id);
            }

            completed_tasks.Clear();
        }

        // ── 表视图（生命周期归调用方：关机/析构时逐表取消与销毁） ──
        ValueArray<GpuUploadTask *> &QueuedTasks(size_t priority_index) { return queued_tasks_[priority_index]; }
        ValueArray<GpuUploadTask *> &InFlightTasks() { return in_flight_tasks; }
        ValueArray<GpuUploadTask *> &CompletedTasks() { return completed_tasks; }

        void ClearTaskTable() { all_tasks.Clear(); }
    };
}