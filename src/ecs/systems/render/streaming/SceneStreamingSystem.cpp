#include<hgl/ecs/systems/render/streaming/SceneStreamingSystem.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/asset/AssetSourceMount.h>
#include<hgl/graph/asset/SceneLoadPlan.h>
#include<hgl/graph/mesh/SceneStreaming.h>
#include<hgl/asset/LoadedFileManager.h>
#include<hgl/asset/LoadedFileHandler.h>
#include<hgl/log/Log.h>
#include<hgl/utf.h>

namespace hgl::ecs
{
    namespace
    {
        /// 单帧最多发起几次模型包读盘。模型包是大文件，一帧两个已经足够把 IO
        /// 队列喂满，同时又不会让「解析 + 建显存」在第一帧堆成峰值。
        constexpr int kMaxSubmitPerFrame = 2;

        constexpr const char *kLogModule = "SceneStreaming";
    }//namespace

    /**
     * 读盘回调接收者。
     *
     * 寿命比系统长：`LoadedFileManager` 只持有裸指针，回调可能在系统析构之后
     * 才被 `Stop(Drain)` 同步派发。因此沿用纹理流送的「一次提交一次引用」，
     * 系统析构只 `DetachOwner()` + 放掉自己那一份。
     *
     * 计数不需要原子：`Submit/Pump/Collect/Stop` 与回调都只发生在主线程。
     */
    class SceneStreamingSystem::StreamHandler final : public LoadedFileHandler
    {
    private:

        SceneStreamingSystem *owner = nullptr;
        int ref_count = 1;

    public:

        explicit StreamHandler(SceneStreamingSystem *o) : owner(o) {}

        void AddRef() { ++ref_count; }
        void Release() { if (--ref_count <= 0) delete this; }
        void DetachOwner() { owner = nullptr; }

        void OnFileLoaded(const FileLoadTicket &ticket, LoadedFile *file) override
        {
            if (owner && file)
                if (AssetStream *stream = owner->FindStreamByRequest(ticket.id))
                    stream->request.OnFileLoaded(ticket, file);

            Release();      ///< 此后不得再访问成员
        }

        void OnFileFailed(const FileLoadTicket &ticket, io::AsyncFileLoadError error) override
        {
            if (owner)
                if (AssetStream *stream = owner->FindStreamByRequest(ticket.id))
                    stream->request.OnFileFailed(ticket, error);

            Release();
        }

        void OnFileCancelled(const FileLoadTicket &ticket) override
        {
            if (owner)
                if (AssetStream *stream = owner->FindStreamByRequest(ticket.id))
                    stream->request.OnFileCancelled(ticket);

            Release();
        }
    };

    SceneStreamingSystem::SceneStreamingSystem()
        : System("SceneStreamingSystem")
    {
        // 与纹理/几何流送同相位：必须早于引擎的显存提交队列推进，
        // 否则本帧拿到的字节要等下一帧才可能变成可画的图元。
        SetExecutionPhase(ExecutionPhase::RenderPreBeginFrame);

        handler = new StreamHandler(this);
    }

    SceneStreamingSystem::~SceneStreamingSystem()
    {
        Shutdown();
    }

    void SceneStreamingSystem::Shutdown()
    {
        if (handler)
        {
            handler->DetachOwner();
            handler->Release();
            handler = nullptr;
        }

        for (AssetStream *stream : streams)
        {
            if (stream)
            {
                stream->request.Cancel();
                delete stream;
            }
        }

        streams.Clear();

        load_manager = nullptr;
        settings = BuildSettings();
    }

    void SceneStreamingSystem::SetBuildSettings(const graph::GeometryVertexFormat &gvf,
                                                const graph::mtl::MaterialRecipe *default_recipe,
                                                const graph::mtl::MaterialRecipe *const *recipe_table,
                                                uint32_t recipe_table_count,
                                                const OSString &base_dir)
    {
        settings.geometry_vertex_format = gvf;
        settings.default_recipe         = default_recipe;
        settings.recipe_table           = (recipe_table_count > 0) ? recipe_table : nullptr;
        settings.recipe_table_count     = (recipe_table_count > 0) ? recipe_table_count : 0;
        settings.base_dir               = base_dir;
        settings.valid                  = true;
    }

    bool SceneStreamingSystem::EnsureManagers()
    {
        if (!context)
            return false;

        if (!load_manager)
        {
            if (graph::GraphicsContext *gc = context->GetGraphicsContext())
                load_manager = gc->GetLoadedFileManager();
        }

        return load_manager != nullptr;
    }

    SceneStreamingSystem::AssetStream *SceneStreamingSystem::FindStream(const graph::SceneAsset *asset)const
    {
        if (!asset)
            return nullptr;

        for (AssetStream *stream : streams)
            if (stream && stream->asset == asset)
                return stream;

        return nullptr;
    }

    SceneStreamingSystem::AssetStream *SceneStreamingSystem::FindStreamByRequest(uint64 request_id)const
    {
        if (request_id == 0)
            return nullptr;

        for (AssetStream *stream : streams)
            if (stream && stream->request.GetState() == graph::SceneStreamingState::Loading
                     && stream->request.GetRequestID() == request_id)
                return stream;

        return nullptr;
    }

    SceneStreamingSystem::AssetStream *SceneStreamingSystem::EnsureStream(graph::SceneAsset *asset)
    {
        if (AssetStream *found = FindStream(asset))
            return found;

        AssetStream *stream = new AssetStream;

        stream->asset = asset;
        streams.Add(stream);

        return stream;
    }

    void SceneStreamingSystem::CountPrimitives(const graph::SceneAssetContent &content,
                                               int &total,
                                               int &ready,
                                               int &failed)const
    {
        total  = 0;
        ready  = 0;
        failed = 0;

        for (const graph::PrimitiveAsset &primitive : content.assets)
        {
            ++total;

            if (primitive.IsGeometryReady())
            {
                ++ready;
                continue;
            }

            // 几何读盘/解析失败是终态：`GeometryStreamingSystem` 不自动重试，
            // 不按失败落定的话子项永远等不到 ⇒ 场景恒停在 `Assembling`。
            const graph::GeometryAsset *geometry_asset = primitive.GetGeometryAsset();

            if (geometry_asset && geometry_asset->GetPayloadState() == hgl::PayloadState::LoadFailed)
                ++failed;
        }
    }

    void SceneStreamingSystem::PruneStreams()
    {
        graph::SceneAssetRegistry *registry = graph::GetDefaultSceneAssetRegistry();

        if (!registry)
        {
            for (AssetStream *stream : streams)
                delete stream;

            streams.Clear();
            return;
        }

        const ValueArray<graph::SceneAsset *> &assets = registry->GetAssets();

        for (int i = streams.GetCount() - 1; i >= 0; --i)
        {
            AssetStream *stream = streams[i];

            if (!stream)
            {
                streams.DeleteSwap(i);
                continue;
            }

            if (assets.Contains(stream->asset))
                continue;

            stream->request.Cancel();
            delete stream;
            streams.DeleteSwap(i);
        }
    }

    void SceneStreamingSystem::AcquireOneFrame()
    {
        graph::SceneAssetRegistry *registry = graph::GetDefaultSceneAssetRegistry();

        if (!registry)
            return;

        if (!settings.valid)
        {
            // 没有几何顶点格式与配方就无法解析 —— 与其画出一个空场景，
            // 不如把原因写进句柄，调用方一眼能看出是「忘了配置」。
            for (graph::SceneAsset *asset : registry->GetAssets())
            {
                if (asset && asset->GetState() == graph::SceneAssetState::Unloaded)
                    asset->Fail("场景流送系统尚未配置几何顶点格式 / 材质配方（SetBuildSettings）");
            }

            return;
        }

        int submitted = 0;

        for (graph::SceneAsset *asset : registry->GetAssets())
        {
            if (submitted >= kMaxSubmitPerFrame)
                break;

            if (!asset || asset->GetState() != graph::SceneAssetState::Unloaded)
                continue;

            AssetStream *stream = EnsureStream(asset);

            if (!stream)
                continue;

            // `pack(res):x/y` 之类没有磁盘对应物的引用只能同步取流；
            // 物理路径（含挂载目录内解析出的真实路径）一律走异步读盘。
            OSString physical_path;

            const AnsiString &uri_text = asset->GetURI();

            if (!graph::ResolveAssetPhysicalPath(ToOSString(uri_text.c_str()), physical_path))
            {
                AnsiString open_error;
                io::InputStream *stream_in =
                    graph::OpenAssetInputStream(ToOSString(uri_text.c_str()),
                                                asset::AssetType::Scene,
                                                &open_error);

                if (!stream_in)
                {
                    const AnsiString reason = open_error.IsEmpty()
                                            ? AnsiString("无法打开模型包")
                                            : open_error;

                    asset->Fail(reason.c_str());
                    stream->settled = true;
                    continue;
                }

                const int64 size = stream_in->GetSize();

                if (size <= 0 || size > static_cast<int64>(UINT32_MAX))
                {
                    delete stream_in;
                    asset->Fail("模型包尺寸非法");
                    stream->settled = true;
                    continue;
                }

                void *bytes = hgl_malloc(static_cast<size_t>(size));

                if (!bytes)
                {
                    delete stream_in;
                    asset->Fail("模型包缓冲分配失败");
                    stream->settled = true;
                    continue;
                }

                if (stream_in->Read(bytes, static_cast<int64>(size)) != size)
                {
                    hgl_free(bytes);
                    delete stream_in;
                    asset->Fail("模型包读取不完整");
                    stream->settled = true;
                    continue;
                }

                delete stream_in;

                const bool ok = stream->request.SetPackBytes(bytes, static_cast<uint32>(size));

                hgl_free(bytes);

                if (!ok)
                {
                    asset->Fail("模型包缓冲接管失败");
                    stream->settled = true;
                    continue;
                }

                asset->BeginStreaming(0);
                ++submitted;
                continue;
            }

            handler->AddRef();

            const bool started = stream->request.Begin(load_manager, physical_path);

            if (!started)
            {
                handler->Release();
                asset->Fail("模型包读盘提交失败");
                stream->settled = true;
                continue;
            }

            asset->BeginStreaming(stream->request.GetRequestID());
            ++submitted;
        }
    }

    void SceneStreamingSystem::Advance()
    {
        for (AssetStream *stream : streams)
        {
            if (!stream || stream->settled)
                continue;

            if (AdvanceOne(stream))
                stream->settled = true;
        }
    }

    bool SceneStreamingSystem::AdvanceOne(AssetStream *stream)
    {
        graph::SceneAsset *asset = stream->asset;

        if (!asset)
            return true;

        graph::SceneAssetState state = asset->GetState();

        if (state == graph::SceneAssetState::Failed)
            return true;

        if (state == graph::SceneAssetState::Unloaded)
            return false;                   ///< 还没轮到它

        // ── 阶段 B-1：读盘完成 ⇒ 用同一份字节建计划单（不再读第二遍盘） ──────
        if (state == graph::SceneAssetState::Loading)
        {
            if (stream->request.HasFailed())
            {
                asset->Fail("模型包读取失败");
                return true;
            }

            if (!stream->request.IsReady())
                return false;               ///< 还在读

            graph::SceneLoadPlan plan;
            AnsiString           plan_error;

            if (!graph::BuildSceneLoadPlanFromBytes(asset->GetURI(),
                                                    stream->request.GetPackBytes(),
                                                    stream->request.GetPackSize(),
                                                    plan,
                                                    nullptr,
                                                    &plan_error))
            {
                const AnsiString reason = plan_error.IsEmpty()
                                        ? AnsiString("场景依赖解析失败")
                                        : plan_error;

                asset->Fail(reason.c_str());
                return true;
            }

            stream->plan_built = true;

            // 计划单没有任何外部子项时 `SetPlan` 会在内部直接提交 ——
            // 但解析产物还没生成，因此这里**不能**提前返回，必须继续往下走。
            asset->SetPlan(plan, stream->request.GetPackPath());

            state = asset->GetState();
        }

        // ── 阶段 B-2：解析产物（图元 + 节点树） ───────────────────────────────
        if (!stream->content_built)
        {
            if (state == graph::SceneAssetState::Failed)
                return true;

            graph::SceneAssetRegistry *registry = asset->GetRegistry();
            graph::SceneAssetContent  &content  = asset->GetMutableContent();

            const OSString base_dir = settings.base_dir.IsEmpty()
                                    ? stream->request.GetPackPath()
                                    : settings.base_dir;

            if (!stream->request.Build(registry->GetDevice(),
                                       registry->GetGeometryManager(),
                                       settings.geometry_vertex_format,
                                       settings.default_recipe,
                                       base_dir,
                                       content.assets,
                                       content.nodes,
                                       content.root_nodes,
                                       settings.recipe_table,
                                       settings.recipe_table_count))
            {
                content.Clear();
                asset->Fail("模型包解析失败");
                return true;
            }

            stream->content_built = true;

            // 计划单已判为空依赖（`SetPlan` 当时就提交了）：产物补完即完成。
            if (asset->GetState() == graph::SceneAssetState::Ready)
                return true;

            // 子项口径换成「图元数」：几何才是场景就绪的真门禁（见头文件说明）。
            int total  = 0;
            int ready  = 0;
            int failed = 0;

            CountPrimitives(content, total, ready, failed);

            asset->SetChildTotal(total);

            if (total == 0)
            {
                // 没有任何图元需要等几何：没有可轮询的对象，直接提交。
                asset->CommitScene();
                return true;
            }

            // 产物出来时就已全部就绪（内置/已驻留几何）⇒ 当场提交成 `Ready`。
            if ((ready > 0 || failed > 0) && asset->SettleAllChildren(ready, failed))
            {
                asset->CommitScene();
                return true;
            }
        }

        // ── 阶段 C：逐帧结算仍未就绪的图元几何 ───────────────────────────────
        int total  = 0;
        int ready  = 0;
        int failed = 0;

        CountPrimitives(asset->GetContent(), total, ready, failed);

        const int ready_delta  = ready  - asset->GetReadyChildCount();
        const int failed_delta = failed - asset->GetFailedChildCount();

        if (ready_delta > 0 || failed_delta > 0)
        {
            // 「落定」只是记账；把状态机推到 `Ready` 必须显式提交。
            if (asset->SettleAllChildren(ready_delta < 0 ? 0 : ready_delta,
                                         failed_delta < 0 ? 0 : failed_delta))
            {
                asset->CommitScene();
                return true;
            }
        }

        return false;
    }

    void SceneStreamingSystem::Update(float delta_time)
    {
        (void)delta_time;

        if (!EnsureManagers())
            return;

        load_manager->Pump();       ///< ① 派发读盘完成回调（字节落进各句柄的缓冲）
        PruneStreams();             ///< ② 丢掉注册表已清掉的句柄（防悬垂）
        AcquireOneFrame();          ///< ③ 提交本帧该发起的读盘
        Advance();                  ///< ④ 推进状态机
        load_manager->Collect();    ///< ⑤ 回收已归零的文件引用
    }

    int SceneStreamingSystem::GetReadySceneCount()const
    {
        graph::SceneAssetRegistry *registry = graph::GetDefaultSceneAssetRegistry();

        return registry ? registry->GetReadyCount() : 0;
    }
}//namespace hgl::ecs
