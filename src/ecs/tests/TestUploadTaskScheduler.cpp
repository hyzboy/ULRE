#include <hgl/graph/module/UploadTaskScheduler.h>
#include <hgl/log/Log.h>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    /// 造一个只用于调度状态的空任务（不含任何 GPU 资源，故可不经队列销毁）
    GpuUploadTask MakeTask(uint64_t id, UploadPriority priority)
    {
        GpuUploadTask task;
        task.task_id = id;
        task.priority = priority;
        task.state = UploadTaskState::Queued;
        return task;
    }
}

int main()
{
    {
        UploadTaskScheduler s;

        if (s.NextTaskID() != 1u || s.NextTaskID() != 2u)
        {
            GLogError(u8"UploadTaskScheduler: 任务 id 必须从 1 起单调递增");
            return 1;
        }

        if (s.IsShuttingDown())
        {
            GLogError(u8"UploadTaskScheduler: 初值不得是关机态");
            return 2;
        }

        s.BeginShutdown();

        if (!s.IsShuttingDown())
        {
            GLogError(u8"UploadTaskScheduler: BeginShutdown 后必须是关机态");
            return 3;
        }

        GLogInfo(u8"UploadTaskScheduler: id 分配与关机标记通过");
    }

    {
        UploadTaskScheduler s;

        GpuUploadTask a = MakeTask(s.NextTaskID(), UploadPriority::High);
        s.Register(&a);

        if (s.Find(a.task_id) != &a)
        {
            GLogError(u8"UploadTaskScheduler: Register 后必须能按 id 找到");
            return 4;
        }

        s.PushQueued(&a);

        if (s.QueuedCount(static_cast<size_t>(UploadPriority::High)) != 1
         || s.QueuedHead(static_cast<size_t>(UploadPriority::High)) != &a)
        {
            GLogError(u8"UploadTaskScheduler: 入队必须落在对应优先级桶");
            return 5;
        }

        // 撤销：就绪 ⇒ Cancelled
        if (!s.Cancel(a.task_id) || a.state != UploadTaskState::Cancelled)
        {
            GLogError(u8"UploadTaskScheduler: 就绪任务撤销必须置 Cancelled");
            return 6;
        }

        // 在飞 ⇒ Discarded
        a.state = UploadTaskState::InFlight;
        if (!s.Cancel(a.task_id) || a.state != UploadTaskState::Discarded)
        {
            GLogError(u8"UploadTaskScheduler: 在飞任务撤销必须置 Discarded");
            return 7;
        }

        // 其它状态 ⇒ 不接受
        a.state = UploadTaskState::Completed;
        if (s.Cancel(a.task_id))
        {
            GLogError(u8"UploadTaskScheduler: 已完成任务不得再撤销");
            return 8;
        }

        GLogInfo(u8"UploadTaskScheduler: 入队与撤销三态通过");
    }

    {
        UploadTaskScheduler s;

        GpuUploadTask a = MakeTask(s.NextTaskID(), UploadPriority::Normal);
        s.Register(&a);

        // 状态查询：表内问对象；提取后问历史；未知 id ⇒ Cancelled
        a.state = UploadTaskState::Queued;
        if (s.State(a.task_id) != UploadTaskState::Queued)
        {
            GLogError(u8"UploadTaskScheduler: 表内任务应返回自身状态");
            return 9;
        }

        s.PushCompleted(&a);

        ValueArray<GpuUploadTask *> extracted;
        s.ExtractCompleted(extracted);

        if (extracted.GetCount() != 1 || extracted[0] != &a)
        {
            GLogError(u8"UploadTaskScheduler: 提取必须移出完成表全部任务");
            return 10;
        }

        if (s.State(a.task_id) != UploadTaskState::Completed)
        {
            GLogError(u8"UploadTaskScheduler: 已提取的 id 必须从历史答 Completed");
            return 11;
        }

        if (s.Find(a.task_id) != nullptr)
        {
            GLogError(u8"UploadTaskScheduler: 提取后任务必须移出任务表");
            return 12;
        }

        if (s.State(UINT64_MAX) != UploadTaskState::Cancelled)
        {
            GLogError(u8"UploadTaskScheduler: 未知 id 必须答 Cancelled");
            return 13;
        }

        GLogInfo(u8"UploadTaskScheduler: 状态查询与完成提取通过");
    }

    {
        UploadTaskScheduler s;

        // 背压：预算未占用时不阻塞；占用后超预算则阻塞
        GpuUploadTask small = MakeTask(100, UploadPriority::Normal);
        small.staging_bytes = 60;

        s.SetMaxStagingBytes(100);

        if (s.WouldExceedBudget(&small))
        {
            GLogError(u8"UploadTaskScheduler: 预算未占用时不得阻塞");
            return 14;
        }

        s.AddStagingBytes(60);

        if (!s.WouldExceedBudget(&small))
        {
            GLogError(u8"UploadTaskScheduler: 60+60 超 100 必须阻塞");
            return 15;
        }

        // 恰好用满不算超
        GpuUploadTask exact = MakeTask(101, UploadPriority::Normal);
        exact.staging_bytes = 40;

        if (s.WouldExceedBudget(&exact))
        {
            GLogError(u8"UploadTaskScheduler: 恰好用满预算不得阻塞");
            return 16;
        }

        // 归还后解除阻塞，且归还不得越过 0
        s.ReleaseStagingBytes(60);
        if (s.GetCurrentStagingBytes() != 0u || s.WouldExceedBudget(&small))
        {
            GLogError(u8"UploadTaskScheduler: 归还后必须回到 0 且不再阻塞");
            return 17;
        }

        s.ReleaseStagingBytes(1000);
        if (s.GetCurrentStagingBytes() != 0u)
        {
            GLogError(u8"UploadTaskScheduler: 过度归还不得下溢");
            return 18;
        }

        GLogInfo(u8"UploadTaskScheduler: staging 背压与归还通过");
    }

    {
        UploadTaskScheduler s;

        GpuUploadTask a = MakeTask(s.NextTaskID(), UploadPriority::Low);
        GpuUploadTask b = MakeTask(s.NextTaskID(), UploadPriority::Low);
        s.Register(&a);
        s.Register(&b);

        s.PushQueued(&a);
        s.PushQueued(&b);

        // DeferToHead：放回队首（下一轮必重试，且不改变同优先级原始次序）
        s.PopQueued(static_cast<size_t>(UploadPriority::Low));
        s.DeferToHead(&a);

        if (s.QueuedHead(static_cast<size_t>(UploadPriority::Low)) != &a)
        {
            GLogError(u8"UploadTaskScheduler: DeferToHead 必须把任务放回队首");
            return 19;
        }

        // RemoveFromQueues：同步派发前把任务摘出队列
        if (!s.RemoveFromQueues(&a) || s.QueuedCount(static_cast<size_t>(UploadPriority::Low)) != 1)
        {
            GLogError(u8"UploadTaskScheduler: RemoveFromQueues 必须只摘掉目标任务");
            return 20;
        }

        // 在飞表的对象移除
        s.MarkInFlight(&a);
        if (s.InFlightTasks().GetCount() != 1)
        {
            GLogError(u8"UploadTaskScheduler: MarkInFlight 必须进在飞表");
            return 21;
        }

        if (!s.RemoveFromInFlight(&a) || s.InFlightTasks().GetCount() != 0)
        {
            GLogError(u8"UploadTaskScheduler: RemoveFromInFlight 必须摘掉目标任务");
            return 22;
        }

        // 注销后不可再找到
        s.Unregister(&b);
        if (s.Find(b.task_id) != nullptr)
        {
            GLogError(u8"UploadTaskScheduler: Unregister 后不得再找到");
            return 23;
        }

        GLogInfo(u8"UploadTaskScheduler: 队首放回/队内摘除/在飞表/注销通过");
    }

    GLogInfo(u8"=== All UploadTaskScheduler tests passed successfully! ===");
    return 0;
}