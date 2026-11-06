#include<hgl/ecs/support/material/MaterialAuthoringSideTable.h>
#include<hgl/log/Log.h>
#include<hgl/log/Logger.h>

/**
 * 判据：**必需纹理**的就绪结论（`EvaluateRequiredTextureReadiness`）。
 *
 * 为什么值得单独立测：这条规则写错的后果极不对称 ——
 *
 * | 规则写错 | 现场表现 |
 * |----------|----------|
 * | `LoadFailed` 仍算「在途」 | 一个坏贴图 ⇒ 整个图元**永久消失**，画面只少一块，没有任何提示 |
 * | `Loading` 也算「失败」 | 图元用句柄 0 占位**先画出来**，贴图到位后不会自动变好（材质链已 clean） |
 *
 * 而 `TextureStreamingSystem` 对 `LoadFailed` **不自动重试**（坏文件每帧重读只会刷屏），
 * 所以「永久失败」在现场确实是终态，必须由渲染门禁显式降级为「当它不在」，
 * 交给 GLSL 侧 `SampleOptional()` / `if (handle == 0u)` 的 fallback 兜底。
 *
 * 本文件不碰设备：`SetupFromTexture()` 只做状态置位、不解引用纹理指针，因此用一个
 * **哨兵指针**就能造出 `Resident` 句柄；`MarkAsyncRequested()` / `FailAsyncLoad()`
 * 同样只改状态机。
 */

static int g_fail = 0;

#define EXPECT(cond)                                                            \
    do                                                                          \
    {                                                                           \
        if (!(cond))                                                            \
        {                                                                       \
            GLogError(u8"判据失败：%s（%s:%d）", #cond, __FILE__, __LINE__);    \
            ++g_fail;                                                           \
        }                                                                       \
    } while (0)

namespace
{
    using namespace hgl;
    using namespace hgl::ecs;

    /// 只用作「句柄已就绪」的标记值：没有任何代码路径会解引用它
    /// （`IsReady()`/`GetTexture()` 只比较与返回指针）。
    graph::Texture *const kReadySentinel = reinterpret_cast<graph::Texture *>(uintptr_t(1));

    /// 造一个**只有来源、尚未加载**的句柄（`MetaOnly`）
    void MakeMetaOnly(graph::TextureAsset &asset)
    {
        asset.GetMeta().source_path = OS_TEXT("texfile:missing.png");
        asset.GetMeta().asset_type  = OS_TEXT("Texture2D");
    }

    /// 造一个**已就绪**的句柄（`Resident`）
    void MakeReady(graph::TextureAsset &asset)
    {
        MakeMetaOnly(asset);
        asset.SetupFromTexture(kReadySentinel);
    }

    /// 造一个**永久失败**的句柄（`LoadFailed`，流送系统不会重试）
    void MakeFailed(graph::TextureAsset &asset)
    {
        MakeMetaOnly(asset);
        asset.FailAsyncLoad("判据构造的失败样本");
    }

    MaterialTextureAuthoringResource Bind(graph::TextureAsset *asset,
                                         bool required)
    {
        MaterialTextureAuthoringResource resource;
        resource.asset    = asset;
        resource.required = required;
        return resource;
    }

    using Readiness = RequiredTextureReadiness;

    // ── 判据 1：没有旁表条目 / 条目为空 ⇒ 无必需纹理可等 ──────────────────

    void TestNoResources()
    {
        // 无水材质 / 无授权资源：不该把图元钉死。
        EXPECT(EvaluateRequiredTextureReadiness(nullptr) == Readiness::Ready);

        MaterialAuthoringSideTable::Entry empty_entry;
        EXPECT(EvaluateRequiredTextureReadiness(&empty_entry) == Readiness::Ready);
    }

    // ── 判据 2：单个条目的三种结论 ────────────────────────────────────────

    void TestSingleResource()
    {
        graph::TextureAsset meta_only(nullptr);
        graph::TextureAsset ready(nullptr);
        graph::TextureAsset failed(nullptr);

        MakeMetaOnly(meta_only);
        MakeReady(ready);
        MakeFailed(failed);

        // 前提自检：造出来的样本确实是三种状态，否则后面全是空测。
        EXPECT(!meta_only.IsReady() && meta_only.GetPayloadState() == PayloadState::MetaOnly);
        EXPECT(ready.IsReady());
        EXPECT(!failed.IsReady() && failed.GetPayloadState() == PayloadState::LoadFailed);

        {
            MaterialAuthoringSideTable::Entry entry;
            entry.named_texture_resources.Add(AnsiString("base_color"), Bind(&meta_only, true));

            // 已声明来源但还没读到 ⇒ 等它，本帧不渲染（这是原语义，不能退化成占位）。
            EXPECT(EvaluateRequiredTextureReadiness(&entry) == Readiness::Pending);
        }

        {
            MaterialAuthoringSideTable::Entry entry;
            entry.named_texture_resources.Add(AnsiString("base_color"), Bind(&ready, true));
            EXPECT(EvaluateRequiredTextureReadiness(&entry) == Readiness::Ready);
        }

        {
            MaterialAuthoringSideTable::Entry entry;
            entry.named_texture_resources.Add(AnsiString("base_color"), Bind(&failed, true));

            // 核心判据：终态失败 ⇒ 降级占位保底，而不是永久阻断。
            EXPECT(EvaluateRequiredTextureReadiness(&entry) == Readiness::Degraded);
        }

        {
            // 非必需（optional）不参与门禁：失败与否都算 Ready。
            MaterialAuthoringSideTable::Entry entry;
            entry.named_texture_resources.Add(AnsiString("detail"), Bind(&failed, false));
            EXPECT(EvaluateRequiredTextureReadiness(&entry) == Readiness::Ready);
        }

        {
            // 空条目（既无句柄也无文件名）：本来就收集不到绑定，不该影响门禁。
            MaterialAuthoringSideTable::Entry entry;
            entry.named_texture_resources.Add(AnsiString("ghost"), Bind(nullptr, true));
            EXPECT(EvaluateRequiredTextureReadiness(&entry) == Readiness::Ready);
        }

        {
            // 「有句柄但无来源」：`HasSource()` 为假 ⇒ 同样不参与门禁。
            graph::TextureAsset blank(nullptr);
            MaterialAuthoringSideTable::Entry entry;
            entry.named_texture_resources.Add(AnsiString("blank"), Bind(&blank, true));
            EXPECT(!blank.IsReady());
            EXPECT(EvaluateRequiredTextureReadiness(&entry) == Readiness::Ready);
        }
    }

    // ── 判据 3：多条目时「在途」优先于「失败」 ────────────────────────────

    void TestMixedResources()
    {
        graph::TextureAsset loading(nullptr);
        graph::TextureAsset failed_a(nullptr);
        graph::TextureAsset failed_b(nullptr);
        graph::TextureAsset ready(nullptr);

        MakeMetaOnly(loading);
        loading.MarkAsyncRequested(1);      // MetaOnly ⇒ Loading
        MakeFailed(failed_a);
        MakeFailed(failed_b);
        MakeReady(ready);

        EXPECT(loading.GetPayloadState() == PayloadState::Loading);

        {
            // 一张失败挂在另一张在途上：仍应等 —— 在途那张可能马上就好，
            // 现在就画出来会在它到位后继续用旧占位（材质链已 clean，不会自动重算）。
            MaterialAuthoringSideTable::Entry entry;
            entry.named_texture_resources.Add(AnsiString("base_color"), Bind(&failed_a, true));
            entry.named_texture_resources.Add(AnsiString("normal"), Bind(&loading,    true));
            EXPECT(EvaluateRequiredTextureReadiness(&entry) == Readiness::Pending);
        }

        {
            // 全部有结论、且至少一张失败 ⇒ 降级。
            MaterialAuthoringSideTable::Entry entry;
            entry.named_texture_resources.Add(AnsiString("base_color"), Bind(&failed_a, true));
            entry.named_texture_resources.Add(AnsiString("normal"),     Bind(&failed_b, true));
            EXPECT(EvaluateRequiredTextureReadiness(&entry) == Readiness::Degraded);
        }

        {
            // 失败 + 就绪（无在途）⇒ 仍是降级，不是 Ready：不能让失败被就绪掩盖掉。
            MaterialAuthoringSideTable::Entry entry;
            entry.named_texture_resources.Add(AnsiString("base_color"), Bind(&failed_a, true));
            entry.named_texture_resources.Add(AnsiString("normal"),     Bind(&ready,    true));
            EXPECT(EvaluateRequiredTextureReadiness(&entry) == Readiness::Degraded);
        }

        {
            // 非必需的失败不得把整体拖进 Degraded。
            MaterialAuthoringSideTable::Entry entry;
            entry.named_texture_resources.Add(AnsiString("base_color"), Bind(&ready,    true));
            entry.named_texture_resources.Add(AnsiString("detail"),     Bind(&failed_a, false));
            EXPECT(EvaluateRequiredTextureReadiness(&entry) == Readiness::Ready);
        }
    }

    // ── 判据 4：状态迁移后结论必须跟着变（不得缓存） ──────────────────────

    void TestStateTransitions()
    {
        graph::TextureAsset asset(nullptr);
        MakeMetaOnly(asset);

        MaterialAuthoringSideTable::Entry entry;
        entry.named_texture_resources.Add(AnsiString("base_color"), Bind(&asset, true));

        EXPECT(EvaluateRequiredTextureReadiness(&entry) == Readiness::Pending);

        asset.MarkAsyncRequested(7);
        EXPECT(EvaluateRequiredTextureReadiness(&entry) == Readiness::Pending);

        asset.FailAsyncLoad("判据构造的失败样本");
        EXPECT(EvaluateRequiredTextureReadiness(&entry) == Readiness::Degraded);

        // 失败后被外部重试并成功（`LoadFailed` 只是流送系统「不自动重试」，
        // 不是「不可重试」）⇒ 门禁必须立刻回到 Ready。
        EXPECT(asset.CommitAsyncTexture(kReadySentinel));
        EXPECT(EvaluateRequiredTextureReadiness(&entry) == Readiness::Ready);
    }
}//namespace

int main(int, char **)
{
    hgl::logger::InitLogger(OS_TEXT("TestMaterialTextureFallback"));

    TestNoResources();
    TestSingleResource();
    TestMixedResources();
    TestStateTransitions();

    if (g_fail != 0)
    {
        GLogError(u8"TestMaterialTextureFallback：%d 项判据失败", g_fail);
        return 1;
    }

    GLogInfo(u8"TestMaterialTextureFallback：全部判据通过");
    return 0;
}
