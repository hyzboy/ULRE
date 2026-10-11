// TestFramePrepareOncePerFrame.cpp —— §2 #11 判据：`PrepareFrame` 必须是**每帧一次**的真守卫
//
// 背景：`PrimitiveRenderPipeline::RunCull / RunSort / RunBuild` 各自调用 `impl_->PrepareFrame(ctx)`，
//       原实现每帧最多做三遍同样的帧前准备（其中 Sort 那遍没有任何下游动作）。
//       2026-10-10 删掉的 `prepared_frame_index` 只有"读一次再写回"、没有 early return ⇒ 是**假守卫**；
//       本用例钉的是真守卫：帧号 + 结果缓存（同帧后续调用直接复用，帧号推进后才重做）。
//
// 判据（红的样子）：同帧 3 次调用后 `GetPrepareFrameCount()` != 1；
//                  帧号推进后 != 2；null 入参被计数。
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/support/PrimitiveBatchPipeline.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::ecs;

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestFramePrepareOncePerFrame"));

    ECSContext ctx;
    PrimitiveBatchPipeline pipeline;

    // 同帧三次（模拟 Cull / Sort / Build 三口各自调用）
    pipeline.PrepareFrame(&ctx);
    pipeline.PrepareFrame(&ctx);
    pipeline.PrepareFrame(&ctx);

    if (pipeline.GetPrepareFrameCount() != 1)
    {
        GLogError(u8"Test 1 Failed: 同帧 3 次 PrepareFrame 实际准备 %u 次（期望 1 —— 缺每帧守卫）",
                  pipeline.GetPrepareFrameCount());
        return 1;
    }

    // 帧号推进 ⇒ 必须重新准备一次
    ctx.SetFrameIndex(1);

    pipeline.PrepareFrame(&ctx);

    if (pipeline.GetPrepareFrameCount() != 2)
    {
        GLogError(u8"Test 1 Failed: 帧号推进后应重新准备（累计=%u，期望 2）",
                  pipeline.GetPrepareFrameCount());
        return 1;
    }

    // 新帧内再调两次仍不重做
    pipeline.PrepareFrame(&ctx);
    pipeline.PrepareFrame(&ctx);

    if (pipeline.GetPrepareFrameCount() != 2)
    {
        GLogError(u8"Test 1 Failed: 新帧内重复调用仍应复用（累计=%u，期望 2）",
                  pipeline.GetPrepareFrameCount());
        return 1;
    }

    // null 入参：安全返回 false 且不计数
    if (pipeline.PrepareFrame(nullptr))
    {
        GLogError(u8"Test 1 Failed: null 入参不该返回 true");
        return 1;
    }

    if (pipeline.GetPrepareFrameCount() != 2)
    {
        GLogError(u8"Test 1 Failed: null 入参被计数（累计=%u）", pipeline.GetPrepareFrameCount());
        return 1;
    }

    GLogInfo(u8"Test 1 Passed: PrepareFrame 是每帧一次的真守卫（帧号推进才重做；null 入参不计数）。");
    return 0;
}
