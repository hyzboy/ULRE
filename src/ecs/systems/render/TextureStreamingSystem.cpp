#include<hgl/ecs/systems/render/TextureStreamingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/framework/LoadedFileManager.h>
#include<hgl/framework/LoadedFileHandler.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/asset/TextureAsset.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/TextureUploadTask.h>
#include<hgl/utf.h>

namespace hgl::ecs
{
    namespace
    {
        /// 单帧最多发起几次读盘。IO 侧本来就是单线程串行，这里再限流的意义在于
        /// 压住"一帧内解析+建纹理+提交上传"的瞬时峰值，避免切换/启动首帧掉帧。
        constexpr int kMaxSubmitPerFrame = 2;

        /// GPU 上传完成回调：句柄的唯一"转正"点。
        /// user_data 就是句柄本身（归 TextureManager 所有，活得比任何世界都久），
        /// 因此这里不需要系统指针，也就不存在系统先死、回调后到的悬垂。
        void OnTextureUploadFinished(graph::TextureUploadTask *task, void *user_data)
        {
            auto *asset = static_cast<graph::TextureAsset *>(user_data);

            if (!task || !asset)
                return;

            if (task->state != graph::UploadTaskState::Completed || !task->target_texture)
            {
                asset->FailAsyncLoad("GPU 上传未完成");
                return;
            }

            if (!asset->CommitAsyncTexture(task->target_texture))
                return;

            // 回填「文件名 → 纹理」：此后同步的 LoadTexture2D(同一文件) 直接复用这张，
            // 同一个文件不会因为走了哪条路径而出现两张纹理。
            if (graph::TextureManager *tm = asset->GetTextureManager())
                tm->RegisterTextureFilename(asset->GetFilename(), task->target_texture);

            const U8String u8_filename = ToU8String(asset->GetFilename());

            GLogDebug(u8"[TextureStreaming] 就绪：%s（tex=%p）",
                      reinterpret_cast<const char *>(u8_filename.c_str()),
                      static_cast<void *>(task->target_texture));
        }
    }//namespace

    /**
     * 读盘回调接收者。
     *
     * 生命周期比系统长：`LoadedFileManager` 手里只有裸指针，而回调可能在系统析构
     * 之后才被 `Stop()` 派发（`GraphicsContext::Shutdown` 的 `Stop(Drain)` 会同步
     * 派发所有取消回调）。所以这里用「一次 Submit 一次引用」的自持计数，
     * 系统析构时只 `DetachOwner()` + 放掉自己那一份 ⇒ 晚到的回调安全地发现
     * `owner == nullptr`，仅做计数收尾。
     *
     * 计数不需要原子：`Submit/Pump/Collect/Stop` 与三个回调都只会在主线程上发生。
     */
    class TextureStreamingSystem::StreamHandler final : public LoadedFileHandler
    {
    private:

        TextureStreamingSystem *owner = nullptr;
        int ref_count = 1;

    public:

        explicit StreamHandler(TextureStreamingSystem *o) : owner(o) {}

        void AddRef() { ++ref_count; }
        void Release() { if (--ref_count <= 0) delete this; }
        void DetachOwner() { owner = nullptr; }

        void OnFileLoaded(const FileLoadTicket &ticket, LoadedFile *file) override;
        void OnFileFailed(const FileLoadTicket &ticket, io::AsyncFileLoadError error) override;
        void OnFileCancelled(const FileLoadTicket &ticket) override;
    };

    void TextureStreamingSystem::StreamHandler::OnFileLoaded(const FileLoadTicket &ticket, LoadedFile *file)
    {
        if (owner && owner->tex_manager && file)
        {
            graph::TextureAsset *asset = owner->FindAssetByRequest(ticket.id);

            if (asset)
            {
                const uint64 task_id = owner->committer.CommitTexture2D(file,
                                                                        asset->GetAutoMipmaps(),
                                                                        graph::UploadPriority::Normal,
                                                                        0,
                                                                        OnTextureUploadFinished,
                                                                        asset);
                if (task_id == 0)
                    asset->FailAsyncLoad("GPU 上传任务提交失败");
            }
        }

        Release();      ///< 此后不得再访问成员
    }

    void TextureStreamingSystem::StreamHandler::OnFileFailed(const FileLoadTicket &ticket, io::AsyncFileLoadError error)
    {
        if (owner && owner->tex_manager)
        {
            if (graph::TextureAsset *asset = owner->FindAssetByRequest(ticket.id))
            {
                const AnsiString reason = AnsiString("文件读取失败(AsyncFileLoadError=")
                                        + AnsiString::numberOf(static_cast<int>(error))
                                        + ")";

                asset->FailAsyncLoad(reason.c_str());
            }
        }

        Release();
    }

    void TextureStreamingSystem::StreamHandler::OnFileCancelled(const FileLoadTicket &ticket)
    {
        if (owner && owner->tex_manager)
        {
            // 取消只回退"已提交"标记（Loading ⇒ MetaOnly），句柄留在待办集合里，
            // 下一帧重新排队 —— 取消不该把资源变成永久不可用。
            if (graph::TextureAsset *asset = owner->FindAssetByRequest(ticket.id))
                asset->ClearAsyncRequest();
        }

        Release();
    }

    TextureStreamingSystem::TextureStreamingSystem()
        :System("TextureStreamingSystem")
    {
        // 必须早于引擎自己的 UpdateUploadQueue（见头文件说明）。
        SetExecutionPhase(ExecutionPhase::RenderPreBeginFrame);

        handler = new StreamHandler(this);
    }

    TextureStreamingSystem::~TextureStreamingSystem()
    {
        Shutdown();
    }

    void TextureStreamingSystem::Shutdown()
    {
        if (handler)
        {
            handler->DetachOwner();
            handler->Release();
            handler = nullptr;
        }

        committer.SetTextureManager(nullptr);

        load_manager = nullptr;
        tex_manager  = nullptr;
    }

    bool TextureStreamingSystem::EnsureManagers()
    {
        if (load_manager && tex_manager)
            return true;

        if (!context)
            return false;

        graph::GraphicsContext *gc = context->GetGraphicsContext();

        if (!gc)
            return false;

        // 惰性抓取：系统在 InstallPrimitiveGroup 时注册，那一刻设备/纹理管理器可能还没建。
        if (!load_manager)
            load_manager = gc->GetLoadedFileManager();

        if (!tex_manager)
        {
            tex_manager = gc->GetTextureManager();

            if (tex_manager)
                committer.SetTextureManager(tex_manager);
        }

        return load_manager && tex_manager;
    }

    graph::TextureAsset *TextureStreamingSystem::FindAssetByRequest(uint64 request_id)const
    {
        if (!tex_manager || request_id == 0)
            return nullptr;

        for (graph::TextureAsset *asset : tex_manager->GetTextureAssets())
        {
            if (asset && asset->IsAsyncRequested() && asset->GetAsyncRequestID() == request_id)
                return asset;
        }

        return nullptr;
    }

    void TextureStreamingSystem::AcquireOneFrame()
    {
        int submitted = 0;

        for (graph::TextureAsset *asset : tex_manager->GetTextureAssets())
        {
            if (submitted >= kMaxSubmitPerFrame)
                break;

            if (!asset || asset->IsReady() || asset->IsAsyncRequested())
                continue;

            // RenderTarget 来源的句柄没有磁盘负载：它的就绪由 RenderTargetManager
            // 在附件创建/重建时直接绑定，绝不能被当成文件名送去读盘。
            if (asset->IsRenderTarget())
                continue;

            const PayloadState state = asset->GetPayloadState();

            // 失败/已销毁的句柄不自动重试：坏文件每帧重读磁盘只会刷屏，
            // 而且它对应的图元本来就因为未就绪而不渲染，不会画错。
            if (state == PayloadState::LoadFailed || state == PayloadState::Disposed)
                continue;

            const OSString &filename = asset->GetFilename();

            if (filename.IsEmpty())
                continue;           ///< 包装现成纹理的句柄：没有文件可读

            // 收敛捷径：同步路径已经加载过这个文件 ⇒ 直接认领那张纹理，不重复读盘建纹理。
            if (graph::Texture *ready = tex_manager->FindTextureByFilename(filename))
            {
                asset->CommitAsyncTexture(ready);
                continue;
            }

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

            LogDebug(u8"[TextureStreaming] 提交读取：%s（request=%llu）",
                     reinterpret_cast<const char *>(u8_filename.c_str()),
                     static_cast<unsigned long long>(id));
        }
    }

    void TextureStreamingSystem::Update(float delta_time)
    {
        (void)delta_time;

        if (!EnsureManagers())
            return;

        load_manager->Pump();       ///< ① 派发读盘完成回调（内部会提交 GPU 上传）
        AcquireOneFrame();          ///< ② 提交本帧该发起的读取
        load_manager->Collect();    ///< ③ 回收已归零的文件引用
    }

    int TextureStreamingSystem::GetPendingRequestCount()const
    {
        if (!tex_manager)
            return 0;

        int count = 0;

        for (graph::TextureAsset *asset : tex_manager->GetTextureAssets())
        {
            if (asset && asset->IsAsyncRequested())
                ++count;
        }

        return count;
    }
}//namespace hgl::ecs
