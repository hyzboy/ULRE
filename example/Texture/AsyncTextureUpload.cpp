// 异步纹理上传示例（两段式流水线）
//
// 本示例演示引擎推荐的两段式资源加载：
//
//   第一段(文件 IO，IO 线程池)：LoadedFileManager 把 .Tex2D 整个文件读进内存对象 LoadedFile，
//                              读盘发生在后台线程，主线程不再阻塞在磁盘上；
//   第二段(GPU 提交，主线程)：Tick 里 Pump() 回调 handler，handler 用 TextureLoadCommitter
//                              把内存字节解析并提交到 TextureManager 的上传队列。
//
// 同时演示三类取消/优先级语义的边界：
//   * 文件读取层取消：LoadedFileManager::Cancel(FileLoadId) —— 结果转成 OnFileCancelled；
//   * GPU 上传层取消：TextureManager::CancelUpload(task_id) —— 只影响已提交的上传任务；
//   * 两层各自有独立的优先级：请求读盘用 AsyncFilePriority，上传用 UploadPriority。
//
// 画面：一个全屏四边形，占位纹理先顶上；High 优先级的 lena.Tex2D 就绪后动态替换材质纹理，
//       用于直观验证"异步加载完成 → 显存就绪 → 材质切换"这条链路。

#include <hgl/framework/WorkManager.h>
#include <hgl/framework/LoadedFileManager.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/module/TextureUploadQueue.h>
#include <hgl/graph/module/TextureLoadCommitter.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/ShaderProgramManager.h>
#include <hgl/graph/asset/TextureAsset.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/vk/VKBindlessTextureManager.h>
#include <hgl/log/Log.h>

// ECS headers
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/support/TransformAccessor.h>
#include <hgl/ecs/components/GeometryData.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

class AsyncTextureUploadApp;

namespace
{
    GeometryVertexFormat CreatePureTexture2DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V2F},
            {VertexSemantic::TexCoord, VF_V2F},
        };
        return gvf;
    }

    constexpr uint32_t VERTEX_COUNT = 4;
    constexpr uint32_t INDEX_COUNT = 6;

    constexpr float position_data[VERTEX_COUNT][2] =
    {
        {-1, -1},
        { 1, -1},
        { 1,  1},
        {-1,  1},
    };

    constexpr float tex_coord_data[VERTEX_COUNT][2] =
    {
        {0, 0},
        {1, 0},
        {1, 1},
        {0, 1}
    };

    constexpr uint16_t index_data[INDEX_COUNT] =
    {
        0, 1, 2,
        0, 2, 3,
    };

    // 五个文件请求：读盘优先级与上传优先级可以不同，这里故意错开以展示两层独立调度
    constexpr const os_char *kImmediateFile = OS_TEXT("res/image/noise32.Tex2D");
    constexpr const os_char *kHighFile      = OS_TEXT("res/image/lena.Tex2D");
    constexpr const os_char *kNormalFile    = OS_TEXT("res/image/Gear.Tex2D");
    constexpr const os_char *kLowFile       = OS_TEXT("res/image/PeachHouse.Tex2D");
    constexpr const os_char *kCancelFile    = OS_TEXT("res/image/PeachWine.Tex2D");

    /**
     * GPU 上传完成回调：在线程/帧序上由 TextureUploadQueue 触发。
     */
    void OnUploadTaskFinished(TextureUploadTask *task, void *user_data);
}

/**
 * 一个纹理上传请求的接收者。
 *
 * 生命周期：handler 必须活到它名下所有请求回调完成，这里都由 App 以成员方式持有。
 * 业务上下文(优先级、bindless 槽位、是否切换材质)直接放在成员里，无需 user_data 打包。
 */
class UploadHandler:public LoadedFileHandler
{
public:

    AsyncTextureUploadApp * app=nullptr;

    const char *            label       ="";
    UploadPriority          upload_priority=UploadPriority::Normal;
    uint32                  bindless_handle=0;
    bool                    switch_material=false;

    uint64                  task_id=0;              //!< 提交后的 GPU 上传任务号
    bool                    loaded   =false;
    bool                    failed   =false;
    bool                    cancelled=false;

public:

    void OnFileLoaded   (const FileLoadTicket &ticket,LoadedFile *file)override;
    void OnFileFailed   (const FileLoadTicket &ticket,io::AsyncFileLoadError error)override;
    void OnFileCancelled(const FileLoadTicket &ticket)override;
};

class AsyncTextureUploadApp : public WorkObject
{
public:
    void OnHighTextureReady(Texture *tex)
    {
        if (tex && quad_primitive && quad_material_data)
        {
            // 运行期换图 = 换配方绑定：纹理是配方（材质原型）的固有属性，
            // 因此这里给实体挂一份**配方覆盖**（与共享 recipe 同名 upsert），
            // 而不是回到实体级的裸纹理授权。采样器由 Bindless 预设统一管理。
            TextureAsset *switched = GetManager<TextureManager>()->WrapTextureAsset(tex);

            if (!switched)
                return;

            mtl::MaterialRecipe switched_recipe = quad_recipe;

            if (!switched_recipe.AddTexture("base_color", switched, true))
                return;

            quad_material_data->SetRecipe(switched_recipe);

            switched_texture = true;
            GLogInfo(u8"[AsyncTextureUpload] Dynamically switched Quad texture to loaded high priority lena.Tex2D!");
        }
    }

    TextureLoadCommitter *GetCommitter() { return &committer; }

private:
    ECSContext *        ecs_world           = nullptr;
    Entity *            quad_entity         = nullptr;
    hgl::ecs::GeometryData *quad_primitive      = nullptr;
    hgl::ecs::MaterialData *quad_material_data = nullptr;

    Texture2D *         placeholder_tex     = nullptr;
    TextureAsset *      placeholder_asset   = nullptr;   ///< 占位纹理句柄（配方 base_color 的初值）
    mtl::MaterialRecipe quad_recipe{};
    PrimitiveAsset      quad_asset{};

    LoadedFileManager       load_manager;           //!< 第一段：后台读盘
    TextureLoadCommitter    committer;              //!< 第二段：主线程提交 GPU 上传

    UploadHandler       immediate_job;              //!< noise32：读盘 Critical + 上传 Immediate
    UploadHandler       high_job;                   //!< lena：读盘 High + 上传 High，就绪后切换材质
    UploadHandler       normal_job;                 //!< Gear：读盘 Normal + 上传 Normal
    UploadHandler       low_job;                    //!< PeachHouse：读盘 Low + 上传 Low
    UploadHandler       cancel_job;                 //!< PeachWine：提交后立刻取消文件读取

    FileLoadId          cancel_file_id      = 0;

    uint32              bindless_handle     = 0;
    uint32              frame_count         = 0;
    bool                switched_texture    = false;
    bool                gpu_cancel_done     = false;

private:
    bool InitMaterial()
    {
        auto *tex_manager = GetManager<TextureManager>();
        if (!tex_manager)
            return false;

        // 占位纹理走同步加载：首帧必须有材质可用，且它很小、不构成卡顿
        placeholder_tex = tex_manager->LoadTexture2D(OS_TEXT("res/image/Lena.Tex2D"), false);
        if (!placeholder_tex)
        {
            GLogError(u8"Failed to load placeholder texture Lena.Tex2D");
            return false;
        }

        // 纹理绑定写进配方（材质原型固有属性）；采样器由 Bindless 预设统一管理。
        placeholder_asset = tex_manager->WrapTextureAsset(placeholder_tex);
        if (!placeholder_asset)
            return false;

        if (!quad_recipe.AddTexture("base_color", placeholder_asset, true))
            return false;

        // 预分配 Bindless Handle 并绑定占位纹理
        auto *gc = GetGraphicsContext();
        if (gc)
        {
            auto *bindless_mgr = gc->GetBindlessTextureManager();
            if (bindless_mgr)
            {
                bindless_handle = bindless_mgr->AllocateHandle(placeholder_tex);
                GLogInfo(u8"[AsyncTextureUpload] Allocated Bindless Handle = %u with placeholder", bindless_handle);
            }
        }

        return true;
    }

    bool InitVBO()
    {
        auto *device = GetDevice();
        auto *buffer_manager = GetManager<BufferManager>();
        auto *geometry_manager = GetManager<GeometryManager>();
        if (!device || !buffer_manager || !geometry_manager)
            return false;

        GeometryCreater pc(device, CreatePureTexture2DGeometryVertexFormat(), buffer_manager);
        pc.Init("TextureQuad", VERTEX_COUNT, INDEX_COUNT);
        if (!pc.WriteVAB(VAN::Position, VF_V2F, position_data) ||
            !pc.WriteVAB(VAN::TexCoord, VF_V2F, tex_coord_data) ||
            !pc.WriteIBO(index_data))
            return false;

        auto *geometry = pc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);

        quad_recipe.recipe_name = "TextureQuad.UnlitTexture";
        quad_recipe.mtl_def_id = "UnlitTexture";
        quad_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid2DConfig();
        quad_asset = PrimitiveAsset(geometry, &quad_recipe, PrimitiveType::Triangles);

        return true;
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if (!ecs_world)
            return false;

        quad_entity = ecs_world->CreateEntity<Entity>("TextureQuad");
        auto quad_transform = ecs_world->GetTransform(ecs_world->CreateTransform(quad_entity->GetEntityID(), Mobility::Static));
        quad_primitive = quad_entity->GetContext()->GetOrCreateGeometryData(quad_entity);
        quad_material_data = ecs_world->GetOrCreateMaterialData(quad_entity);

        quad_transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        quad_transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        quad_transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        quad_transform.SetMobility(Mobility::Static);

        quad_primitive->GetOwner()->GetContext()->GetOrCreateGeometryData(quad_primitive->GetOwnerID())->SetPrimitiveAsset(&quad_asset);
        // 纹理已由配方声明（quad_recipe.AddTexture），实体端不再重复绑定
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

        return true;
    }

    /**
     * 第一段：把五个文件请求投给读盘线程池。全部非阻塞。
     */
    bool InitAsyncUploads()
    {
        auto *tex_manager = GetManager<TextureManager>();
        if (!tex_manager)
            return false;

        committer.SetTextureManager(tex_manager);

        // 默认单线程串行读盘：同一块物理设备上的并发读会互相争道，示例不需要更快
        if (!load_manager.Start())
        {
            GLogError(u8"[AsyncTextureUpload] Failed to start the IO worker pool");
            return false;
        }

        immediate_job.app              = this;
        immediate_job.label            = "Immediate";
        immediate_job.upload_priority  = UploadPriority::Immediate;
        immediate_job.bindless_handle  = 0;

        high_job.app                   = this;
        high_job.label                 = "High";
        high_job.upload_priority       = UploadPriority::High;
        high_job.bindless_handle       = bindless_handle;   // 完成后原子刷新该槽位
        high_job.switch_material       = true;

        normal_job.app                 = this;
        normal_job.label               = "Normal";
        normal_job.upload_priority     = UploadPriority::Normal;

        low_job.app                    = this;
        low_job.label                  = "Low";
        low_job.upload_priority        = UploadPriority::Low;

        cancel_job.app                 = this;
        cancel_job.label               = "Cancelled";
        cancel_job.upload_priority     = UploadPriority::Low;

        if (load_manager.Submit(kImmediateFile, &immediate_job, io::AsyncFilePriority::Critical) == 0)
            return false;

        if (load_manager.Submit(kHighFile, &high_job, io::AsyncFilePriority::High) == 0)
            return false;

        if (load_manager.Submit(kNormalFile, &normal_job, io::AsyncFilePriority::Normal) == 0)
            return false;

        if (load_manager.Submit(kLowFile, &low_job, io::AsyncFilePriority::Low) == 0)
            return false;

        cancel_file_id = load_manager.Submit(kCancelFile, &cancel_job, io::AsyncFilePriority::Low);
        if (cancel_file_id == 0)
            return false;

        // 文件层取消：只对"尚未读出来"的请求生效，回调会转成 OnFileCancelled
        const bool cancel_ok = load_manager.Cancel(cancel_file_id);
        GLogInfo(u8"[AsyncTextureUpload] Cancel(file stage) id=%llu result=%s",
                 cancel_file_id, cancel_ok ? "SUCCESS" : "FAILED");

        GLogInfo(u8"[AsyncTextureUpload] Submitted 5 file requests, IO workers = 2");

        return true;
    }

    /**
     * 示例中的内存像素贴图：不经过文件层，直接走上传队列(验证环形 Staging 复用)。
     */
    void SubmitMemoryTexture()
    {
        auto *tex_manager = GetManager<TextureManager>();
        if (!tex_manager)
            return;

        constexpr uint32_t MEM_DIM = 64;
        static uint32_t mem_pixels[MEM_DIM * MEM_DIM];
        for (uint32_t i = 0; i < MEM_DIM * MEM_DIM; ++i)
            mem_pixels[i] = 0xFF00FF00; // 纯绿 RGBA8

        ColorTextureCreateInfo *mem_tci = new ColorTextureCreateInfo(
            PF_RGBA8UN,
            VkExtent2D{MEM_DIM, MEM_DIM},
            U8String((const u8char *)u8"MemTexture64"));
        mem_tci->pixels = mem_pixels;
        mem_tci->total_bytes = sizeof(mem_pixels);
        mem_tci->origin_mipmaps = 1;
        mem_tci->target_mipmaps = 1;

        const uint64_t mem_task_id = tex_manager->CreateTexture2DAsync(
            mem_tci,
            UploadPriority::Normal,
            0,
            OnUploadTaskFinished,
            nullptr);
        GLogInfo(u8"[AsyncTextureUpload] Enqueued Memory Pixels Ring Task ID = %llu", mem_task_id);
    }

public:
    bool Init() override
    {
        if (!InitMaterial())
            return false;

        if (!InitVBO())
            return false;

        if (!InitECS())
            return false;

        if (!InitAsyncUploads())
            return false;

        SubmitMemoryTexture();

        return true;
    }

    ~AsyncTextureUploadApp() override
    {
        // 先停读盘(Drain 读完已接受的请求)，再在析构中由管理器丢弃未回调的 Job
        load_manager.Stop(thread::ShutdownMode::Drain);
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        ++frame_count;

        // 第二段：把已完成读盘的文件切成 GPU 上传。
        // 每帧限量 2 个，模拟大型工程里给上传留出的帧预算。
        load_manager.Pump(2);

        auto *tex_manager = GetManager<TextureManager>();
        if (!tex_manager)
            return;

        // GPU 上传层取消：等 Low 任务真正入队后再撤销，展示与文件层取消的边界
        if (!gpu_cancel_done && low_job.task_id != 0)
        {
            const UploadTaskState state = tex_manager->GetUploadState(low_job.task_id);

            if (state == UploadTaskState::Queued)
            {
                const bool ok = tex_manager->CancelUpload(low_job.task_id);
                GLogInfo(u8"[AsyncTextureUpload] Cancel(upload stage) id=%llu result=%s",
                         low_job.task_id, ok ? "SUCCESS" : "FAILED");
                gpu_cancel_done = true;
            }
        }

        // 每 30 帧打印一次两层的状态，便于观察异步进度
        if (frame_count % 30 == 0)
        {
            GLogInfo(u8"[AsyncTextureUpload Frame %u] file stage: Immediate=%d High=%d Normal=%d Low=%d Cancelled=%d (pending jobs=%d)",
                     frame_count,
                     immediate_job.loaded ? 1 : 0,
                     high_job.loaded ? 1 : 0,
                     normal_job.loaded ? 1 : 0,
                     low_job.loaded ? 1 : 0,
                     cancel_job.cancelled ? 1 : 0,
                     load_manager.GetPendingJobCount());

            GLogInfo(u8"[AsyncTextureUpload Frame %u] upload stage: Imm(ID=%llu)=%d High(ID=%llu)=%d Normal(ID=%llu)=%d Low(ID=%llu)=%d",
                     frame_count,
                     immediate_job.task_id, static_cast<int>(tex_manager->GetUploadState(immediate_job.task_id)),
                     high_job.task_id,      static_cast<int>(tex_manager->GetUploadState(high_job.task_id)),
                     normal_job.task_id,    static_cast<int>(tex_manager->GetUploadState(normal_job.task_id)),
                     low_job.task_id,       static_cast<int>(tex_manager->GetUploadState(low_job.task_id)));
        }

        // 帧末：回收本轮已放手引用的文件
        load_manager.Collect();

        if (frame_count >= 150)
        {
            const LoadedFileManagerStatistics stats = load_manager.GetStatistics();

            GLogInfo(u8"[AsyncTextureUpload] Test completed across 150 frames! "
                        "submitted=%llu cache_hits=%llu loaded=%llu failed=%llu cancelled=%llu "
                        "alive_files=%llu pending_release=%llu released=%llu loaded_bytes=%llu",
                     stats.submitted, stats.cache_hits,
                     stats.dispatched_loaded, stats.dispatched_failed, stats.dispatched_cancelled,
                     stats.alive_files, stats.pending_release, stats.released_files,
                     stats.io.loaded_bytes);

            #if HGL_OS == HGL_OS_Windows
            PostQuitMessage(0);
            #endif
        }
    }

    const bool IsDestroy() const override
    {
        return frame_count >= 150;
    }
};

namespace
{
    void OnUploadTaskFinished(TextureUploadTask *task, void *user_data)
    {
        if (!task)
            return;

        UploadHandler *handler = static_cast<UploadHandler *>(user_data);

        GLogInfo(u8"[AsyncTextureUpload Callback] Task completed: ID=%llu, Priority=%d, Backend=%d, %s",
                 task->task_id,
                 static_cast<int>(task->priority),
                 static_cast<int>(task->backend),
                 handler && handler->label ? handler->label : "pixel-source");

        if (handler && handler->switch_material && handler->app)
            handler->app->OnHighTextureReady(task->target_texture);
    }
}

// ---------------------------------------------------------------- UploadHandler 实现
// 说明：这些回调只会在 Tick() 里 Pump() 的线程触发，因此可以直接调用 GPU 提交接口。

void UploadHandler::OnFileLoaded(const FileLoadTicket &ticket, LoadedFile *file)
{
    loaded = true;

    if (!app || !file)
        return;

    task_id = app->GetCommitter()->CommitTexture2D(file,
                                                  false,               // 与旧示例一致：不自动生成 mipmap
                                                  upload_priority,
                                                  bindless_handle,
                                                  OnUploadTaskFinished,
                                                  this);               // user_data 指向本 handler，回调里再分发

    GLogInfo(u8"[AsyncTextureUpload] %s file ready (id=%llu, %llu bytes) -> upload task %llu",
             label, ticket.id, file->GetSize(), task_id);

    // 注意：这里不需要保留 file 的引用——Committer 已把像素同步拷入 staging 缓冲。
}

void UploadHandler::OnFileFailed(const FileLoadTicket &ticket, io::AsyncFileLoadError error)
{
    failed = true;

    GLogError(u8"[AsyncTextureUpload] %s file load failed (id=%llu, error=%d)",
              label, ticket.id, static_cast<int>(error));
}

void UploadHandler::OnFileCancelled(const FileLoadTicket &ticket)
{
    cancelled = true;

    GLogInfo(u8"[AsyncTextureUpload] %s file request cancelled (id=%llu)", label, ticket.id);
}

int os_main(int argc, os_char **argv)
{
    return RunFramework<AsyncTextureUploadApp>(OS_TEXT("Async Texture Upload Test (two-stage)"), argc, argv, 512, 512);
}