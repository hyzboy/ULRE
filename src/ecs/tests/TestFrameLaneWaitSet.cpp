#include <hgl/ecs/core/FrameLaneWaitSet.h>
#include <hgl/log/Log.h>

using namespace hgl;
using namespace hgl::ecs;

namespace
{
    // 仅作不透明句柄：状态机只搬运指针与值，不解引用
    graph::Semaphore *FakeLane(uintptr_t tag)
    {
        return reinterpret_cast<graph::Semaphore *>(tag);
    }
}

int main()
{
    {
        FrameLaneWaitSet waits;

        waits.AddFrameLaneWait(nullptr, 7);
        waits.AddFrameLaneWait(FakeLane(1), 0);
        waits.SetSubmitWaitsFromFrameLanes();

        uint32_t count = 123;
        if (waits.TakeSubmitWaits(count, false) != nullptr || count != 0)
        {
            GLogError(u8"FrameLaneWaitSet: 空 lane / 零 value 必须被忽略");
            return 1;
        }

        GLogInfo(u8"FrameLaneWaitSet: 空 lane / 零 value 忽略通过");
    }

    {
        FrameLaneWaitSet waits;

        waits.AddFrameLaneWait(FakeLane(2), 11);
        waits.AddFrameLaneWait(FakeLane(3), 22);
        waits.SetSubmitWaitsFromFrameLanes();

        uint32_t count = 0;
        const graph::SemaphoreSubmit *submit = waits.TakeSubmitWaits(count, false);

        if (!submit || count != 2
            || submit[0].semaphore != FakeLane(2) || submit[0].value != 11
            || submit[1].semaphore != FakeLane(3) || submit[1].value != 22)
        {
            GLogError(u8"FrameLaneWaitSet: 帧车道提升为提交等待失败");
            return 2;
        }

        // 取走即清空：再次取用应为空
        if (waits.TakeSubmitWaits(count, false) != nullptr || count != 0)
        {
            GLogError(u8"FrameLaneWaitSet: TakeSubmitWaits 必须清空提交列表");
            return 3;
        }

        GLogInfo(u8"FrameLaneWaitSet: 提升与取走清空通过");
    }

    {
        FrameLaneWaitSet waits;

        waits.AddFrameLaneWait(FakeLane(4), 33);
        waits.SetSubmitWaitsFromFrameLanes();

        uint32_t count = 0;
        if (!waits.TakeSubmitWaits(count, true) || count != 1)
        {
            GLogError(u8"FrameLaneWaitSet: 主帧取用应返回本帧累积");
            return 4;
        }

        // 主帧提交即收尾：累积已清，重新提升后应为空
        waits.SetSubmitWaitsFromFrameLanes();
        if (waits.TakeSubmitWaits(count, false) != nullptr || count != 0)
        {
            GLogError(u8"FrameLaneWaitSet: 主帧取用后必须清空帧车道累积");
            return 5;
        }

        GLogInfo(u8"FrameLaneWaitSet: 主帧收尾清空累积通过");
    }

    {
        FrameLaneWaitSet waits;

        const graph::SemaphoreSubmit mixed[3] = {
            {nullptr, 0, 0},
            graph::SemaphoreSubmit::Wait(FakeLane(5), 44),
            {nullptr, 0, 0},
        };
        waits.SetSubmitWaits(mixed, 3);

        uint32_t count = 0;
        const graph::SemaphoreSubmit *submit = waits.TakeSubmitWaits(count, false);
        if (!submit || count != 1 || submit[0].semaphore != FakeLane(5) || submit[0].value != 44)
        {
            GLogError(u8"FrameLaneWaitSet: 显式等待列表必须丢弃空信号量条目");
            return 6;
        }

        GLogInfo(u8"FrameLaneWaitSet: 显式等待列表过滤通过");
    }

    {
        FrameLaneWaitSet waits;

        for (uint32_t i = 0; i < FrameLaneWaitSet::kMaxLaneWaits + 4; ++i)
            waits.AddFrameLaneWait(FakeLane(100 + i), i + 1);

        waits.SetSubmitWaitsFromFrameLanes();

        uint32_t count = 0;
        const graph::SemaphoreSubmit *submit = waits.TakeSubmitWaits(count, false);
        if (!submit || count != FrameLaneWaitSet::kMaxLaneWaits)
        {
            GLogError(u8"FrameLaneWaitSet: 超限车道必须被丢弃（count=%u）", count);
            return 7;
        }

        if (submit[0].value != 1 || submit[count - 1].value != FrameLaneWaitSet::kMaxLaneWaits)
        {
            GLogError(u8"FrameLaneWaitSet: 超限截断必须保留前 %u 条", FrameLaneWaitSet::kMaxLaneWaits);
            return 8;
        }

        GLogInfo(u8"FrameLaneWaitSet: 超限截断通过");
    }

    GLogInfo(u8"=== All FrameLaneWaitSet tests passed successfully! ===");
    return 0;
}