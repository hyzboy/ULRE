#include<hgl/ecs/systems/render/GeometryStreamingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/framework/LoadedFileManager.h>
#include<hgl/framework/LoadedFileHandler.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/asset/GeometryAsset.h>
#include<hgl/graph/asset/GeometryAssetRegistry.h>
#include<hgl/graph/asset/GeometryAssetFile.h>
#include<hgl/graph/geo/LoadGeometry.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/utf.h>

namespace hgl::ecs
{
    namespace
    {
        /// 单帧最多发起几次读盘。几何文件通常比纹理大，一次读盘的峰值更重，
        /// 因此这里比纹理流送更保守：压住"一帧内解包+建 buffer"的瞬时峰值。
        constexpr int kMaxSubmitPerFrame = 1;
    }//namespace

    /**
     * 读盘回调接收者（与 `TextureStreamingSystem::StreamHandler` 同构）。
     *
     * 生命周期比系统长：`LoadedFileManager` 手里只有裸指针，而回调可能在系统析构
     * 之后才被 `Stop()` 派发（`GraphicsContext::Shutdown` 的 `Stop(Drain)` 会同步
     * 派发所有取消回调）。所以这里用「一次 Submit 一次引用」的自持计数，系统析构时
     * 只 `DetachOwner()` + 放掉自己那一份 ⇒ 晚到的回调安全地发现 `owner == nullptr`。
     *
     * 计数不需要原子：`Submit/Pump/Collect/Stop` 与三个回调都只会在主线程发生。
     */
    class GeometryStreamingSystem::StreamHandler final : public LoadedFileHandler
    {
    private:

        GeometryStreamingSystem *owner = nullptr;
        int ref_count = 1;

    public:

        explicit StreamHandler(GeometryStreamingSystem *o) : owner(o) {}

        void AddRef() { ++ref_count; }
        void Release() { if (--ref_count <= 0) delete this; }
        void DetachOwner() { owner = nullptr; }

        void OnFileLoaded(const FileLoadTicket &ticket, LoadedFile *file) override;
        void OnFileFailed(const FileLoadTicket &ticket, io::AsyncFileLoadError error) override;
        void OnFileCancelled(const FileLoadTicket &ticket) override;
    };

    void GeometryStreamingSystem::StreamHandler::OnFileLoaded(const FileLoadTicket &ticket, LoadedFile *file)
    {
        if (owner && owner->geometry_registry && file)
        {
            graph::GeometryAsset *asset = owner->FindAssetByRequest(ticket.id);

            if (asset)
            {
                const void *bytes = file->GetData();
                const uint32 size = static_cast<uint32>(file->GetSize());

                // 主线程解析 Minipack 并建 GPU buffer：与同步 `LoadGeometry` 走同一段
                // 解析代码，只是数据来源从 `FileInputStream` 换成了已就绪的内存块。
                graph::Geometry *geometry = graph::LoadGeometryFromMiniPackBytes(
                    asset->GetDevice(),
                    asset->GetVertexFormat(),
                    bytes,
                    size,
                    file->GetFilename());

                // 走到这里 `file` 只在回调期间有效，不能留引用 ⇒ 解析完就不再碰它。
                if (!geometry)
                    asset->FailAsyncLoad("Minipack 解析失败");
                else if (!asset->CommitAsyncGeometry(geometry))
                    delete geometry;
                else
                {
                    const U8String u8_name = ToU8String(file->GetFilename());

                    GLogDebug(u8"[GeometryStreaming] 就绪：%s（geo=%p）",
                              reinterpret_cast<const char *>(u8_name.c_str()),
                              static_cast<void *>(geometry));
                }
            }
        }

        Release();      ///< 此后不得再访问成员
    }

    void GeometryStreamingSystem::StreamHandler::OnFileFailed(const FileLoadTicket &ticket, io::AsyncFileLoadError error)
    {
        if (owner && owner->geometry_registry)
        {
            if (graph::GeometryAsset *asset = owner->FindAssetByRequest(ticket.id))
            {
                const AnsiString reason = AnsiString("文件读取失败(AsyncFileLoadError=")
                                        + AnsiString::numberOf(static_cast<int>(error))
                                        + ")";

                asset->FailAsyncLoad(reason.c_str());
            }
        }

        Release();
    }

    void GeometryStreamingSystem::StreamHandler::OnFileCancelled(const FileLoadTicket &ticket)
    {
        if (owner && owner->geometry_registry)
        {
            // 取消只回退「已提交」标记（Loading ⇒ MetaOnly），句柄留在待办集合里，
            // 下一帧重新排队 —— 取消不该把资源变成永久不可用。
            if (graph::GeometryAsset *asset = owner->FindAssetByRequest(ticket.id))
                asset->ClearAsyncRequest();
        }

        Release();
    }

    GeometryStreamingSystem::GeometryStreamingSystem()
        :System("GeometryStreamingSystem")
    {
        // 必须早于渲染收集（见头文件说明）。
        SetExecutionPhase(ExecutionPhase::RenderPreBeginFrame);

        handler = new StreamHandler(this);
    }

    GeometryStreamingSystem::~GeometryStreamingSystem()
    {
        Shutdown();
    }

    void GeometryStreamingSystem::Shutdown()
    {
        if (handler)
        {
            handler->DetachOwner();
            handler->Release();
            handler = nullptr;
        }

        load_manager      = nullptr;
        geometry_registry = nullptr;
    }

    bool GeometryStreamingSystem::EnsureManagers()
    {
        if (load_manager && geometry_registry)
            return true;

        if (!context)
            return false;

        graph::GraphicsContext *gc = context->GetGraphicsContext();

        if (!gc)
            return false;

        // 惰性抓取：系统在 InstallPrimitiveGroup 时注册，那一刻设备/管理器可能还没建。
        if (!load_manager)
            load_manager = gc->GetLoadedFileManager();

        if (!geometry_registry)
            geometry_registry = gc->GetGeometryAssetRegistry();

        return load_manager && geometry_registry;
    }

    graph::GeometryAsset *GeometryStreamingSystem::FindAssetByRequest(const uint64 request_id)const
    {
        if (!geometry_registry || request_id == 0)
            return nullptr;

        for (graph::GeometryAsset *asset : geometry_registry->GetAssets())
        {
            if (asset && asset->IsAsyncRequested() && asset->GetAsyncRequestID() == request_id)
                return asset;
        }

        return nullptr;
    }

    void GeometryStreamingSystem::AcquireOneFrame()
    {
        int submitted = 0;

        for (graph::GeometryAsset *asset : geometry_registry->GetAssets())
        {
            if (submitted >= kMaxSubmitPerFrame)
                break;

            if (!asset || asset->IsReady() || asset->IsAsyncRequested())
                continue;

            // 内置几何体在创建期就 Resident 了；万一没建成功（LoadFailed），
            // 它没有磁盘负载可读，绝不能把形状名当文件名送去读盘。
            if (!asset->NeedsFileLoad())
                continue;

            const PayloadState state = asset->GetPayloadState();

            // 失败/已销毁的句柄不自动重试：坏文件每帧重读磁盘只会刷屏，
            // 而且它对应的图元本来就因为未就绪而不渲染，不会画错。
            if (state == PayloadState::LoadFailed || state == PayloadState::Disposed)
                continue;

            const OSString &filename = asset->GetSpec().file_path;

            if (filename.IsEmpty())
                continue;

            handler->AddRef();

            const FileLoadId id = load_manager->Submit(filename, handler, io::AsyncFilePriority::Normal);

            if (id == 0)
            {
                handler->Release();     ///< 提交失败：不标记，下一帧自然重试
                continue;
            }

            asset->MarkAsyncRequested(id);
            ++submitted;

            const U8String u8_filename = ToU8String(filename);

            LogDebug(u8"[GeometryStreaming] 提交读取：%s（request=%llu）",
                     reinterpret_cast<const char *>(u8_filename.c_str()),
                     static_cast<unsigned long long>(id));
        }
    }

    void GeometryStreamingSystem::Update(float delta_time)
    {
        (void)delta_time;

        if (!EnsureManagers())
            return;

        load_manager->Pump();       ///< ① 派发读盘完成回调（内部解析并建 buffer）
        AcquireOneFrame();          ///< ② 提交本帧该发起的读取
        load_manager->Collect();    ///< ③ 回收已归零的文件引用
    }

    int GeometryStreamingSystem::GetPendingRequestCount()const
    {
        if (!geometry_registry)
            return 0;

        int count = 0;

        for (graph::GeometryAsset *asset : geometry_registry->GetAssets())
        {
            if (asset && asset->IsAsyncRequested())
                ++count;
        }

        return count;
    }
}//namespace hgl::ecs
