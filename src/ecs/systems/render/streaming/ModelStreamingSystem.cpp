#include<hgl/ecs/systems/render/streaming/ModelStreamingSystem.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/asset/AssetSourceMount.h>
#include<hgl/asset/AssetByteSource.h>
#include<hgl/asset/AssetURI.h>
#include<hgl/graph/asset/MaterialAsyncLoadLane.h>
#include<hgl/graph/asset/ModelLoadPlan.h>
#include<hgl/ecs/components/ModelRefComponent.h>   // A.1：定义先行接线在 A.2 消费
#include<hgl/graph/mesh/ModelStreaming.h>
#include<hgl/asset/LoadedFileManager.h>
#include<hgl/asset/LoadedFileHandler.h>
#include<hgl/filesystem/Path.h>
#include<hgl/log/Log.h>
#include<hgl/utf.h>

namespace hgl::ecs
{
    namespace
    {
        /// 单帧最多发起几次模型包读盘。模型包是大文件，一帧两个已经足够把 IO
        /// 队列喂满，同时又不会让「解析 + 建显存」在第一帧堆成峰值。
        constexpr int kMaxSubmitPerFrame = 2;

        constexpr const char *kLogModule = "ModelStreaming";
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
    class ModelStreamingSystem::StreamHandler final : public LoadedFileHandler
    {
    private:

        ModelStreamingSystem *owner = nullptr;
        int ref_count = 1;

    public:

        explicit StreamHandler(ModelStreamingSystem *o) : owner(o) {}

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

    ModelStreamingSystem::ModelStreamingSystem()
        : System("ModelStreamingSystem")
    {
        // 与纹理/几何流送同相位：必须早于引擎的显存提交队列推进，
        // 否则本帧拿到的字节要等下一帧才可能变成可画的图元。
        SetExecutionPhase(ExecutionPhase::RenderPreBeginFrame);

        handler = new StreamHandler(this);
    }

    ModelStreamingSystem::~ModelStreamingSystem()
    {
        Shutdown();
    }

    void ModelStreamingSystem::Shutdown()
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

    void ModelStreamingSystem::PumpMaterialLane()
    {
        if (!context)
            return;

        if (graph::GraphicsContext *gc = context->GetGraphicsContext())
            gc->PumpMaterialAsyncLoadLane();
    }

    void ModelStreamingSystem::FanOutMaterialDeps(AssetStream *stream)
    {
        stream->material_fanout = true;

        if (!context || !stream || !stream->asset)
            return;

        graph::GraphicsContext *gc = context->GetGraphicsContext();

        if (!gc)
            return;

        // 场景目录：优先取调用方给的基准目录，否则退到模型包所在目录。
        OSString scene_dir = settings.base_dir;

        if (scene_dir.IsEmpty())
        {
            const OSString &pack_path = stream->request.GetPackPath();

            // 包裹 / 身份寻址下 `pack_path` 不是磁盘路径（甚至为空），没有
            // 「同目录」这个概念：枚举要靠包裹索引，不在本门禁的职责里。
            if (pack_path.IsEmpty())
                return;

            scene_dir = filesystem::Path(pack_path).GetParent().ToOSString();
        }

        if (scene_dir.IsEmpty())
            return;

        graph::mtl::MaterialAsyncLoadLane *lane = gc->GetMaterialAsyncLoadLane();

        if (!lane)
            return;             ///< 起不了线程 ⇒ 退回同步装载，行为与门禁之前一致

        // 车道自带去重：表里已有的配方 / 数据块不会再解析一遍，所以调用方
        // 「先同步装一遍配方」的老写法与这里的扇出可以共存。
        const int recipes = lane->BeginRecipeDirectory(scene_dir);

        lane->BeginDataBlockDirectory(scene_dir);

        GLogDebug(u8"[ModelStreaming] 配方扇出：dir=%ls recipes=%d",
                  scene_dir.c_str(), recipes);
    }

    void ModelStreamingSystem::SetBuildSettings(const graph::GeometryVertexFormat &gvf,
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

    bool ModelStreamingSystem::EnsureManagers()
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

    ModelStreamingSystem::AssetStream *ModelStreamingSystem::FindStream(const graph::ModelAsset *asset)const
    {
        if (!asset)
            return nullptr;

        for (AssetStream *stream : streams)
            if (stream && stream->asset == asset)
                return stream;

        return nullptr;
    }

    ModelStreamingSystem::AssetStream *ModelStreamingSystem::FindStreamByRequest(uint64 request_id)const
    {
        if (request_id == 0)
            return nullptr;

        for (AssetStream *stream : streams)
            if (stream && stream->request.GetState() == graph::ModelStreamingState::Loading
                     && stream->request.GetRequestID() == request_id)
                return stream;

        return nullptr;
    }

    ModelStreamingSystem::AssetStream *ModelStreamingSystem::EnsureStream(graph::ModelAsset *asset)
    {
        if (AssetStream *found = FindStream(asset))
            return found;

        AssetStream *stream = new AssetStream;

        stream->asset = asset;
        streams.Add(stream);

        return stream;
    }

    void ModelStreamingSystem::CountPrimitives(const graph::ModelAssetContent &content,
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

    void ModelStreamingSystem::PruneStreams()
    {
        graph::ModelAssetRegistry *registry = graph::GetDefaultModelAssetRegistry();

        if (!registry)
        {
            for (AssetStream *stream : streams)
                delete stream;

            streams.Clear();
            return;
        }

        const ValueArray<graph::ModelAsset *> &assets = registry->GetAssets();

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

    void ModelStreamingSystem::AcquireOneFrame()
    {
        graph::ModelAssetRegistry *registry = graph::GetDefaultModelAssetRegistry();

        if (!registry)
            return;

        if (!settings.valid)
        {
            // 没有几何顶点格式与配方就无法解析 —— 与其画出一个空场景，
            // 不如把原因写进句柄，调用方一眼能看出是「忘了配置」。
            for (graph::ModelAsset *asset : registry->GetAssets())
            {
                if (asset && asset->GetState() == graph::ModelAssetState::Unloaded)
                    asset->Fail("场景流送系统尚未配置几何顶点格式 / 材质配方（SetBuildSettings）");
            }

            return;
        }

        int submitted = 0;

        for (graph::ModelAsset *asset : registry->GetAssets())
        {
            if (submitted >= kMaxSubmitPerFrame)
                break;

            if (!asset || asset->GetState() != graph::ModelAssetState::Unloaded)
                continue;

            AssetStream *stream = EnsureStream(asset);

            if (!stream)
                continue;

            // 两条通道统一走 `LoadedFileManager`：能折成物理路径的直接按路径读；
            // 折不成的（归档型包裹、`builtin`/`rt`）按 URI 原文作查表键，字节
            // 改由 `AssetURIByteSource` 在 IO 线程上经 `OpenAssetStream` 分派取流
            // —— 主线程不再为了拿字节同步读整包，也不再有独立的 CPU 取流池。
            OSString physical_path;

            const AnsiString &uri_text = asset->GetURI();

            OSString                 key_path = ToOSString(uri_text.c_str());
            io::AsyncFileByteSource *source   = nullptr;

            if (graph::ResolveAssetPhysicalPath(key_path, physical_path))
            {
                key_path = physical_path;           // 裸路径通道：查表键就是真实文件
            }
            else
            {
                const asset::AssetURI uri = asset::AssetURI::Parse(key_path,
                                                                    asset::AssetType::Model);

                OSString route;                     // 挂载根只用于选物理盘；取不到
                graph::GetAssetMountRoot(uri.pack_name, route);   // 就退回默认通道

                source = new asset::AssetURIByteSource(route, uri);
            }

            handler->AddRef();

            const bool started = stream->request.Begin(load_manager, key_path, source, handler);

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

    void ModelStreamingSystem::Advance()
    {
        for (AssetStream *stream : streams)
        {
            if (!stream || stream->settled)
                continue;

            if (AdvanceOne(stream))
                stream->settled = true;
        }
    }

    bool ModelStreamingSystem::AdvanceOne(AssetStream *stream)
    {
        graph::ModelAsset *asset = stream->asset;

        if (!asset)
            return true;

        graph::ModelAssetState state = asset->GetState();

        if (state == graph::ModelAssetState::Failed)
            return true;

        if (state == graph::ModelAssetState::Unloaded)
            return false;                   ///< 还没轮到它

        // ── 阶段 B-1：读盘完成 ⇒ 用同一份字节建计划单（不再读第二遍盘） ──────
        if (state == graph::ModelAssetState::Loading)
        {
            if (stream->request.HasFailed())
            {
                asset->Fail("模型包读取失败");
                return true;
            }

            if (!stream->request.IsReady())
                return false;               ///< 还在读

            graph::ModelLoadPlan plan;
            AnsiString           plan_error;

            if (!graph::BuildModelLoadPlanFromBytes(asset->GetURI(),
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

            // 字节已在手、目录已知：趁主线程还在建产物，把配方目录投给工作线程，
            // 让解析与下面这一段重叠，而不是等 `Build()` 时再同步扫盘。
            if (settings.preload_scene_recipes && !stream->material_fanout)
                FanOutMaterialDeps(stream);
        }

        // ── 阶段 B-2：解析产物（图元 + 节点树） ───────────────────────────────
        if (!stream->content_built)
        {
            if (state == graph::ModelAssetState::Failed)
                return true;

            // 配方门禁：等场景目录的配方在后台落地。`Build()` 之后紧接着的材质
            // 装配（`FindOrLoadMaterialRecipe` / 目录扫描回退）就全部命中表，
            // 主线程上不再出现整目录扫盘。
            //
            // 车道起不了 / 目录不可枚举 / 调用方关掉预载时这一步立刻放行。
            if (settings.preload_scene_recipes && context)
                if (graph::GraphicsContext *gc = context->GetGraphicsContext())
                    if (gc->IsMaterialAsyncLoadPending())
                        return false;

            graph::ModelAssetRegistry *registry = asset->GetRegistry();
            graph::ModelAssetContent  &content  = asset->GetMutableContent();

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
            if (asset->GetState() == graph::ModelAssetState::Ready)
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
                asset->CommitModel();
                return true;
            }

            // 产物出来时就已全部就绪（内置/已驻留几何）⇒ 当场提交成 `Ready`。
            if ((ready > 0 || failed > 0) && asset->SettleAllChildren(ready, failed))
            {
                asset->CommitModel();
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
                asset->CommitModel();
                return true;
            }
        }

        return false;
    }

    void ModelStreamingSystem::Update(float delta_time)
    {
        (void)delta_time;

        if (!EnsureManagers())
            return;

        load_manager->Pump();       ///< ① 派发读盘完成回调（字节落进各句柄的缓冲）
        PruneStreams();             ///< ② 丢掉注册表已清掉的句柄（防悬垂）
        PumpMaterialLane();         ///< ③ 把材质车道解析好的配方提交进表
        AcquireOneFrame();          ///< ④ 提交本帧该发起的取流
        Advance();                  ///< ⑤ 推进状态机（含配方门禁）
        load_manager->Collect();    ///< ⑥ 回收已归零的文件引用
    }

    int ModelStreamingSystem::GetReadyModelCount()const
    {
        graph::ModelAssetRegistry *registry = graph::GetDefaultModelAssetRegistry();

        return registry ? registry->GetReadyCount() : 0;
    }
}//namespace hgl::ecs
