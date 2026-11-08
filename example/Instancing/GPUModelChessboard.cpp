// GPUModelChessboard —— 「GPU 常驻模型模板」的正式示例（ECS Model Template 全链路首秀）
//
// 演示内容（对应落地计划 T1~T8 的运行期形态）：
//   · 10×10 = 100 个**棋盘模型**实例摆成大方阵，远看即一张巨型棋盘格
//   · 棋盘模型 = 1 个 **GPU 模板**（3 个部件：底板 / 台面 / 中央钮，全部复用
//     内置单位立方体几何，部件间只差 local_matrix）；`RegisterModelTemplate`
//     一次进 VRAM 模板池后，**场景里再加一个棋盘 = 实体树上加一行**，
//     部件展开 / 逐实例矩阵 / 剔除全部在 GPU 内完成（entity_gpu_cull.glsl）
//   · 逐实例颜色 = T6.4 的 `InstanceCustomDataPool`：浅 / 深两行自定义色，
//     实体按 `(col+row)&1` 挂 `GPUModelRefComponent::custom_data_index` ⇒
//     **同一个模板、同一条批次**画出黑白交替，CPU 零材质分支
//   · 静态 / 可动混排：每 6 个棋盘取 1 为 `Mobility::Movable`，上下浮动，
//     演示双区 TRS 竞技场 + 前帧矩阵（Phase 7）对动目标的接管
//   · 相机绕方阵轨道旋转 ⇒ 展开实例随视锥进出，剔除账在动
//
// 判据（可自动判定）：
//   · 引擎日志 `[T3i] GPU cull draw override ENGAGED`（接管恒开，A6-d T2）
//   · `[ModelTemplateManager] 模板注册 id=... parts=3`（模板真的进了池）
//   · 帧摘要自检：`CB_SELFCHECK=1`（第 8 帧回读颜色靶 ⇒ {distinct, FNV-1a 64}
//     比对内联常量；口径与 GPUDrivenCull 一致：Debug 钉像素 / Release 钉下限）
//   · 0 VUID / 0 `[ERROR]`
//
// env：
//   CB_STATIC=1             冻结相机与浮动 ⇒ 取景确定，摘要判据不被时序干扰
//   CB_FRAMES=N             跑 N 帧后退出
//   CB_DUMP=<tag>           第 8 帧帧内回读落原始字节后退出
//   CB_SELFCHECK=1          帧摘要自检（PASS 退 0 / FAIL 退 1；自动冻结）
//   CB_ORBIT_SPEED=<deg/s>  轨道转速（默认 14）
//   CB_YAW/CB_PITCH/CB_DIST/CB_TX/CB_TY/CB_TZ  相机覆盖（取景迭代，无需重编）

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/ModelTemplateManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/graph/mesh/StaticMesh.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/InstanceCustomData.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialResolver.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/GPUModelRefComponent.h>
#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/support/cull/GPUCullSystem.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>

#include"../Shared/FrameDigest.h"   // 帧摘要自检（A6-d 判据重建共用件）

#include<glm/glm.hpp>
#include<cmath>
#include<cstdio>
#include<cstdlib>
#include<string>
#include<vector>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    // 棋盘本体全部由**内置单位立方体**（±0.5）拼出：部件只差模板内的 local_matrix。
    GeometryVertexFormat CreateCubeGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }

    // ⚠ 世界是 **Z 轴向上** ⇒ 水平面 = XY、竖向 = Z（与 GPUDrivenCull 同款地板式方阵）。
    constexpr uint  BOARD_DIM     = 10;              // 10×10 = 100 个棋盘实例
    constexpr uint  BOARD_COUNT   = BOARD_DIM * BOARD_DIM;
    constexpr float BOARD_SPACING = 1.25f;           // 底板宽 1.12 ⇒ 留 0.13 缝

    // 模板部件：底板 / 台面（略窄一圈、压在底板上）/ 中央钮（棋子意象）
    struct PartDef
    {
        const char *name;
        math::Vector3f scale;
        math::Vector3f translation;
    };

    static const PartDef kParts[] =
    {
        { "Base",    { 1.12f, 1.12f, 0.06f }, { 0.0f, 0.0f, 0.030f } },
        { "Surface", { 0.98f, 0.98f, 0.03f }, { 0.0f, 0.0f, 0.075f } },
        { "Knob",    { 0.16f, 0.16f, 0.20f }, { 0.0f, 0.0f, 0.190f } },
    };
    constexpr size_t PART_COUNT = sizeof(kParts) / sizeof(kParts[0]);

    // 逐实例自定义色：浅色格 / 深色格（材质行是白 ⇒ 最终画面色 = 本色）
    static const math::Vector4f kLightColor(0.87f, 0.83f, 0.74f, 1.0f);
    static const math::Vector4f kDarkColor (0.16f, 0.17f, 0.20f, 1.0f);

    // 可动子集：每 6 个棋盘取 1 个上下浮动（演示双区 TRS + 前帧矩阵）
    constexpr uint  MOVER_STRIDE = 6;
    constexpr float MOVER_AMP    = 0.06f;
    constexpr float MOVER_SPEED  = 2.4f;   // rad/s
}

namespace
{
    // 材质数据块：一行中性白，颜色差异全部走逐实例自定义色
    constexpr char MATERIAL_DATA_URI[] =
        "mtldata:pack(ShaderLibrary):asset/material_data/gpu_model_chessboard";
    constexpr uint32_t MATERIAL_DATA_ROW = 0;
}

class GPUModelChessboardApp : public WorkObject
{
private:

    ECSContext * ecs_context   = nullptr;
    Entity *     camera_entity = nullptr;

    // 共享几何 / 材质（模板部件与实体代表图元用**同一个** PrimitiveAsset）
    Geometry *                 cube_geometry = nullptr;
    PrimitiveAsset             cube_asset{};
    graph::mtl::MaterialRecipe board_recipe{};
    graph::GlobalSSBOBinding   base_row{};

    // GPU 模板与自定义数据行号（Init 期注册，终身有效）
    uint32_t template_id    = 0;
    uint32_t custom_light   = 0;   ///< InstanceCustomDataPool 行号（≠0，行 0 是哨兵）
    uint32_t custom_dark    = 0;

    struct Mover
    {
        TransformID tid;
        float x, y, base_z, phase;
    };
    std::vector<Mover> movers;

    double time = 0.0;

    bool  static_scene  = false;
    float orbit_speed   = 14.0f;

    std::string dump_tag;
    uint32_t    dump_frame  = 0;
    uint32_t    run_frames  = 0;
    uint32_t    frame_seen  = 0;
    bool        selfcheck       = false;
    uint32_t    selfcheck_frame = 0;

    // 帧摘要常量：**冻结场景 + 定帧 8** 下 Debug 实跑记录（CB_STATIC 口径，
    // 相机与浮动同时冻结）。改渲染结果必须同步复核并重录。
    static constexpr uint32_t kSelfW           = 1280;
    static constexpr uint32_t kSelfH           = 720;
    static constexpr uint32_t kSelfMinDistinct = 2;
    static constexpr uint64_t kSelfFNV         = 0x0C85065FA52E007Dull;  // 冻结相机 + 定帧 8 实跑记录

private:

    bool InitMaterial()
    {
        if (!GetMaterialDataBinding(MATERIAL_DATA_URI, MATERIAL_DATA_ROW, base_row))
            return false;

        const mtl::MaterialRecipe *loaded_recipe = GetRecipe("rcp:pack(ShaderLibrary):asset/recipe/Examples/Instancing/GPUModelChessboard.Board");
        if (!loaded_recipe)
            return false;
        board_recipe = *loaded_recipe;

        if (!(board_recipe.material_ssbo_binding = base_row).IsValid())
            return false;

        return true;
    }

    bool CreateCubeGeometry()
    {
        auto * device        = GetDevice();
        auto * ssbo_registry = GetManager<GlobalSSBOBufferRegistry>();

        if (!device || !ssbo_registry)
            return false;

        const GeometryVertexFormat geometry_vertex_format = CreateCubeGeometryVertexFormat();
        cube_geometry = GetGeometry("geo:builtin:cube", &geometry_vertex_format);
        if (!cube_geometry)
            return false;

        // 池行必须就位：剔除与绘制都按 geometry_id 索引 MeshDrawParams 行
        cube_geometry->EnsureMeshDrawParams(ssbo_registry, device);

        if (cube_geometry->GetGeometryID() == 0)
        {
            GLogError(u8"[GPUModelChessboard] 立方体几何未拿到 MeshDrawParams 行号（geometry_id==0）");
            return false;
        }

        return true;
    }

    /**
     * @brief 把「3 部件棋盘」打包进 GPU 模板池（一次性，之后场景侧只增实体行）。
     *
     * content 只在本函数内存在：`RegisterModelTemplate` 当场把行数据写进
     * `ModelTemplatePool` / `ModelPartPool`（VRAM 常驻），不留 CPU 引用。
     */
    bool RegisterBoardTemplate()
    {
        auto * ssbo_registry = GetManager<GlobalSSBOBufferRegistry>();
        if (!ssbo_registry)
            return false;

        ModelAssetContent content;

        // 1 个图元资产（单位立方体 + 棋盘配方），3 个节点各引用它一次 ⇒ 3 个部件行
        content.assets.emplace_back(cube_geometry, &board_recipe, PrimitiveType::Triangles);

        for (size_t p = 0; p < PART_COUNT; ++p)
        {
            StaticMeshNode node;
            node.name        = kParts[p].name;
            node.parentIndex = -1;
            node.hasTRS      = false;

            // 部件 local_matrix = 平移 ∘ 缩放（列主序：先缩后移）
            node.localMatrix = glm::translate(glm::mat4(1.0f),
                                              glm::vec3(kParts[p].translation.x,
                                                        kParts[p].translation.y,
                                                        kParts[p].translation.z));
            node.localMatrix = glm::scale(node.localMatrix,
                                          glm::vec3(kParts[p].scale.x,
                                                    kParts[p].scale.y,
                                                    kParts[p].scale.z));
            node.worldMatrix = node.localMatrix;   // 无父链 ⇒ 世界 = 局部

            // 部件包围盒在**节点自身局部空间**（单位立方体 ±0.5），
            // 展开侧再用 L2W × local_matrix 变换测试。
            node.nodeBounds.SetFromAABB(math::Vector3f(-0.5f, -0.5f, -0.5f),
                                        math::Vector3f( 0.5f,  0.5f,  0.5f));
            node.boundsValid = true;

            node.primitiveIndices.push_back(0);
            content.nodes.push_back(node);
            content.root_nodes.push_back(static_cast<int32_t>(p));
        }

        template_id = ModelTemplateManager::RegisterModelTemplate(ssbo_registry, content);
        if (template_id == 0)
        {
            printf("[ERROR] RegisterBoardTemplate: 模板注册失败\n");
            return false;
        }

        // 逐实例自定义色两行：池 reserve_rows=1 ⇒ Acquire 不会碰到哨兵行 0
        custom_light = ssbo_registry->Acquire(GlobalSSBOType::InstanceCustomDataPool);
        custom_dark  = ssbo_registry->Acquire(GlobalSSBOType::InstanceCustomDataPool);

        if (custom_light == 0 || custom_dark == 0)
        {
            printf("[ERROR] RegisterBoardTemplate: 自定义数据行发号失败\n");
            return false;
        }

        if (!ssbo_registry->Write(GlobalSSBOType::InstanceCustomDataPool, custom_light,
                                  ssbo::InstanceCustomData(kLightColor))
         || !ssbo_registry->Write(GlobalSSBOType::InstanceCustomDataPool, custom_dark,
                                  ssbo::InstanceCustomData(kDarkColor)))
        {
            printf("[ERROR] RegisterBoardTemplate: 自定义数据行写入失败\n");
            return false;
        }

        printf("[GPUModelChessboard] 模板 id=%u parts=%u｜自定义色行 light=%u dark=%u\n",
               template_id, static_cast<uint>(PART_COUNT), custom_light, custom_dark);
        fflush(stdout);

        return true;
    }

    bool InitScene()
    {
        cube_asset = PrimitiveAsset(cube_geometry, &board_recipe, PrimitiveType::Triangles);

        const float offset = float(BOARD_DIM - 1) * 0.5f * BOARD_SPACING;

        for (uint i = 0; i < BOARD_COUNT; ++i)
        {
            char name[64];
            snprintf(name, sizeof(name), "Board_%u", i);

            Entity * e = ecs_context->CreateEntity<Entity>(name);

            const uint col = i % BOARD_DIM;
            const uint row = i / BOARD_DIM;

            const float x = float(col) * BOARD_SPACING - offset;
            const float y = float(row) * BOARD_SPACING - offset;

            const bool movable = (i % MOVER_STRIDE) == 2;

            auto transform = ecs_context->GetTransform(
                ecs_context->CreateTransform(e->GetEntityID(),
                                             movable ? Mobility::Movable : Mobility::Static));
            transform.SetLocalPosition(glm::vec3(x, y, 0.0f));
            transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform.SetLocalScale(glm::vec3(1.0f));
            transform.SetMobility(movable ? Mobility::Movable : Mobility::Static);

            if (movable)
                movers.push_back({ transform.GetID(), x, y, 0.0f, float(i) * 0.7f });

            // 代表图元：模板实体**仍要被收集**（4-ID 行是展开的宿主；部件本体
            // 走模板桶组，不再由这条代表图元自己画 ⇒ PrimitiveBatchPipeline T3.1）
            auto * geometry_data = e->GetContext()->GetOrCreateGeometryData(e);
            geometry_data->GetOwner()->GetContext()
                ->GetOrCreateGeometryData(geometry_data->GetOwnerID())->SetPrimitiveAsset(&cube_asset);

            hgl::ecs::MaterialData * material_data = e->GetContext()->GetOrCreateMaterialData(e);
            hgl::ecs::MaterialData::MaterialDataAuthoringResource row_resource{};
            row_resource = base_row;
            material_data->SetDataResource(row_resource);

            // 核心：一条模板引用。加/减棋盘 = 增/删这个实体，VRAM 里的模板不动。
            auto * ref = e->AddComponent<GPUModelRefComponent>();
            ref->template_id      = template_id;
            ref->custom_data_index = ((col + row) & 1u) ? custom_dark : custom_light;
        }

        printf("[GPUModelChessboard] %u 个棋盘（%ux%u 方阵；模板 1 份 × 部件 %zu；浮动实体 %zu 个）\n",
               BOARD_COUNT, BOARD_DIM, BOARD_DIM, PART_COUNT, movers.size());
        fflush(stdout);

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");

        auto * camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target       = math::Vector3f(0.0f, 0.0f, 0.1f);
        camera->distance     = 17.0f;
        camera->yaw          = 0.0f;
        camera->pitch        = -34.0f;

        // 取景覆盖（dump 模式也走这里 ⇒ 两次运行的像素可比）
        if (const char * v = getenv("CB_YAW"))   camera->yaw      = (float)atof(v);
        if (const char * v = getenv("CB_PITCH")) camera->pitch    = (float)atof(v);
        if (const char * v = getenv("CB_DIST"))  camera->distance = (float)atof(v);
        if (const char * v = getenv("CB_TX"))    camera->target.x = (float)atof(v);
        if (const char * v = getenv("CB_TY"))    camera->target.y = (float)atof(v);
        if (const char * v = getenv("CB_TZ"))    camera->target.z = (float)atof(v);

        camera->is_main_camera = true;
        camera->matrix_dirty   = true;
        camera->viewport_info  = GetViewportInfo();

        return true;
    }

    bool InitCull()
    {
        // A6-d T2：接管已强制（opt-in 面整体删除）——这里只剩「系统在不在」这一道检查。
        auto gpu_cull = ecs_context->GetSystem<GPUCullSystem>();
        if (!gpu_cull)
        {
            printf("[ERROR] InitCull: GPUCullSystem 未注册\n");
            return false;
        }

        printf("[GPUModelChessboard] 引擎 GPU 剔除：接管已强制（A6-d T2）；模板展开恒开\n");
        fflush(stdout);

        return true;
    }

public:

    ~GPUModelChessboardApp()
    {
        // cube_geometry 由 GeometryAssetRegistry 持有，此处不再 delete。
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.06f, 0.07f, 0.10f, 1.0f));

        ecs_context = GetECSContext();
        if (!ecs_context)
        {
            printf("[ERROR] Init: no ECSContext\n");
            return false;
        }

        static_scene = getenv("CB_STATIC") != nullptr;

        if (const char * v = getenv("CB_ORBIT_SPEED")) orbit_speed = (float)atof(v);

        if (!InitMaterial())        return false;
        if (!CreateCubeGeometry())  return false;
        if (!RegisterBoardTemplate()) return false;
        if (!InitScene())           return false;
        if (!InitCamera())          return false;
        if (!InitCull())            return false;

        if (const char * tag = getenv("CB_DUMP")) dump_tag = tag;
        if (const char * v   = getenv("CB_FRAMES")) run_frames = (uint32_t)atoi(v);
        if (getenv("CB_SELFCHECK"))
        {
            selfcheck = true;

            // 摘要判据必须建在**冻结场景**上（相机 + 浮动一起冻）；缺 CB_STATIC 时自动补。
            if (!static_scene)
            {
                static_scene = true;
                printf("[CBSELF] 加固：CB_SELFCHECK=1 但未给 CB_STATIC=1 ⇒ 已**自动冻结相机与浮动**"
                       "（摘要判据需确定取景）。\n");
                fflush(stdout);
            }
        }

        return true;
    }

    void Tick(double delta_time) override
    {
        // 冻结模式：相机与浮动都定住（帧 8 的画面 = Init 姿态）⇒ 摘要判据可钉像素。
        if (!static_scene)
        {
            time += delta_time;

            if (ecs_context && camera_entity)
            {
                auto * camera = ecs_context->GetCameraByEntity(camera_entity);
                if (camera)
                {
                    camera->yaw          += orbit_speed * float(delta_time);
                    camera->matrix_dirty  = true;
                }
            }

            for (const Mover & m : movers)
            {
                auto transform = ecs_context->GetTransform(m.tid);
                transform.SetLocalPosition(glm::vec3(m.x, m.y,
                                                     m.base_z + MOVER_AMP *
                                                         float(std::sin(time * MOVER_SPEED + m.phase))));
            }
        }

        WorkObject::Tick(delta_time);

        // CB_DUMP=<tag>：第 8 帧帧内回读落原始字节后退出（同 GPUDrivenCull 先例）。
        if (!dump_tag.empty() && ++dump_frame == 8)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>(ecs_context->GetRenderTarget());
            if (!rt)
            {
                printf("[CBDUMP] no swapchain RT\n");
                std::exit(2);
            }

            std::string tag = dump_tag;
            graph::IRenderTarget * rt_ptr = rt;
            rt->SetInFrameReadbackHook([tag, rt_ptr]()
            {
                std::vector<uint8_t> pixels;
                graph::TextureReadbackInfo info;
                if (!graph::ReadbackColorTarget(rt_ptr, pixels, 0, &info) || pixels.empty())
                {
                    printf("[CBDUMP] readback failed\n");
                    std::exit(2);
                }

                char name[256];
                snprintf(name, sizeof(name), "cbdump_%s_%ux%u_%s.raw",
                         tag.c_str(), info.width, info.height,
                         GetVulkanFormat(info.format) ? GetVulkanFormat(info.format)->name : "unknown");

                FILE * fp = fopen(name, "wb");
                if (!fp)
                {
                    printf("[CBDUMP] open %s failed\n", name);
                    std::exit(2);
                }

                fwrite(pixels.data(), 1, pixels.size(), fp);
                fclose(fp);
                printf("[CBDUMP] wrote %s (%zu bytes)\n", name, pixels.size());
                std::exit(0);
            });
        }

        // CB_SELFCHECK=1：帧摘要自检（第 8 帧）；PASS 退 0 / FAIL 退 1。
        if (selfcheck && ++selfcheck_frame == 8)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>(ecs_context->GetRenderTarget());
            if (!rt)
            {
                printf("[CBSELF] no swapchain RT\n");
                std::exit(2);
            }

            ulre_example::FrameDigest expect;
            expect.width                = kSelfW;
            expect.height               = kSelfH;
            expect.distinct_byte_values = kSelfMinDistinct;
            expect.fnv1a64              = kSelfFNV;

            ulre_example::InstallFrameDigestHook(rt, expect, "CBSELF");
        }

        // CB_FRAMES=N：定帧退出（CI / 像素对照）
        if (run_frames > 0 && ++frame_seen >= run_frames)
        {
            printf("[CB] done frames=%u\n", frame_seen);
            fflush(stdout);
            std::exit(0);
        }
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<GPUModelChessboardApp>(
        OS_TEXT("GPU Model Chessboard: 10x10 boards, one GPU-resident template"),
        argc, argv, 1280, 720);
}
