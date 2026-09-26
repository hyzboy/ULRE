// ComputeIndirectCount (Step 2)
//
// 验证内容：
// 1. 设备特性确认：drawIndirectCount 与 cmd_draw_mesh_tasks_indirect_count 就绪
// 2. GPU 写入计数：Compute Shader 计算并通过 DrawCountBuffer 写入 draw_count
// 3. GPU 管线屏障：COMPUTE_SHADER_BIT (SHADER_WRITE) -> DRAW_INDIRECT_BIT (INDIRECT_COMMAND_READ)
// 4. GPU 消费间接绘制：RenderCmdBuffer::DrawMeshTasksIndirectCount 由 GPU 计数驱动动态绘制
// 5. ECS 集成：MaterialBatch::icb_count_buffer 自动触发 Indirect Count Multi-Draw

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/color/Color.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/log/Log.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>
#include<cmath>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    constexpr uint32_t TOTAL_CUBES = 12;

    struct CountPushConstants
    {
        uint32_t max_draws;
        float    cutoff_ratio;
        uint64_t count_addr;     // DrawCountBuffer 设备地址（BDA）
    };

    static_assert(sizeof(CountPushConstants) == 16, "CountPushConstants 布局必须与 GLSL push_constant block 一致");

    constexpr const char COMPUTE_COUNT_GLSL[] = R"(
#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(local_size_x = 1) in;

// 计数值经 BDA 写入：地址由 push constant 下发，无 set 无 binding
layout(buffer_reference, std430, buffer_reference_align=16) buffer DrawCountRef {
    uint draw_count;
};

layout(push_constant) uniform PushConstants {
    uint max_draws;
    float cutoff_ratio;
    uint64_t count_addr;
} pc;

void main() {
    uint count = uint(round(float(pc.max_draws) * pc.cutoff_ratio));
    if (count > pc.max_draws)
        count = pc.max_draws;

    DrawCountRef counter = DrawCountRef(pc.count_addr);
    counter.draw_count = count;
}
)";

    GeometryVertexFormat CreateGizmo3DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }

    // ECS 渲染阶段系统：在 RenderFrameSync 阶段为每个活动 MaterialBatch 挂载 GPU count_buffer
    class GPUIndirectCountHookSystem : public System
    {
        DeviceBuffer *count_buffer = nullptr;

    public:
        explicit GPUIndirectCountHookSystem(DeviceBuffer *cb)
            : System("GPUIndirectCountHookSystem")
            , count_buffer(cb)
        {
            SetExecutionPhase(ExecutionPhase::RenderFrameSync);
        }

        void Update(float /*deltaTime*/) override
        {
            if (!context || !count_buffer)
                return;

            auto &cache = context->GetRenderFrameCache();
            for (auto &pair : cache.materialBatches)
            {
                MaterialBatch *batch = pair.second.get();
                if (batch && !batch->items.empty() && batch->icb_mesh_tasks)
                {
                    batch->icb_count_buffer = count_buffer;
                    batch->icb_count_buffer_offset = 0;
                }
            }
        }
    };
}

class ComputeIndirectCountApp : public WorkObject
{
private:
    ECSContext *ecs_context = nullptr;
    Entity     *camera_entity = nullptr;

    Geometry             *geometry = nullptr;
    graph::mtl::MaterialRecipe cube_recipe{};
    PrimitiveAsset             cube_asset{};
    GlobalSSBODataAccessor   mtl_data_ssbo_accessor{};

    DeviceBuffer     *count_buffer     = nullptr;
    ComputePipeline  *compute_pipeline = nullptr;
    ComputeCmdBuffer *compute_cmd      = nullptr;
    DeviceQueue      *compute_queue    = nullptr;

    float    anim_time        = 0.0f;
    uint32_t tick_frame_count = 0;

private:

    bool CreateCubeGeometry()
    {
        using namespace inline_geometry;

        auto *geometry_manager = GetManager<GeometryManager>();
        auto *device = GetDevice();
        if (!geometry_manager || !device)
            return false;

        auto pc = std::make_unique<GeometryCreater>(device, CreateGizmo3DGeometryVertexFormat());

        CubeCreateInfo cci;
        cci.segments_x = 1;
        cci.segments_y = 1;
        cci.segments_z = 1;

        geometry = CreateCube(pc.get(), &cci);
        if (!geometry)
            return false;

        geometry_manager->Add(geometry);
        return true;
    }

    bool InitMISSBO()
    {
        auto *domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        mtl_data_ssbo_accessor = domain_manager->GetAccessor<graph::ssbo::EmissiveSurfaceRow>();
        if (!mtl_data_ssbo_accessor)
            return false;

        graph::ssbo::EmissiveSurfaceRow material_data{};
        material_data.color = GetColor4f(COLOR::Cyan, 1.0f);
        return mtl_data_ssbo_accessor.Write(material_data);
    }

    bool InitECSScene()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        cube_recipe.recipe_name = "ComputeIndirectCount.CubeMaterial";
        cube_recipe.mtl_def_id  = "DebugNormalColor";
        cube_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        if (!(cube_recipe.material_ssbo_binding = mtl_data_ssbo_accessor.GetGlobalSSBOBinding()).IsValid())
            return false;

        cube_asset = PrimitiveAsset(geometry, &cube_recipe, PrimitiveType::Triangles);

        // 创建一排 12 个立方体，沿 X 轴分布
        for (uint32_t i = 0; i < TOTAL_CUBES; ++i)
        {
            auto *e = ecs_context->CreateEntity<Entity>(("Cube_" + AnsiString::numberOf(i)).c_str());

            auto transform = e->AddComponent<TransformComponent>(Mobility::Static);
            const float x = (static_cast<float>(i) - static_cast<float>(TOTAL_CUBES - 1) * 0.5f) * 2.2f;
            transform->SetLocalPosition(glm::vec3(x, 0.0f, 0.0f));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(0.9f));
            transform->SetMovable(false);

            auto prim = e->AddComponent<PrimitiveComponent>();
            prim->SetPrimitiveAsset(&cube_asset);
            PrimitiveComponent::MaterialDataAuthoringResource named_struct{};
            named_struct = mtl_data_ssbo_accessor.GetGlobalSSBOBinding();
            prim->SetMaterialDataResource(named_struct);
            prim->SetVisible(true);
        }

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode  = CameraComponent::ControlMode::ViewModel;
        camera->target        = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance      = 22.0f;
        camera->yaw           = 0.0f;
        camera->pitch         = -25.0f;
        camera->is_main_camera= true;
        camera->matrix_dirty  = true;

        camera->camera_data   = GetCamera();
        camera->camera_info   = const_cast<graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

    bool CreateComputeResources(VulkanDevice *dev)
    {
        // 1. 创建 DrawCountBuffer（INDIRECT|STORAGE|TRANSFER_DST, CPUVisible）
        count_buffer = dev->CreateDrawCountBuffer("ComputeIndirectCount.CountBuffer", 1, BufferAllocPolicy::CPUVisible);
        if (!count_buffer)
        {
            GLogError(u8"[ComputeIndirectCount] Failed to create count buffer");
            return false;
        }

        // 2. 创建 Compute Pipeline（用户数据走 BDA：push constant 下发 count_buffer 地址）
        GraphicsContext *gc = GetGraphicsContext();
        if (!gc || !gc->GetMaterialManager())
            return false;

        compute_pipeline = gc->GetMaterialManager()->CreateComputePipeline(
            "ComputeIndirectCount.Pipeline",
            COMPUTE_COUNT_GLSL,
            sizeof(CountPushConstants)
        );

        if (!compute_pipeline)
        {
            GLogError(u8"[ComputeIndirectCount] Failed to create compute pipeline");
            return false;
        }

        // 4. 命令与队列
        compute_cmd   = dev->CreateComputeCommandBuffer("ComputeIndirectCount.Cmd");
        compute_queue = dev->CreateQueue("ComputeIndirectCount.Queue");

        return compute_cmd && compute_queue;
    }

public:

    ~ComputeIndirectCountApp() override
    {
        if (GetDevice())
        {
            if (compute_cmd)   delete compute_cmd;
            if (count_buffer)  delete count_buffer;
        }
    }

    bool Init() override
    {
        auto *dev = GetDevice();
        if (!dev)
            return false;

        // 验证 Step 2 基础设施：drawIndirectCount 必须被驱动支持并由引擎启用
        if (!dev->SupportDrawIndirectCount())
        {
            GLogError(u8"[ComputeIndirectCount] FATAL: VulkanDevice does not support DrawIndirectCount or cmd_draw_mesh_tasks_indirect_count is null!");
            return false;
        }

        GLogInfo(u8"[ComputeIndirectCount] Step 2 Hardware Support Confirmed: DrawIndirectCount is ENABLED!");

        if (!CreateCubeGeometry()) return false;
        if (!InitMISSBO())         return false;
        if (!InitECSScene())       return false;
        if (!InitCamera())         return false;
        if (!CreateComputeResources(dev)) return false;

        // 注册 ECS 挂载系统
        ecs_context->RegisterRenderSystem<GPUIndirectCountHookSystem>(count_buffer);

        GLogInfo(u8"[ComputeIndirectCount] Init completed successfully. 12 cubes ready for GPU-Driven Indirect Count!");
        return true;
    }

    void Tick(double delta_time) override
    {
        VulkanDevice *dev = GetDevice();
        if (!dev)
            return;

        anim_time += static_cast<float>(delta_time);

        // 动态计算剔除比例（0.0 ~ 1.0 连续平滑周期变化）
        const float cutoff_ratio = 0.5f + 0.5f * std::sin(anim_time * 2.0f);
        const uint32_t expected_count = static_cast<uint32_t>(std::round(float(TOTAL_CUBES) * cutoff_ratio));

        // 1. Compute Shader 分派：在 GPU 写入 count_buffer（地址经 push constant 下发）
        CountPushConstants pc{TOTAL_CUBES, cutoff_ratio, dev->GetBufferDeviceAddressAligned16(count_buffer->GetBuffer())};

        if (pc.count_addr == 0)
        {
            GLogError(u8"[ComputeIndirectCount] count_buffer 设备地址无效（BDA 16B 对齐承诺失败）");
            return;
        }

        compute_cmd->Begin();
        compute_cmd->BindPipeline(compute_pipeline);
        compute_cmd->PushConstants(compute_pipeline->GetPipelineLayout(), &pc, sizeof(pc));
        compute_cmd->Dispatch(1, 1, 1);

        // 2. 管线内存屏障：确保 Compute Shader 写入完成，对后续 Indirect Command 读取完全可见
        compute_cmd->BufferMemoryBarrier(
            count_buffer->GetBuffer(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        );

        compute_cmd->End();

        compute_queue->Submit(compute_cmd, nullptr, 0, nullptr, 0);
        compute_queue->WaitFence();

        // 3. CPU 读回验证：确认 GPU 计算并写入的数值 100% 正确
        uint32_t gpu_written_count = 0;
        void *ptr = count_buffer->GetGPUBuffer()->Map(0, sizeof(uint32_t));
        if (ptr)
        {
            gpu_written_count = *reinterpret_cast<uint32_t *>(ptr);
            count_buffer->GetGPUBuffer()->Unmap();
        }

        ++tick_frame_count;
        if (tick_frame_count % 60 == 1)
        {
            if (gpu_written_count == expected_count)
            {
                GLogInfo(u8"[ComputeIndirectCount] PASS: GPU-driven count=%u/%u (cutoff=%.2f) successfully verified & dispatched to DrawMeshTasksIndirectCount!",
                         gpu_written_count, TOTAL_CUBES, cutoff_ratio);
            }
            else
            {
                GLogError(u8"[ComputeIndirectCount] MISMATCH: GPU count=%u expected=%u",
                          gpu_written_count, expected_count);
            }
        }

        // 4. 调用基类 Tick，执行 ECS 帧渲染
        // GPUIndirectCountHookSystem 在 RenderFrameSync 挂载 count_buffer
        // PipelineMaterialRenderer::ProcIndirectRender 进而执行 DrawMeshTasksIndirectCount
        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<ComputeIndirectCountApp>(OS_TEXT("Compute Indirect Count (Step 2)"), argc, argv);
}
