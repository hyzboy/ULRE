#include<hgl/ecs/systems/render/streaming/GeometryStreamingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/asset/LoadedFileManager.h>
#include<hgl/asset/LoadedFileHandler.h>
#include<hgl/asset/AssetByteSource.h>
#include<hgl/asset/AssetURI.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/asset/GeometryAsset.h>
#include<hgl/graph/asset/GeometryAssetRegistry.h>
#include<hgl/graph/asset/GeometryAssetFile.h>
#include<hgl/graph/asset/AssetSourceMount.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GpuUploadQueue.h>
#include<hgl/graph/geo/LoadGeometry.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/type/ValueArray.h>
#include<hgl/time/Time.h>
#include<hgl/utf.h>

namespace hgl::ecs
{
    namespace
    {
        /// 单帧最多发起几次读盘。几何文件通常比纹理大，一次读盘的峰值更重，
        /// 因此这里比纹理流送更保守：压住"一帧内解包+建 buffer"的瞬时峰值。
        constexpr int kMaxSubmitPerFrame = 1;

        /// Direct-to-Staging 前置：查询**已折叠真实路径**的字节数。
        /// 调用方先把 URI 折成物理路径再送来；不存在/非文件返回 0 ⇒ 退回经典 Submit 通道。
        /// 与 `TextureStreamingSystem` 同名辅助函数同构（口径一致，便于统一治理）。
        uint64 QueryPayloadBytes(const OSString &path)
        {
            filesystem::FileInfo fi;

            if (!filesystem::GetFileInfo(path.c_str(), fi) || !fi.is_file)
                return 0;

            return fi.size;
        }
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

        // ── Direct-to-Staging 登记表：请求编号 -> 发起时预分配的映射 staging 缓冲 ──
        // IO 线程只负责往 Map 指针写字节；缓冲的最终去向(转交异步装载器/作废回收)永远
        // 发生在 Pump 线程的回调里，所以这里和三个回调一样无需加锁。
        UnorderedMap<uint64, graph::DeviceBuffer *> direct_buffers;
        ValueArray<graph::DeviceBuffer *>           direct_all;   ///< 未取走的缓冲(供析构兜底遍历)
        uint64 direct_bytes = 0;                                  ///< 在途直写 staging 总字节，预算门控用

        int upload_jobs = 0;      ///< 已转交异步装载器、尚未收到上传完成回调的作业数

        /// 挂进上传队列时随 user_data 传递的发起方上下文；装载完成回调里释放。
        struct UploadCtx
        {
            StreamHandler *handler;
            uint64         request_id;
        };

    public:

        explicit StreamHandler(GeometryStreamingSystem *o) : owner(o) {}

        ~StreamHandler() override
        {
            if (direct_all.IsEmpty())
                return;

            // 理论上不可达：LoadedFileManager 保证所有请求在销毁前派发终态回调。
            // 兜底回收在途缓冲，避免泄漏 host-visible 映射内存。
            GLogError(u8"[GeometryStreaming] %d in-flight staging buffer(s) reclaimed at handler teardown",
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

        /** @brief CN: 尚未收到上传完成回调的异步装载作业数(关机排空判据)。 */
        int GetInFlightUploadJobs()const{return upload_jobs;}

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

    private:

        /// DMA 上传(或同步回退)全部落定后的唯一收尾点：转正句柄、注销作业计数、放引用。
        static void OnGeometryUploadDone(graph::Geometry *geometry, bool success, void *user_data);
    };

    void GeometryStreamingSystem::StreamHandler::OnFileLoaded(const FileLoadTicket &ticket, LoadedFile *file)
    {
        graph::DeviceBuffer *staging = TakeDirectBuffer(ticket.id);

        graph::GeometryAsset *asset = (owner && file) ? owner->FindAssetByRequest(ticket.id) : nullptr;

        if (asset)
        {
            if (staging)
            {
                // Direct-to-Staging：字节已由 IO 线程直写进映射缓冲；解析、建
                // device-local VAB/IBO 并挂 BufferToBuffer 上传任务，收尾走
                // OnGeometryUploadDone（同步回退路径也会当场回调一次）。
                // staging 所有权移交装载器；本引用活到上传完成回调。
                void *map_ptr = staging->Map(0, staging->GetSize());

                if (map_ptr)
                {
                    UploadCtx *ctx = new UploadCtx{ this, ticket.id };

                    ++upload_jobs;
                    AddRef();     ///< 装载作业的引用挂到完成回调上

                    graph::LoadGeometryFromStagingAsync(asset->GetDevice(),
                                                        asset->GetVertexFormat(),
                                                        owner->upload_queue,
                                                        staging,
                                                        map_ptr,
                                                        static_cast<uint32>(staging->GetSize()),
                                                        file->GetFilename(),
                                                        OnGeometryUploadDone,
                                                        ctx);
                }
                else
                {
                    staging->Unmap();
                    delete staging;
                    asset->FailAsyncLoad("staging 映射失效");
                }
            }
            else
            {
                // 经典路径：`file` 只在回调期间有效，主线程同步解析 Minipack 并建 GPU buffer。
                graph::Geometry *geometry = graph::LoadGeometryFromMiniPackBytes(
                    asset->GetDevice(),
                    asset->GetVertexFormat(),
                    file->GetData(),
                    static_cast<uint32>(file->GetSize()),
                    file->GetFilename());

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
        else if (staging)
        {
            // 句柄已消失或 owner 已脱钩：直写进来的字节随之作废。
            staging->Unmap();
            delete staging;
        }

        Release();      ///< 此后不得再访问成员
    }

    void GeometryStreamingSystem::StreamHandler::OnGeometryUploadDone(graph::Geometry *geometry, bool success, void *user_data)
    {
        UploadCtx *ctx = static_cast<UploadCtx *>(user_data);
        StreamHandler *handler = ctx->handler;

        graph::Geometry *owned = (success ? geometry : nullptr);

        if (handler->owner && handler->owner->geometry_registry && owned)
        {
            graph::GeometryAsset *asset = handler->owner->FindAssetByRequest(ctx->request_id);

            if (asset)
            {
                if (!asset->CommitAsyncGeometry(owned))
                    delete owned;
                else
                {
                    GLogDebug(u8"[GeometryStreaming] DMA 就绪：geo=%p", static_cast<void *>(owned));
                }
            }
            else
            {
                delete owned;     ///< 句柄已消失，几何随之丢弃
            }
        }
        else if (owned)
        {
            delete owned;
        }

        if (handler->owner && handler->owner->geometry_registry && !owned)
        {
            if (graph::GeometryAsset *asset = handler->owner->FindAssetByRequest(ctx->request_id))
                asset->FailAsyncLoad("几何 Direct-to-Staging 装载失败");
            // 句柄已消失 ⇒ 无处标记；下一轮扫描自然不会再见到它。
        }

        delete ctx;
        --handler->upload_jobs;
        handler->Release();   ///< 此后不得再访问 handler
    }

    void GeometryStreamingSystem::StreamHandler::OnFileFailed(const FileLoadTicket &ticket, io::AsyncFileLoadError error)
    {
        if (graph::DeviceBuffer *staging = TakeDirectBuffer(ticket.id))
        {
            // IO 线程只写了(或没写完)字节，映射始终由发起方在这里关闭
            staging->Unmap();
            delete staging;
        }

        if (owner && owner->geometry_registry)
        {
            if (graph::GeometryAsset *asset = owner->FindAssetByRequest(ticket.id))
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

    void GeometryStreamingSystem::StreamHandler::OnFileCancelled(const FileLoadTicket &ticket)
    {
        if (graph::DeviceBuffer *staging = TakeDirectBuffer(ticket.id))
        {
            staging->Unmap();
            delete staging;
        }

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
        // Direct-to-Staging 在途作业要先排空：GpuUploadQueue 析构只释放任务而不派发
        // on_complete，若关机时仍有挂着的几何上传，作业里的 staging/几何会泄漏、
        // handler 引用也永不归零。借助仍存活的设备把队列跑到空（上限兜底防死循环）。
        if (upload_queue && handler)
        {
            for (int spin = 0; spin < 3000 && handler->GetInFlightUploadJobs() > 0; ++spin)
            {
                upload_queue->Update();
                hgl::SleepSecond(0.001);
            }

            if (handler->GetInFlightUploadJobs() > 0)
                GLogError(u8"[GeometryStreaming] %d upload job(s) still in flight at shutdown",
                          handler->GetInFlightUploadJobs());
        }

        if (handler)
        {
            handler->DetachOwner();
            handler->Release();
            handler = nullptr;
        }

        load_manager      = nullptr;
        geometry_registry = nullptr;
        upload_queue      = nullptr;
        tex_manager       = nullptr;
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

        // 上传队列与 staging 池寄生在 TextureManager 上（它拥有 GpuUploadQueue）。
        // 拿不到也不影响经典路径，只是永远退回 Submit 直读。
        if (!tex_manager)
            tex_manager = gc->GetTextureManager();

        if (tex_manager && !upload_queue)
            upload_queue = tex_manager->GetUploadQueue();

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
                    OSString route;             // 挂载根只用于选物理盘；取不到
                    graph::GetAssetMountRoot(uri.pack_name, route);     // 就退回默认通道

                    source = new asset::AssetURIByteSource(route, uri);
                }
            }

            // Direct-to-Staging：能解析物理文件、且在 staging 预算内时，预先分配
            // 映射缓冲并让 IO 线程直写其中；否则退回经典 Submit(内存副本)路径。
            // 预算门控与 TextureStreamingSystem 同口径：全部在途直写 staging 之和
            // 不得超过队列的 staging 上限，超限自然回退，不阻塞不重试风暴。
            graph::DeviceBuffer *staging = nullptr;
            void *map_ptr = nullptr;
            uint64 payload = 0;

            if (upload_queue && tex_manager)
            {
                payload = QueryPayloadBytes(key_path);

                if (payload > 0 &&
                    handler->GetPendingDirectBytes() + payload <= (uint64)upload_queue->GetMaxStagingMemory())
                {
                    staging = tex_manager->CreateTransferSourceBuffer(static_cast<VkDeviceSize>(payload));

                    if (staging)
                    {
                        map_ptr = staging->Map(0, staging->GetSize());

                        if (!map_ptr)
                        {
                            staging->Unmap();
                            delete staging;
                            staging = nullptr;
                        }
                    }
                }
            }

            // map_ptr 只在 foldable（有物理路径）时建立，此时 source 必为空；
            // 经典通道则可能带 AssetURIByteSource（归档包裹按 URI 取字节）。
            FileLoadId id;

            if (map_ptr)
            {
                id = load_manager->SubmitDirect(key_path, map_ptr, static_cast<int64>(payload),
                                                handler, io::AsyncFilePriority::Normal,
                                                nullptr, nullptr, nullptr, payload);
            }
            else
            {
                id = load_manager->Submit(key_path, handler, io::AsyncFilePriority::Normal,
                                          nullptr, source);
            }

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
