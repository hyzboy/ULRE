#include <hgl/graph/asset/ModelAsset.h>
#include <hgl/log/Log.h>

DEFINE_LOGGER_MODULE(ModelAssetStream)
USE_MODULE_LOGGER(ModelAssetStream)

namespace hgl::graph
{
    namespace
    {
        ModelAssetRegistry *g_default_model_asset_registry = nullptr;
    }

    const char *GetModelAssetStateName(ModelAssetState state)
    {
        switch (state)
        {
            case ModelAssetState::Unloaded:   return "Unloaded";
            case ModelAssetState::Loading:    return "Loading";
            case ModelAssetState::Assembling: return "Assembling";
            case ModelAssetState::Ready:      return "Ready";
            case ModelAssetState::Failed:     return "Failed";
            default:                          break;
        }

        return "Unknown";
    }

    ModelAsset::ModelAsset(ModelAssetRegistry *owner, const AnsiString &request_uri)
        : registry(owner), uri(request_uri)
    {
    }

    void ModelAsset::ResetProgress()
    {
        plan = ModelLoadPlan();
        stats = Stats();

        child_total = 0;
        child_ready = 0;
        child_failed = 0;

        async_request_id = 0;
        fail_reason = AnsiString();
        pack_path = OSString();

        content.Clear();
    }

    void ModelAsset::RebuildStats()
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

    bool ModelAsset::BeginStreaming(uint64 request_id)
    {
        if (state != ModelAssetState::Unloaded)
            return false;

        state = ModelAssetState::Loading;
        async_request_id = request_id;
        ++async_attempt;

        return true;
    }

    bool ModelAsset::SetPlan(const ModelLoadPlan &new_plan, const OSString &resolved_path)
    {
        if (state != ModelAssetState::Loading && state != ModelAssetState::Unloaded)
            return state == ModelAssetState::Ready;

        plan = new_plan;
        pack_path = resolved_path;
        RebuildStats();

        if (!plan.acyclic)
            FLogWarning(u8"场景依赖有环，装载顺序不再严格拓扑：%s", uri.c_str());

        state = ModelAssetState::Assembling;

        // 没有任何外部子项（空场景，或几何全内嵌且无材质依赖）：无需等待，直接提交。
        if (child_total <= 0)
            return CommitModel();

        return false;
    }

    bool ModelAsset::SettleChild(bool ok)
    {
        if (state == ModelAssetState::Ready)
            return true;

        if (state != ModelAssetState::Assembling)
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

    bool ModelAsset::SettleAllChildren(int ready_delta, int failed_delta)
    {
        if (state == ModelAssetState::Ready)
            return true;

        if (state != ModelAssetState::Assembling)
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

    void ModelAsset::SetChildTotal(int total)
    {
        if (state != ModelAssetState::Assembling)
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

    bool ModelAsset::CommitModel()
    {
        if (state == ModelAssetState::Ready)
            return true;

        if (state != ModelAssetState::Assembling && state != ModelAssetState::Loading)
            return false;

        if (GetPendingChildCount() > 0)
            return false;

        state = ModelAssetState::Ready;
        async_request_id = 0;

        // 子项失败不阻断场景：缺一张纹理应当降级显示，而不是整个场景打不开。
        if (child_failed > 0)
            FLogWarning(u8"场景已就绪，但有 %d/%d 个子项失败：%s",
                        child_failed, child_total, uri.c_str());

        if (on_ready)
            on_ready(this, notify_user);

        return true;
    }

    void ModelAsset::Fail(const char *reason)
    {
        if (state == ModelAssetState::Failed)
            return;

        state = ModelAssetState::Failed;
        async_request_id = 0;
        fail_reason = reason ? reason : "";

        FLogError(u8"场景装载失败：%s（%s）", uri.c_str(), fail_reason.c_str());

        if (on_failed)
            on_failed(this, fail_reason.c_str(), notify_user);
    }

    void ModelAsset::Reset()
    {
        ResetProgress();
        state = ModelAssetState::Unloaded;
    }

    // ── ModelAssetRegistry ────────────────────────────────────────────────

    ModelAssetRegistry::~ModelAssetRegistry()
    {
        Clear();
    }

    ModelAsset *ModelAssetRegistry::FindByURI(const AnsiString &request_uri) const
    {
        if (request_uri.IsEmpty())
            return nullptr;

        for (ModelAsset *asset : assets)
            if (asset && asset->uri == request_uri)
                return asset;

        return nullptr;
    }

    ModelAsset *ModelAssetRegistry::Request(const AnsiString &uri)
    {
        if (uri.IsEmpty())
        {
            FLogError(u8"请求场景句柄失败：URI 为空");
            return nullptr;
        }

        if (ModelAsset *found = FindByURI(uri))
            return found;

        ModelAsset *asset = new ModelAsset(this, uri);
        if (!asset)
        {
            FLogError(u8"请求场景句柄失败：内存不足（%s）", uri.c_str());
            return nullptr;
        }

        assets.Add(asset);
        return asset;
    }

    int ModelAssetRegistry::GetPendingCount() const
    {
        int count = 0;

        for (ModelAsset *asset : assets)
            if (asset && asset->NeedsPump())
                ++count;

        return count;
    }

    int ModelAssetRegistry::GetReadyCount() const
    {
        int count = 0;

        for (ModelAsset *asset : assets)
            if (asset && asset->IsReady())
                ++count;

        return count;
    }

    int ModelAssetRegistry::GetFailedCount() const
    {
        int count = 0;

        for (ModelAsset *asset : assets)
            if (asset && asset->HasFailed())
                ++count;

        return count;
    }

    int ModelAssetRegistry::GetChildTaskCount() const
    {
        int count = 0;

        for (ModelAsset *asset : assets)
            if (asset)
                count += asset->GetChildCount();

        return count;
    }

    void ModelAssetRegistry::Clear()
    {
        for (ModelAsset *asset : assets)
            delete asset;

        assets.Clear();

        device = nullptr;
        geometry_manager = nullptr;
    }

    // ── 进程内默认句柄表 ──────────────────────────────────────────────────

    void SetDefaultModelAssetRegistry(ModelAssetRegistry *registry)
    {
        g_default_model_asset_registry = registry;
    }

    ModelAssetRegistry *GetDefaultModelAssetRegistry()
    {
        return g_default_model_asset_registry;
    }

    ModelAsset *RequestModelAsset(const AnsiString &uri)
    {
        ModelAssetRegistry *registry = g_default_model_asset_registry;
        if (!registry)
        {
            FLogError(u8"请求场景句柄失败：默认句柄表未登记（设备尚未初始化？）");
            return nullptr;
        }

        return registry->Request(uri);
    }
}//namespace hgl::graph
