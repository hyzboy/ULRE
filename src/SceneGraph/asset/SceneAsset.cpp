#include <hgl/graph/asset/SceneAsset.h>
#include <hgl/log/Log.h>

DEFINE_LOGGER_MODULE(SceneAssetStream)
USE_MODULE_LOGGER(SceneAssetStream)

namespace hgl::graph
{
    namespace
    {
        SceneAssetRegistry *g_default_scene_asset_registry = nullptr;
    }

    const char *GetSceneAssetStateName(SceneAssetState state)
    {
        switch (state)
        {
            case SceneAssetState::Unloaded:   return "Unloaded";
            case SceneAssetState::Loading:    return "Loading";
            case SceneAssetState::Assembling: return "Assembling";
            case SceneAssetState::Ready:      return "Ready";
            case SceneAssetState::Failed:     return "Failed";
            default:                          break;
        }

        return "Unknown";
    }

    SceneAsset::SceneAsset(SceneAssetRegistry *owner, const AnsiString &request_uri)
        : registry(owner), uri(request_uri)
    {
    }

    void SceneAsset::ResetProgress()
    {
        plan = SceneLoadPlan();
        stats = Stats();

        child_total = 0;
        child_ready = 0;
        child_failed = 0;

        async_request_id = 0;
        fail_reason = AnsiString();
        pack_path = OSString();

        content.Clear();
    }

    void SceneAsset::RebuildStats()
    {
        stats = Stats();
        stats.total = plan.TotalCount();

        stats.geometry        = plan.CountOf(AssetDepKind::Geometry);
        stats.material        = plan.CountOf(AssetDepKind::Material);
        stats.material_recipe = plan.CountOf(AssetDepKind::MaterialRecipe);
        stats.texture         = plan.CountOf(AssetDepKind::Texture);
        stats.material_data   = plan.CountOf(AssetDepKind::MaterialData);
        stats.embedded_geometry = plan.embedded_geometry_count;

        stats.other = stats.total - stats.geometry - stats.material - stats.material_recipe
                    - stats.texture - stats.material_data;

        child_total = stats.total;
        child_ready = 0;
        child_failed = 0;
    }

    bool SceneAsset::BeginStreaming(uint64 request_id)
    {
        if (state != SceneAssetState::Unloaded)
            return false;

        state = SceneAssetState::Loading;
        async_request_id = request_id;
        ++async_attempt;

        return true;
    }

    bool SceneAsset::SetPlan(const SceneLoadPlan &new_plan, const OSString &resolved_path)
    {
        if (state != SceneAssetState::Loading && state != SceneAssetState::Unloaded)
            return state == SceneAssetState::Ready;

        plan = new_plan;
        pack_path = resolved_path;
        RebuildStats();

        if (!plan.acyclic)
            FLogWarning(u8"场景依赖有环，装载顺序不再严格拓扑：%s", uri.c_str());

        state = SceneAssetState::Assembling;

        // 没有任何外部子项（空场景，或几何全内嵌且无材质依赖）：无需等待，直接提交。
        if (child_total <= 0)
            return CommitScene();

        return false;
    }

    bool SceneAsset::SettleChild(bool ok)
    {
        if (state == SceneAssetState::Ready)
            return true;

        if (state != SceneAssetState::Assembling)
            return false;

        if (GetPendingChildCount() <= 0)
        {
            // 派发出去的落定比登记的子任务还多 —— 上层记账有误，不能继续递减成负数。
            FLogWarning(u8"场景子项落定数超出登记数：%s", uri.c_str());
            return true;
        }

        if (ok)
            ++child_ready;
        else
            ++child_failed;

        return GetPendingChildCount() <= 0;
    }

    bool SceneAsset::SettleAllChildren(int ready_delta, int failed_delta)
    {
        if (state == SceneAssetState::Ready)
            return true;

        if (state != SceneAssetState::Assembling)
            return false;

        if (ready_delta < 0 || failed_delta < 0)
        {
            FLogWarning(u8"场景子项落定增量非法（ready=%d failed=%d）：%s",
                        ready_delta, failed_delta, uri.c_str());
            return false;
        }

        const int pending = GetPendingChildCount();
        int       ok      = ready_delta;
        int       bad     = failed_delta;

        // 汇总口径同样不能把计数推成负数：超出部分说明上层记账有误，截断并告警。
        if (ok + bad > pending)
        {
            FLogWarning(u8"场景子项落定数超出登记数（本帧 ready=%d failed=%d 未落定=%d）：%s",
                        ready_delta, failed_delta, pending, uri.c_str());

            bad = (pending > ok) ? (pending - ok) : 0;
            ok  = (ok < pending) ? ok : pending;
        }

        child_ready  += ok;
        child_failed += bad;

        return GetPendingChildCount() <= 0;
    }

    void SceneAsset::SetChildTotal(int total)
    {
        if (state != SceneAssetState::Assembling)
            return;

        if (child_ready > 0 || child_failed > 0)
        {
            FLogWarning(u8"场景已有子项落定，拒绝改写子项总数：%s", uri.c_str());
            return;
        }

        if (total < 0)
            total = 0;

        child_total = total;
    }

    bool SceneAsset::CommitScene()
    {
        if (state == SceneAssetState::Ready)
            return true;

        if (state != SceneAssetState::Assembling && state != SceneAssetState::Loading)
            return false;

        if (GetPendingChildCount() > 0)
            return false;

        state = SceneAssetState::Ready;
        async_request_id = 0;

        // 子项失败不阻断场景：缺一张纹理应当降级显示，而不是整个场景打不开。
        if (child_failed > 0)
            FLogWarning(u8"场景已就绪，但有 %d/%d 个子项失败：%s",
                        child_failed, child_total, uri.c_str());

        if (on_ready)
            on_ready(this, notify_user);

        return true;
    }

    void SceneAsset::Fail(const char *reason)
    {
        if (state == SceneAssetState::Failed)
            return;

        state = SceneAssetState::Failed;
        async_request_id = 0;
        fail_reason = reason ? reason : "";

        FLogError(u8"场景装载失败：%s（%s）", uri.c_str(), fail_reason.c_str());

        if (on_failed)
            on_failed(this, fail_reason.c_str(), notify_user);
    }

    void SceneAsset::Reset()
    {
        ResetProgress();
        state = SceneAssetState::Unloaded;
    }

    // ── SceneAssetRegistry ────────────────────────────────────────────────

    SceneAssetRegistry::~SceneAssetRegistry()
    {
        Clear();
    }

    SceneAsset *SceneAssetRegistry::FindByURI(const AnsiString &request_uri) const
    {
        if (request_uri.IsEmpty())
            return nullptr;

        for (SceneAsset *asset : assets)
            if (asset && asset->uri == request_uri)
                return asset;

        return nullptr;
    }

    SceneAsset *SceneAssetRegistry::Request(const AnsiString &uri)
    {
        if (uri.IsEmpty())
        {
            FLogError(u8"请求场景句柄失败：URI 为空");
            return nullptr;
        }

        if (SceneAsset *found = FindByURI(uri))
            return found;

        SceneAsset *asset = new SceneAsset(this, uri);
        if (!asset)
        {
            FLogError(u8"请求场景句柄失败：内存不足（%s）", uri.c_str());
            return nullptr;
        }

        assets.Add(asset);
        return asset;
    }

    int SceneAssetRegistry::GetPendingCount() const
    {
        int count = 0;

        for (SceneAsset *asset : assets)
            if (asset && asset->NeedsPump())
                ++count;

        return count;
    }

    int SceneAssetRegistry::GetReadyCount() const
    {
        int count = 0;

        for (SceneAsset *asset : assets)
            if (asset && asset->IsReady())
                ++count;

        return count;
    }

    int SceneAssetRegistry::GetFailedCount() const
    {
        int count = 0;

        for (SceneAsset *asset : assets)
            if (asset && asset->HasFailed())
                ++count;

        return count;
    }

    int SceneAssetRegistry::GetChildTaskCount() const
    {
        int count = 0;

        for (SceneAsset *asset : assets)
            if (asset)
                count += asset->GetChildCount();

        return count;
    }

    void SceneAssetRegistry::Clear()
    {
        for (SceneAsset *asset : assets)
            delete asset;

        assets.Clear();

        device = nullptr;
        geometry_manager = nullptr;
    }

    // ── 进程内默认句柄表 ──────────────────────────────────────────────────

    void SetDefaultSceneAssetRegistry(SceneAssetRegistry *registry)
    {
        g_default_scene_asset_registry = registry;
    }

    SceneAssetRegistry *GetDefaultSceneAssetRegistry()
    {
        return g_default_scene_asset_registry;
    }

    SceneAsset *RequestSceneAsset(const AnsiString &uri)
    {
        SceneAssetRegistry *registry = g_default_scene_asset_registry;
        if (!registry)
        {
            FLogError(u8"请求场景句柄失败：默认句柄表未登记（设备尚未初始化？）");
            return nullptr;
        }

        return registry->Request(uri);
    }
}//namespace hgl::graph
