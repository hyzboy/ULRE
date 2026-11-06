// TestSceneAsset — 阶段四 Step 4.1 判据：场景句柄的状态机与句柄表去重
//
// 这些判据全部**不碰设备**：句柄只装状态与计划单，不持有 GPU 资源，因此
// 可以在没有 `GraphicsContext` 的进程里把状态机跑完。真正需要设备的派发
// （读盘 / 建 buffer）由阶段四 Step 4.2 的 `SceneStreamingSystem` 覆盖。
//
// 判据清单：
//   1. 句柄表按 URI 原文去重，空 URI 拒绝，计数接口自洽；
//   2. `Unloaded → Loading → Assembling → Ready` 全链路，且 `BeginStreaming` 防重复；
//   3. 无外部子项时 `SetPlan` 直接提交（不自锁在 `Assembling`）；
//   4. 子项未全部落定时 `CommitScene` 必须拒绝，落定后才提交；
//   5. 子项失败不阻断场景就绪（降级显示），失败计数可观测；
//   6. `Fail` 幂等、拒绝后续 `SetPlan`；`Reset` 可重试且保留尝试次数；
//   7. 记账防线：落定数不得超过登记数，计数不得为负；
//   8. 进程内默认表：未登记时 `RequestSceneAsset` 返回 nullptr。

#include <hgl/graph/asset/SceneAsset.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::graph;

#define EXPECT(cond)                                                            \
    do {                                                                        \
        if(!(cond)) {                                                           \
            GLogError(u8"判据失败：%s（%s:%d）", #cond, __FILE__, __LINE__);    \
            return false;                                                       \
        }                                                                       \
    } while(0)

namespace
{
    SceneLoadTask MakeTask(asset::AssetType type, const char *path,
                           AssetDepKind kind, int ref_count = 1)
    {
        SceneLoadTask task;

        task.key.type = type;
        task.key.SetPath(AnsiString(path));
        task.kind     = kind;
        task.ref_count = ref_count;

        return task;
    }

    /// 3 个子项（2 几何 + 1 纹理）+ 2 份内嵌几何
    SceneLoadPlan MakePlan(int task_count = 3)
    {
        SceneLoadPlan plan;

        if(task_count >= 1)
            plan.tasks.Add(MakeTask(asset::AssetType::Geometry, "chess/board.geo",
                                    AssetDepKind::Geometry));
        if(task_count >= 2)
            plan.tasks.Add(MakeTask(asset::AssetType::Geometry, "chess/pawn.geo",
                                    AssetDepKind::Geometry, 2));
        if(task_count >= 3)
            plan.tasks.Add(MakeTask(asset::AssetType::Texture, "chess/board.ktx2",
                                    AssetDepKind::Texture));

        plan.embedded_geometry_count = 2;
        return plan;
    }

    // ── 通知计数的接收者 ──────────────────────────────────────────────────

    struct NotifyProbe
    {
        int ready_count  = 0;
        int failed_count = 0;
        const char *last_reason = nullptr;
    };

    void OnReady(SceneAsset *, void *user)
    {
        static_cast<NotifyProbe *>(user)->ready_count++;
    }

    void OnFailed(SceneAsset *, const char *reason, void *user)
    {
        NotifyProbe *probe = static_cast<NotifyProbe *>(user);
        probe->failed_count++;
        probe->last_reason = reason;
    }

    // ── 判据 1：句柄表去重 ────────────────────────────────────────────────

    bool TestRegistryDedupe()
    {
        SceneAssetRegistry registry;

        SceneAsset *a = registry.Request("pack(res):ABeautifulGame.StaticMesh/ABeautifulGame.model");
        EXPECT(a != nullptr);
        EXPECT(a->GetState() == SceneAssetState::Unloaded);
        EXPECT(a->NeedsPump());

        // 同一 URI 只应得到一个句柄 —— 这是「同一场景被多个世界请求只读一次盘」的前提。
        SceneAsset *again = registry.Request("pack(res):ABeautifulGame.StaticMesh/ABeautifulGame.model");
        EXPECT(again == a);
        EXPECT(registry.GetCount() == 1);

        SceneAsset *other = registry.Request("pack(res):Chess/Other.Scene.model");
        EXPECT(other != nullptr);
        EXPECT(other != a);
        EXPECT(registry.GetCount() == 2);

        // 空 URI 不许产生句柄（否则去重键会撞在一起）。
        EXPECT(registry.Request(AnsiString()) == nullptr);
        EXPECT(registry.GetCount() == 2);

        EXPECT(registry.GetPendingCount() == 2);
        EXPECT(registry.GetReadyCount() == 0);
        EXPECT(registry.GetFailedCount() == 0);
        EXPECT(registry.GetChildTaskCount() == 0);
        EXPECT(registry.Find("pack(res):Chess/Other.Scene.model") == other);

        registry.Clear();
        EXPECT(registry.GetCount() == 0);

        return true;
    }

    // ── 判据 2：完整状态链路 ──────────────────────────────────────────────

    bool TestFullStateChain()
    {
        SceneAssetRegistry registry;
        SceneAsset *asset = registry.Request("pack(res):Probe/Chain.Scene.model");
        EXPECT(asset != nullptr);

        NotifyProbe probe;
        asset->SetNotifyCallback(OnReady, OnFailed, &probe);

        // 未派发前不能凭空提交。
        EXPECT(!asset->CommitScene());
        EXPECT(!asset->SettleChild(true));

        EXPECT(asset->BeginStreaming(7));
        EXPECT(asset->GetState() == SceneAssetState::Loading);
        EXPECT(asset->IsLoading());
        EXPECT(asset->GetAsyncRequestID() == 7);
        EXPECT(asset->GetAsyncAttempt() == 1);

        // 重复派发必须被挡下（否则同一个包会被读两遍）。
        EXPECT(!asset->BeginStreaming(8));
        EXPECT(asset->GetAsyncRequestID() == 7);

        // 子项没到齐前，CommitScene 必须拒绝。
        EXPECT(!asset->SetPlan(MakePlan(), OS_TEXT("res/Probe/Chain.Scene.model")));
        EXPECT(asset->GetState() == SceneAssetState::Assembling);
        EXPECT(asset->GetChildCount() == 3);
        EXPECT(asset->GetPendingChildCount() == 3);
        EXPECT(!asset->IsReady());
        EXPECT(asset->NeedsPump());

        const SceneAsset::Stats &stats = asset->GetStats();
        EXPECT(stats.total == 3);
        EXPECT(stats.geometry == 2);
        EXPECT(stats.texture == 1);
        EXPECT(stats.material == 0);
        EXPECT(stats.other == 0);
        EXPECT(stats.embedded_geometry == 2);

        EXPECT(registry.GetChildTaskCount() == 3);

        EXPECT(!asset->SettleChild(true));
        EXPECT(asset->GetReadyChildCount() == 1);
        EXPECT(asset->GetPendingChildCount() == 2);

        EXPECT(!asset->SettleChild(true));
        EXPECT(asset->GetReadyChildCount() == 2);

        // 还差一个：会话尚未就绪，通知也不该提前触发。
        EXPECT(!asset->CommitScene());
        EXPECT(probe.ready_count == 0);

        EXPECT(asset->SettleChild(true));
        EXPECT(asset->GetPendingChildCount() == 0);

        EXPECT(asset->CommitScene());
        EXPECT(asset->GetState() == SceneAssetState::Ready);
        EXPECT(asset->IsReady());
        EXPECT(!asset->NeedsPump());
        EXPECT(probe.ready_count == 1);
        EXPECT(probe.failed_count == 0);

        // 就绪后重复提交是幂等的，通知不重复。
        EXPECT(asset->CommitScene());
        EXPECT(probe.ready_count == 1);
        EXPECT(registry.GetReadyCount() == 1);
        EXPECT(registry.GetPendingCount() == 0);

        return true;
    }

    // ── 判据 3：无外部子项直接提交 ────────────────────────────────────────

    bool TestEmptyPlanCommitsImmediately()
    {
        SceneAssetRegistry registry;
        SceneAsset *asset = registry.Request("pack(res):Probe/Empty.Scene.model");
        EXPECT(asset != nullptr);

        NotifyProbe probe;
        asset->SetNotifyCallback(OnReady, OnFailed, &probe);

        EXPECT(asset->BeginStreaming());

        SceneLoadPlan empty;
        empty.embedded_geometry_count = 5;

        // 只有内嵌几何的场景不产生外部依赖：不应自锁在 Assembling 等一个永不来的子项。
        EXPECT(asset->SetPlan(empty));
        EXPECT(asset->GetState() == SceneAssetState::Ready);
        EXPECT(asset->GetStats().embedded_geometry == 5);
        EXPECT(asset->GetChildCount() == 0);
        EXPECT(probe.ready_count == 1);

        return true;
    }

    // ── 判据 4：子项失败不阻断就绪 ────────────────────────────────────────

    bool TestChildFailureDoesNotBlock()
    {
        SceneAssetRegistry registry;
        SceneAsset *asset = registry.Request("pack(res):Probe/Degraded.Scene.model");
        EXPECT(asset != nullptr);

        NotifyProbe probe;
        asset->SetNotifyCallback(OnReady, OnFailed, &probe);

        EXPECT(asset->BeginStreaming());
        EXPECT(!asset->SetPlan(MakePlan(2)));

        EXPECT(!asset->SettleChild(true));
        EXPECT(asset->SettleChild(false));      // 缺一份几何 → 降级，不阻断

        EXPECT(asset->GetReadyChildCount() == 1);
        EXPECT(asset->GetFailedChildCount() == 1);
        EXPECT(asset->GetPendingChildCount() == 0);

        EXPECT(asset->CommitScene());
        EXPECT(asset->IsReady());
        EXPECT(probe.ready_count == 1);
        EXPECT(probe.failed_count == 0);        // 子项失败不触发场景级失败通知
        EXPECT(asset->GetFailReason().IsEmpty());

        return true;
    }

    // ── 判据 5：失败幂等 / 重试 ───────────────────────────────────────────

    bool TestFailAndReset()
    {
        SceneAssetRegistry registry;
        SceneAsset *asset = registry.Request("pack(res):Probe/Broken.Scene.model");
        EXPECT(asset != nullptr);

        NotifyProbe probe;
        asset->SetNotifyCallback(OnReady, OnFailed, &probe);

        EXPECT(asset->BeginStreaming(3));
        asset->Fail("bad MDL2 header");

        EXPECT(asset->HasFailed());
        EXPECT(!asset->IsLoading());
        EXPECT(!asset->NeedsPump());
        EXPECT(probe.failed_count == 1);
        EXPECT(probe.last_reason != nullptr);
        EXPECT(!asset->GetFailReason().IsEmpty());
        EXPECT(registry.GetFailedCount() == 1);
        EXPECT(registry.GetPendingCount() == 0);

        // 失败后不得再被推进（否则坏文件会每帧重试）。
        EXPECT(!asset->SetPlan(MakePlan()));
        EXPECT(!asset->CommitScene());
        asset->Fail("second reason");
        EXPECT(probe.failed_count == 1);         // 幂等，通知不重复

        // 重试：回到 Unloaded，计划与计数清空，尝试次数保留。
        const int attempt_before = asset->GetAsyncAttempt();
        asset->Reset();
        EXPECT(asset->GetState() == SceneAssetState::Unloaded);
        EXPECT(asset->NeedsPump());
        EXPECT(asset->GetChildCount() == 0);
        EXPECT(asset->GetStats().total == 0);
        EXPECT(asset->GetFailReason().IsEmpty());
        EXPECT(asset->GetAsyncAttempt() == attempt_before);

        EXPECT(asset->BeginStreaming(4));
        EXPECT(asset->GetAsyncAttempt() == attempt_before + 1);
        EXPECT(asset->GetAsyncRequestID() == 4);

        return true;
    }

    // ── 判据 6：记账防线 ──────────────────────────────────────────────────

    bool TestOverSettleGuard()
    {
        SceneAssetRegistry registry;
        SceneAsset *asset = registry.Request("pack(res):Probe/OverSettle.Scene.model");
        EXPECT(asset != nullptr);

        EXPECT(asset->BeginStreaming());
        EXPECT(!asset->SetPlan(MakePlan(1)));

        EXPECT(asset->SettleChild(true));
        EXPECT(asset->GetPendingChildCount() == 0);

        // 多出来的落定必须被丢弃，不得把计数递减成负数。
        EXPECT(asset->SettleChild(true));
        EXPECT(asset->GetReadyChildCount() == 1);
        EXPECT(asset->GetFailedChildCount() == 0);
        EXPECT(asset->GetPendingChildCount() == 0);

        EXPECT(asset->CommitScene());
        EXPECT(asset->IsReady());

        return true;
    }

    // ── 判据 7：子项口径可换成「实际图元数」 ──────────────────────────────

    bool TestSetChildTotal()
    {
        SceneAssetRegistry registry;
        SceneAsset *asset = registry.Request("pack(res):Probe/Recount.Scene.model");
        EXPECT(asset != nullptr);

        EXPECT(asset->BeginStreaming());
        EXPECT(!asset->SetPlan(MakePlan(3)));
        EXPECT(asset->GetChildCount() == 3);

        // 解析产物出来后发现实际只有 2 个图元需要等几何：
        // 口径按图元重算，否则会永远差一个子项、场景永不就绪。
        asset->SetChildTotal(2);
        EXPECT(asset->GetChildCount() == 2);
        EXPECT(asset->GetPendingChildCount() == 2);

        // 已有子项落定后不得再改写总数，否则计数错位。
        EXPECT(!asset->SettleChild(true));
        asset->SetChildTotal(9);
        EXPECT(asset->GetChildCount() == 2);

        EXPECT(asset->SettleChild(true));
        EXPECT(asset->CommitScene());
        EXPECT(asset->IsReady());

        return true;
    }

    // ── 判据 9：落定只记账，推状态机必须显式提交 ──────────────────────────

    bool TestSettleDoesNotCommit()
    {
        SceneAssetRegistry registry;
        SceneAsset *asset = registry.Request("pack(res):Probe/SettleOnly.Scene.model");
        EXPECT(asset != nullptr);

        EXPECT(asset->BeginStreaming());
        EXPECT(!asset->SetPlan(MakePlan(2)));
        EXPECT(asset->GetState() == SceneAssetState::Assembling);

        // 一次把两项都落定：计数归零，但状态**仍停在** `Assembling` ——
        // `SettleAllChildren` 只记账。流送系统若落了账不调 `CommitScene()`，
        // 句柄会永远卡在装配中（曾真实发生：示例场景 120 帧都到不了 `Ready`）。
        EXPECT(asset->SettleAllChildren(2, 0));
        EXPECT(asset->GetPendingChildCount() == 0);
        EXPECT(asset->GetState() == SceneAssetState::Assembling);
        EXPECT(!asset->IsReady());

        EXPECT(asset->CommitScene());
        EXPECT(asset->IsReady());

        return true;
    }

    // ── 判据 8：进程内默认表 ──────────────────────────────────────────────

    bool TestDefaultRegistry()
    {
        // 本进程没有创建 GraphicsContext，默认表应当为空。
        EXPECT(GetDefaultSceneAssetRegistry() == nullptr);
        EXPECT(RequestSceneAsset("pack(res):Probe/NoRegistry.Scene.model") == nullptr);

        SceneAssetRegistry registry;
        SetDefaultSceneAssetRegistry(&registry);

        SceneAsset *asset = RequestSceneAsset("pack(res):Probe/Default.Scene.model");
        EXPECT(asset != nullptr);
        EXPECT(asset == registry.Find("pack(res):Probe/Default.Scene.model"));

        SetDefaultSceneAssetRegistry(nullptr);
        EXPECT(GetDefaultSceneAssetRegistry() == nullptr);

        return true;
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    hgl::logger::InitLogger(OS_TEXT("TestSceneAsset"));

    const bool ok = TestRegistryDedupe()
                 && TestFullStateChain()
                 && TestEmptyPlanCommitsImmediately()
                 && TestChildFailureDoesNotBlock()
                 && TestFailAndReset()
                 && TestOverSettleGuard()
                 && TestSetChildTotal()
                 && TestSettleDoesNotCommit()
                 && TestDefaultRegistry();

    if(!ok)
    {
        GLogError(u8"=== TestSceneAsset: 测试失败 ===");
        return 1;
    }

    GLogInfo(u8"=== TestSceneAsset: 全部测试通过 ===");
    return 0;
}
