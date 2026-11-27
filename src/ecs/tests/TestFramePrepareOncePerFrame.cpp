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
#include <hgl/ecs/support/pipeline/RenderBatchPipeline.h>
// Keep the invalid-pass regression observable even when MSVC optimizes away the invalid read.
#ifdef _MSC_VER
#pragma optimize("", off)
#endif
#include <hgl/ecs/support/cull/GPUCullSystem.h>
#ifdef _MSC_VER
#pragma optimize("", on)
#endif
#include <hgl/ecs/support/line/LineRenderPipeline.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::ecs;

namespace hgl::ecs
{
    class ContextBindingProbeSystem : public System
    {
    public:
        ContextBindingProbeSystem() : System("ContextBindingProbe") {}
    };

    struct RenderPassSetupTestAccess
    {
        static void Run(ECSContext &context, uint32_t slot)
        {
            const bool was_active = context.active;
            context.active = true;
            context.PrepareRenderPassSetup(slot);
            context.active = was_active;
        }
    };

    struct GPUCullBatchLayoutTestAccess
    {
        static void SetCount(GPUCullSystem &system, uint32_t pass, uint32_t count)
        {
            // 分区暂存已抽到 BatchLayoutStaging（分区唯一权威）⇒ 经其提交入口写批数
            system.batch_layout.Commit(pass, count);
        }
    };
}

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestFramePrepareOncePerFrame"));

    ECSContext ctx;
    auto context_probe = ctx.RegisterSystem<ContextBindingProbeSystem>();
    if (!context_probe
        || context_probe->GetCoreContext() != &ctx
        || context_probe->GetContext() != &ctx)
    {
        GLogError(u8"Test Failed: ECSContext system registration must bind core and render contexts explicitly");
        return 1;
    }

    RenderBatchPipeline pipeline;

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
    ctx.GetRenderFrameCache().renderableCount = 7;
    RenderPassSetupTestAccess::Run(ctx, 1);

    if (ctx.GetRenderFrameCache().frame_serial != 1
     || ctx.GetRenderFrameCache().renderableCount != 0)
    {
        GLogError(u8"Test Failed: render pass must reset the cache and advance its serial without a Primitive collector");
        return 1;
    }

    pipeline.PrepareFrame(&ctx);

    if (pipeline.GetPrepareFrameCount() != 2)
    {
        GLogError(u8"Test 1 Failed: 帧号推进后应重新准备（累计=%u，期望 2）",
                  pipeline.GetPrepareFrameCount());
        return 1;
    }

    // The same render-target slot can be reused by an offscreen pass.
    RenderPassSetupTestAccess::Run(ctx, 1);
    pipeline.PrepareFrame(&ctx);
    if (ctx.GetRenderFrameCache().frame_serial != 2 || pipeline.GetPrepareFrameCount() != 3)
    {
        GLogError(u8"Test Failed: a second pass in the same slot must start a new frame");
        return 1;
    }

    // A failed Line initialization must not make subsequent calls in the same frame succeed.
    LineRenderPipeline line_pipeline(&ctx);
    if (line_pipeline.PrepareFrame() || line_pipeline.PrepareFrame())
    {
        GLogError(u8"Test Failed: unsuccessful Line initialization was cached as success");
        return 1;
    }

    GPUCullSystem gpu_cull;
    for (uint32_t pass = 0; pass < GPUCullSystem::kArenaPasses; ++pass)
    {
        gpu_cull.SetBatchLayout(pass, GPUCullSystem::kMaxBatches - 1, 16u + pass * 4u, 32u + pass, 0, 0);
        GPUCullBatchLayoutTestAccess::SetCount(gpu_cull, pass, GPUCullSystem::kMaxBatches);
        uint32_t row = 0;
        uint32_t capacity = 0;
        if (!gpu_cull.GetBatchLayout(pass, GPUCullSystem::kMaxBatches - 1, row, capacity)
            || row != 16u + pass * 4u || capacity != 32u + pass
            || gpu_cull.GetBatchLayout(pass, GPUCullSystem::kMaxBatches, row, capacity))
        {
            GLogError(u8"Test Failed: batch layout must respect each pass and batch limit");
            return 1;
        }
        GPUCullBatchLayoutTestAccess::SetCount(gpu_cull, pass, 0);
    }

    uint32_t row = 17;
    uint32_t capacity = 19;
    if (gpu_cull.GetBatchLayout(GPUCullSystem::kArenaPasses, 0, row, capacity)
        || gpu_cull.GetBatchLayout(UINT32_MAX, 0, row, capacity)
        || row != 17 || capacity != 19)
    {
        GLogError(u8"Test Failed: invalid pass must be rejected without changing outputs");
        return 1;
    }

    if (!gpu_cull.Initialize(&ctx)
        || gpu_cull.EndBatchLayout(0, 1)
        || gpu_cull.GetBatchCount() != 0)
    {
        GLogError(u8"Test Failed: batch layout must reject a missing tag-write transaction");
        return 1;
    }

    // 新帧内再调两次仍不重做
    pipeline.PrepareFrame(&ctx);
    pipeline.PrepareFrame(&ctx);

    if (pipeline.GetPrepareFrameCount() != 3)
    {
        GLogError(u8"Test 1 Failed: 新帧内重复调用仍应复用（累计=%u，期望 3）",
                  pipeline.GetPrepareFrameCount());
        return 1;
    }

    // null 入参：安全返回 false 且不计数
    if (pipeline.PrepareFrame(nullptr))
    {
        GLogError(u8"Test 1 Failed: null 入参不该返回 true");
        return 1;
    }

    if (pipeline.GetPrepareFrameCount() != 3)
    {
        GLogError(u8"Test 1 Failed: null 入参被计数（累计=%u）", pipeline.GetPrepareFrameCount());
        return 1;
    }

    GLogInfo(u8"Test 1 Passed: PrepareFrame 是每帧一次的真守卫（帧号推进才重做；null 入参不计数）。");
    return 0;
}
