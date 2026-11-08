#include<hgl/ecs/systems/render/streaming/TextureStreamingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/asset/LoadedFileManager.h>
#include<hgl/asset/LoadedFileHandler.h>
#include<hgl/asset/AssetByteSource.h>
#include<hgl/asset/AssetURI.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/asset/TextureAsset.h>
#include<hgl/graph/asset/AssetSourceMount.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GpuUploadTask.h>
#include<hgl/graph/texture/TextureLoader.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/type/ValueArray.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/utf.h>

namespace hgl::ecs
{
    namespace
    {
        /// 单帧最多发起几次读盘。IO 侧本来就是单线程串行，这里再限流的意义在于
        /// 压住"一帧内解析+建纹理+提交上传"的瞬时峰值，避免切换/启动首帧掉帧。
        constexpr int kMaxSubmitPerFrame = 2;

        /// Direct-to-Staging 前置：查询**已折叠真实路径**的字节数(含 .Tex2D 文件头)。
        /// 调用方先把 URI 折成物理路径再送来；不存在/非文件返回 0 ⇒ 退回经典 Submit 通道
        /// （归档型包裹的 URI 折不成路径，本来也就该走 AssetURIByteSource 取流）。
        uint64 QueryPayloadBytes(const OSString &path)
        {
            filesystem::FileInfo fi;

            if (!filesystem::GetFileInfo(path.c_str(), fi) || !fi.is_file)
                return 0;

            return fi.size;
        }

        /// GPU 上传完成回调：句柄的唯一"转正"点。
        /// user_data 就是句柄本身（归 TextureManager 所有，活得比任何世界都久），
        /// 因此这里不需要系统指针，也就不存在系统先死、回调后到的悬垂。
        void OnTextureUploadFinished(graph::GpuUploadTask *task, void *user_data)
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

        // ── Direct-to-Staging 登记表：请求编号 -> 发起时预分配的映射 staging 缓冲 ──
        // IO 线程只负责往 Map 指针写字节；缓冲的最终去向(转交上传/作废回收)永远发生在
        // Pump 线程的回调里，所以这里和三个回调一样无需加锁。
        UnorderedMap<uint64, graph::DeviceBuffer *> direct_buffers;
        ValueArray<graph::DeviceBuffer *>           direct_all;   ///< 未取走的缓冲(供析构兜底遍历)
        uint64 direct_bytes = 0;                                  ///< 在途直写 staging 总字节，预算门控用

    public:

        explicit StreamHandler(TextureStreamingSystem *o) : owner(o) {}

        ~StreamHandler() override
        {
            if (direct_all.IsEmpty())
                return;

            // 理论上不可达：LoadedFileManager 保证所有请求在销毁前派发终态回调。
            // 兜底回收在途缓冲，避免泄漏 host-visible 映射内存。
            GLogError(u8"[TextureStreaming] %d in-flight staging buffer(s) reclaimed at handler teardown",
                      direct_all.GetCount());

            graph::DeviceBuffer **bufs = direct_all.GetData();

            for (int i = 0; i < direct_all.GetCount(); ++i)
            {
                bufs[i]->Unmap();
                delete bufs[i];
            }
        }

        void AddRef() { ++ref_count; }
        void Release() { if (--ref_count <= 0) delete this; }
        void DetachOwner() { owner = nullptr; }

        /** @brief CN: 当前在途直写 staging 占用的字节数(预算门控)。 EN: Bytes of staging buffers still awaiting their callback. */
        uint64 GetPendingDirectBytes()const{return direct_bytes;}

        /** @brief CN: 记下"编号 id 的请求要把字节写进 buf"。 EN: Record that request `id` writes bytes into `buf`. */
        void RegisterDirectBuffer(uint64 id, graph::DeviceBuffer *buf, uint64 bytes)
        {
            direct_buffers.Add(id, buf);
            direct_all.Add(buf);
            direct_bytes += bytes;
        }

        /** @brief CN: 取走(并注销)编号对应的 staging；不是直写请求就返回 nullptr。 EN: Take (and unregister) the staging for `id`; nullptr if not a direct request. */
        graph::DeviceBuffer *TakeDirectBuffer(uint64 id)
        {
            graph::DeviceBuffer **p = direct_buffers.GetValuePointer(id);

            if (!p)
                return nullptr;

            const graph::DeviceBuffer *found = *p;

            direct_buffers.DeleteByKey(id);
            direct_all.Remove(const_cast<graph::DeviceBuffer*>(found));
            direct_bytes -= found->GetSize();

            return const_cast<graph::DeviceBuffer*>(found);
        }

        void OnFileLoaded(const FileLoadTicket &ticket, LoadedFile *file) override;
        void OnFileFailed(const FileLoadTicket &ticket, io::AsyncFileLoadError error) override;
        void OnFileCancelled(const FileLoadTicket &ticket) override;
    };

    void TextureStreamingSystem::StreamHandler::OnFileLoaded(const FileLoadTicket &ticket, LoadedFile *file)
    {
        graph::DeviceBuffer *staging = TakeDirectBuffer(ticket.id);

        graph::TextureAsset *asset = (owner && file) ? owner->FindAssetByRequest(ticket.id) : nullptr;

        if (asset)
        {
            const uint64 task_id = staging
                ? owner->committer.CommitTexture2DDirect(file,
                                                         staging,
                                                         asset->GetAutoMipmaps(),
                                                         graph::UploadPriority::Normal,
                                                         0,
                                                         OnTextureUploadFinished,
                                                         asset)
                : owner->committer.CommitTexture2D(file,
                                                   asset->GetAutoMipmaps(),
                                                   graph::UploadPriority::Normal,
                                                   0,
                                                   OnTextureUploadFinished,
                                                   asset);
            if (task_id == 0)
            {
                if (staging)
                    delete staging;     ///< CommitTexture2DDirect 各口径都已内部 Unmap，这里只回收内存

                asset->FailAsyncLoad("GPU 上传任务提交失败");
            }
        }
        else if (staging)
        {
            // 句柄已消失或 owner 已脱钩：直写进来的字节随之作废。
            // 这副缓冲还没走过 loader，仍处于映射态，先解映射再释放。
            staging->Unmap();
            delete staging;
        }

        Release();      ///< 此后不得再访问成员
    }

    void TextureStreamingSystem::StreamHandler::OnFileFailed(const FileLoadTicket &ticket, io::AsyncFileLoadError error)
    {
        if (graph::DeviceBuffer *staging = TakeDirectBuffer(ticket.id))
        {
            // IO 线程只写了(或没写完)字节，映射始终由发起方在这里关闭
            staging->Unmap();
            delete staging;
        }

        if (owner && owner->tex_manager)
        {
            if (graph::TextureAsset *asset = owner->FindAssetByRequest(ticket.id))
            {
                if (error == io::AsyncFileLoadError::BufferTooSmall)
                {
                    // 查询大小之后文件变大了：临时且可恢复，退回待办集合，下帧重新查尺寸再提交
                    asset->ClearAsyncRequest();
                }
                else
                {
                    const AnsiString reason = AnsiString("文件读取失败(AsyncFileLoadError=")
                                            + AnsiString::numberOf(static_cast<int>(error))
                                            + ")";

                    asset->FailAsyncLoad(reason.c_str());
                }
            }
        }

        Release();
    }

    void TextureStreamingSystem::StreamHandler::OnFileCancelled(const FileLoadTicket &ticket)
    {
        if (graph::DeviceBuffer *staging = TakeDirectBuffer(ticket.id))
        {
            staging->Unmap();
            delete staging;
        }

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
        const auto & handle_table = tex_manager->GetTextureAssets();

        int submitted = 0;
        int scanned   = 0;   ///< 句柄表里有效的句柄数
        int ready     = 0;   ///< 已 Resident
        int in_flight = 0;   ///< 读取/上传在途
        int no_payload= 0;   ///< RT 来源 / 无文件名：本系统绝不读盘
        int failed    = 0;   ///< LoadFailed / Disposed：不自动重试
        int claimed   = 0;   ///< 同步路径已加载过 ⇒ 直接认领
        int throttled = 0;   ///< 待办但本轮被限流挡下（下一帧继续）

        for (int handle_index = 0; handle_index < handle_table.GetCount(); ++handle_index)
        {
            graph::TextureAsset *asset = handle_table[handle_index];

            if (!asset)
                continue;
            ++scanned;

            if (asset->IsReady())
            {
                ++ready;
                continue;
            }

            if (asset->IsAsyncRequested())
            {
                ++in_flight;
                continue;
            }

            if (submitted >= kMaxSubmitPerFrame)
            {
                ++throttled;
                continue;
            }

            // RenderTarget 来源的句柄没有磁盘负载：它的就绪由 RenderTargetManager
            // 在附件创建/重建时直接绑定，绝不能被当成文件名送去读盘。
            if (asset->IsRenderTarget())
            {
                ++no_payload;
                continue;
            }

            const PayloadState state = asset->GetPayloadState();

            // 失败/已销毁的句柄不自动重试：坏文件每帧重读磁盘只会刷屏，
            // 而且它对应的图元本来就因为未就绪而不渲染，不会画错。
            if (state == PayloadState::LoadFailed || state == PayloadState::Disposed)
            {
                ++failed;
                continue;
            }

            const OSString &filename = asset->GetFilename();

            if (filename.IsEmpty())
            {
                ++no_payload;
                continue;           ///< 包装现成纹理的句柄：没有文件可读
            }

            // 收敛捷径：同步路径已经加载过这个文件 ⇒ 直接认领那张纹理，不重复读盘建纹理。
            if (graph::Texture *ready = tex_manager->FindTextureByFilename(filename))
            {
                asset->CommitAsyncTexture(ready);
                ++claimed;

                const U8String u8_claimed = ToU8String(filename);

                GLogVerbose(u8"[TextureStreaming] 句柄#%d 认领同步纹理（不读盘）：%s",
                          handle_index,
                          reinterpret_cast<const char *>(u8_claimed.c_str()));
                continue;
            }

            // 读盘通道统一（口径同 ModelStreamingSystem）：能折成物理路径的
            // （裸路径、pack(dir) 目录包裹）按真实路径读；折不成的 pack URI
            // （归档型包裹）按 URI 原文作查表键，字节改由 AssetURIByteSource
            // 在 IO 线程经 OpenAssetStream 分派取流，主线程不同步读整包。
            OSString physical_path;

            const bool foldable = graph::ResolveAssetPhysicalPath(filename, physical_path);

            OSString key_path = filename;
            io::AsyncFileByteSource *source = nullptr;

            if (foldable)
                key_path = physical_path;
            else
            {
                const asset::AssetURI uri = asset::AssetURI::Parse(filename);

                if (uri.IsPack())
                {
                    OSString route;                 // 挂载根只用于选物理盘；取不到
                    graph::GetAssetMountRoot(uri.pack_name, route);     // 就退回默认通道

                    source = new asset::AssetURIByteSource(route, uri);
                }
            }

            // 优先 Direct-to-Staging：先分配一副映射 staging，让 IO 线程把字节直接写进去，
            // 主线程之后只解析 32B 头并把缓冲挂进上传任务，全程零 CPU 拷贝。
            // 文件解析不到物理路径、预算不够或缓冲创建失败时，自动退回经典 Submit 路径。
            graph::DeviceBuffer *staging = nullptr;
            void *map_ptr = nullptr;
            uint64 payload = 0;

            if (graph::GpuUploadQueue *queue = tex_manager->GetUploadQueue())
            {
                payload = QueryPayloadBytes(key_path);

                if (payload >= graph::TextureFileHeaderLength &&
                    handler->GetPendingDirectBytes() + payload <=
                        static_cast<uint64>(queue->GetMaxStagingMemory()))
                {
                    staging = tex_manager->CreateTransferSourceBuffer(static_cast<VkDeviceSize>(payload));

                    if (staging)
                    {
                        map_ptr = staging->Map(0, staging->GetSize());

                        if (!map_ptr)
                        {
                            delete staging;       ///< 映射未建立，无需 Unmap
                            staging = nullptr;
                        }
                    }
                }
            }

            handler->AddRef();

            // map_ptr 只在 foldable（有物理路径）时建立，此时 source 必为空；
            // 经典通道则可能带 AssetURIByteSource（归档包裹按 URI 取字节）。
            const FileLoadId id = map_ptr
                ? load_manager->SubmitDirect(key_path,
                                             map_ptr,
                                             static_cast<int64>(payload),
                                             handler,
                                             io::AsyncFilePriority::Normal,
                                             nullptr,
                                             nullptr,
                                             nullptr,
                                             payload)
                : load_manager->Submit(key_path, handler, io::AsyncFilePriority::Normal,
                                       nullptr, source);

            if (id == 0)
            {
                handler->Release();     ///< 提交失败：不标记，下一帧自然重试

                if (staging)
                {
                    staging->Unmap();
                    delete staging;
                }

                continue;
            }

            if (staging)
                handler->RegisterDirectBuffer(id, staging, payload);

            asset->MarkAsyncRequested(id);
            ++submitted;

            const U8String u8_filename = ToU8String(filename);
            const U8String u8_key      = ToU8String(key_path);

            LogDebug(u8"[TextureStreaming] 提交读取：句柄#%d uri=%s 读盘键=%s"
                     u8"（request=%llu，direct=%d，state=%d，句柄表=%d）",
                     handle_index,
                     reinterpret_cast<const char *>(u8_filename.c_str()),
                     reinterpret_cast<const char *>(u8_key.c_str()),
                     static_cast<unsigned long long>(id),
                     map_ptr ? 1 : 0,
                     static_cast<int>(state),
                     handle_table.GetCount());
        }

        // 摘要：本系统的工作集是**整张句柄表**，与场景里有没有人用这张纹理无关。
        // 看到"不该读的文件"时，先在这里确认句柄表规模，再回看是谁把句柄建进表的。
        if (submitted > 0 || !first_scan_logged)
        {
            first_scan_logged = true;

            LogVerbose(u8"[TextureStreaming] 句柄表扫描：total=%d ready=%d in_flight=%d claimed=%d "
                       u8"no_payload=%d failed=%d submitted=%d throttled=%d",
                     scanned, ready, in_flight, claimed, no_payload, failed, submitted, throttled);
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
