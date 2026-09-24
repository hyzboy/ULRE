// 异步纹理上传与优先级队列、撤销及 Bindless 动态更新示例 (ECS)
#include <hgl/framework/WorkManager.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/module/TextureUploadQueue.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/ShaderProgramManager.h>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/vk/VKBindlessTextureManager.h>
#include <hgl/log/Log.h>

// ECS headers
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/ecs/components/PrimitiveComponent.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

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

    void OnUploadTaskFinished(TextureUploadTask *task, void *user_data)
    {
        if (!task)
            return;

        GLogInfo(u8"[AsyncTextureUpload Callback] Task completed: ID=%llu, Priority=%d, Backend=%d",
                 task->task_id, static_cast<int>(task->priority), static_cast<int>(task->backend));
    }

    void OnHighUploadFinished(TextureUploadTask *task, void *user_data);
}

class TestApp : public WorkObject
{
public:
    void OnHighTextureReady(Texture *tex)
    {
        if (tex && quad_primitive && sampler)
        {
            quad_primitive->SetMaterialTextureResource("base_color", tex, sampler);
            switched_texture = true;
            GLogInfo(u8"[AsyncTextureUpload] Dynamically switched Quad texture to loaded high priority lena.Tex2D!");
        }
    }
private:
    ECSContext *        ecs_world           = nullptr;
    Entity *            quad_entity         = nullptr;
    PrimitiveComponent *quad_primitive      = nullptr;

    Texture2D *         placeholder_tex     = nullptr;
    Sampler *           sampler             = nullptr;
    mtl::MaterialRecipe quad_recipe{};
    PrimitiveAsset      quad_asset{};

    uint64_t            immediate_task_id   = 0;
    uint64_t            high_task_id        = 0;
    uint64_t            normal_task_id      = 0;
    uint64_t            low_task_id         = 0;
    uint64_t            cancelled_task_id   = 0;

    uint32_t            bindless_handle     = 0;
    uint32_t            frame_count         = 0;
    bool                switched_texture    = false;

private:
    bool InitMaterial()
    {
        auto *sampler_manager = GetManager<SamplerManager>();
        auto *tex_manager = GetManager<TextureManager>();
        if (!sampler_manager || !tex_manager)
            return false;

        // 加载占位小纹理
        placeholder_tex = tex_manager->LoadTexture2D(OS_TEXT("res/image/Lena.Tex2D"), false);
        if (!placeholder_tex)
        {
            GLogError(u8"Failed to load placeholder texture Lena.Tex2D");
            return false;
        }

        sampler = sampler_manager->CreateSampler();
        if (!sampler)
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
        auto quad_transform = quad_entity->AddComponent<TransformComponent>(Mobility::Static);
        quad_primitive = quad_entity->AddComponent<PrimitiveComponent>().get();

        quad_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        quad_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        quad_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        quad_transform->SetMovable(false);

        quad_primitive->SetPrimitiveAsset(&quad_asset);
        if (!quad_primitive->SetMaterialTextureResource(
                "base_color",
                placeholder_tex,
                sampler))
            return false;
        quad_primitive->SetVisible(true);

        return true;
    }

    bool InitAsyncUploads()
    {
        auto *tex_manager = GetManager<TextureManager>();
        if (!tex_manager)
            return false;

        // 1. Immediate 任务（同步直达，立即就绪）
        immediate_task_id = tex_manager->LoadTexture2DAsync(
            OS_TEXT("res/image/noise32.Tex2D"),
            false,
            UploadPriority::Immediate,
            0,
            OnUploadTaskFinished,
            nullptr);
        GLogInfo(u8"[AsyncTextureUpload] Enqueued Immediate task ID = %llu", immediate_task_id);

        // 2. High 优先级任务（关联预分配的 bindless_handle，完成后原子刷新该槽位）
        high_task_id = tex_manager->LoadTexture2DAsync(
            OS_TEXT("res/image/lena.Tex2D"),
            false,
            UploadPriority::High,
            bindless_handle,
            OnHighUploadFinished,
            this);
        GLogInfo(u8"[AsyncTextureUpload] Enqueued High priority task ID = %llu (bindless=%u)",
                 high_task_id, bindless_handle);

        // 3. Normal 优先级任务
        normal_task_id = tex_manager->LoadTexture2DAsync(
            OS_TEXT("res/image/Gear.Tex2D"),
            false,
            UploadPriority::Normal,
            0,
            OnUploadTaskFinished,
            nullptr);
        GLogInfo(u8"[AsyncTextureUpload] Enqueued Normal priority task ID = %llu", normal_task_id);

        // 4. Low 优先级任务
        low_task_id = tex_manager->LoadTexture2DAsync(
            OS_TEXT("res/image/PeachHouse.Tex2D"),
            false,
            UploadPriority::Low,
            0,
            OnUploadTaskFinished,
            nullptr);
        GLogInfo(u8"[AsyncTextureUpload] Enqueued Low priority task ID = %llu", low_task_id);

        // 5. 撤销/丢弃测试任务（入队后立即撤销）
        cancelled_task_id = tex_manager->LoadTexture2DAsync(
            OS_TEXT("res/image/PeachWine.Tex2D"),
            false,
            UploadPriority::Low,
            0,
            OnUploadTaskFinished,
            nullptr);
        GLogInfo(u8"[AsyncTextureUpload] Enqueued task for cancellation ID = %llu", cancelled_task_id);

        bool cancel_ok = tex_manager->CancelUpload(cancelled_task_id);
        UploadTaskState state = tex_manager->GetUploadState(cancelled_task_id);
        GLogInfo(u8"[AsyncTextureUpload] CancelUpload result = %s, current state = %d",
                 cancel_ok ? "SUCCESS" : "FAILED", static_cast<int>(state));

        // 6. 内存像素贴图任务（验证环形 Staging 显存复用池切片分配）
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

        uint64_t mem_task_id = tex_manager->CreateTexture2DAsync(
            mem_tci,
            UploadPriority::Normal,
            0,
            OnUploadTaskFinished,
            nullptr);
        GLogInfo(u8"[AsyncTextureUpload] Enqueued Memory Pixels Ring Task ID = %llu", mem_task_id);

        return true;
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

        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        ++frame_count;

        auto *tex_manager = GetManager<TextureManager>();
        if (!tex_manager)
            return;

        // 每 30 帧检查一次任务状态
        if (frame_count % 30 == 0)
        {
            UploadTaskState imm_state  = tex_manager->GetUploadState(immediate_task_id);
            UploadTaskState high_state = tex_manager->GetUploadState(high_task_id);
            UploadTaskState norm_state = tex_manager->GetUploadState(normal_task_id);
            UploadTaskState low_state  = tex_manager->GetUploadState(low_task_id);

            GLogInfo(u8"[AsyncTextureUpload Frame %u] Task States: Imm=%d, High=%d, Norm=%d, Low=%d",
                     frame_count,
                     static_cast<int>(imm_state),
                     static_cast<int>(high_state),
                     static_cast<int>(norm_state),
                     static_cast<int>(low_state));

            // 当 High 任务完成后，动态切换材质纹理以验证画面
            if (!switched_texture && high_state == UploadTaskState::Completed)
            {
                auto *queue = tex_manager->GetUploadQueue();
                if (queue)
                {
                    auto *task = queue->FindTask(high_task_id);
                    if (task && task->target_texture && quad_primitive)
                    {
                        quad_primitive->SetMaterialTextureResource("base_color", task->target_texture, sampler);
                        switched_texture = true;
                        GLogInfo(u8"[AsyncTextureUpload] Switched Quad texture to high priority lena.Tex2D!");
                    }
                }
            }
        }

        if (frame_count >= 150)
        {
            GLogInfo(u8"[AsyncTextureUpload] Test completed successfully across 150 frames! Exiting.");
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
    void OnHighUploadFinished(TextureUploadTask *task, void *user_data)
    {
        if (!task)
            return;

        OnUploadTaskFinished(task, user_data);

        if (user_data)
        {
            TestApp *app = static_cast<TestApp *>(user_data);
            app->OnHighTextureReady(task->target_texture);
        }
    }
}

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Async Texture Upload Test"), argc, argv, 512, 512);
}
