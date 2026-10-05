// GPUDrivenCull —— 引擎 GPU 剔除（T3 线）的**正式示例**
//
// 演示内容：
//   · 8×8 = 64 个立方体，**同一几何 + 同一材质配方** ⇒ 单一批次
//   · 相机绕场景轨道旋转 ⇒ 立方体随视锥进出，肉眼可见剔除在动
//   · 剔除**完全由引擎承担**：`ecs::GPUCullSystem`（EntityGPU 表 + L2W 池 +
//     GeometryAABB 池 → `entity_gpu_cull.glsl` → gather 四表 → ICB 接管 + 每批计数）
//   · 示例侧**只装配实体与相机**，不建任何 compute 管线 / 缓冲 / BDA 地址表
//     ——旧写法（自建 7 张表 + 把整套经 `SetGPUDriven` 塞给引擎）已随 B5-a 外部注入
//     入口**整体退役**；那份源码停放于 `example/ApiValidation/_Obsolete/ComputeAsteroidBelt.cpp`
//     与 git 历史，供将来 ④ 实例化落地时重写参考。
//
// 判据（可自动判定）：
//   · 正向：引擎日志 `[T3i] GPU cull draw override ENGAGED: batches=x/y items=N arena_rows=M`
//     ——**必须有这条**：静默退回 CPU 账**也会**像素全等，没有它不能宣称接管成立。
//   · 像素：默认（接管） / `GC_DISPATCH_ONLY=1`（仅 dispatch，绘制仍走 CPU 账） /
//     `GC_CULL_OFF=1`（纯 CPU 账）三者 dump **逐字节全等**。
//   · 0 VUID / 0 `[ERROR]`。
//
// env（默认 = 引擎剔除全开）：
//   GC_CULL_OFF=1      关闭引擎剔除（纯 CPU 账基线，供三方像素对照）
//   GC_DISPATCH_ONLY=1 只 dispatch、不做绘制接管（供三方像素对照）
//   GC_STATIC=1        冻结相机 ⇒ 取景确定，像素判据不被时序干扰
//   GC_FRAMES=N        跑 N 帧后退出
//   GC_DUMP=<tag>      第 8 帧帧内回读落原始字节后退出（`gcdump_<tag>_WxH_fmt.raw`）
//   GC_ORBIT_SPEED=<deg/s>  轨道转速（默认 18）
//   GC_YAW/GC_PITCH/GC_DIST/GC_TX/GC_TY/GC_TZ  相机覆盖（取景迭代，无需重编）

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/color/Color.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/support/GPUCullSystem.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/vk/VKTextureReadback.h>
#include "../Shared/FrameDigest.h"   // 帧摘要自检（A6-d 判据重建共用件）

#include<glm/glm.hpp>
#include<memory>
#include<cstdio>
#include<cstdlib>
#include<string>
#include<vector>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    // 立方体顶点格式：Position + Normal（与旧 ComputeFrustumCull 的 Gizmo3D 格式同源）
    GeometryVertexFormat CreateCubeGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }

    // ⚠ 世界是 **Z 轴向上**（相机 `forward.z = sin(pitch)`、yaw 绕 Z）⇒ 水平面 = XY、竖向 = Z。
    // 本示例摆**地板式方阵**（X 纵深 × Y 横向、全部贴 Z=0），与 PBRSpheres 同款布局；
    // 相机自上方俯视 + 轨道旋转。
    constexpr uint  GRID_DIM     = 8;
    constexpr uint  CUBE_COUNT   = GRID_DIM * GRID_DIM;
    constexpr float GRID_SPACING = 2.6f;
    constexpr float CUBE_SCALE   = 0.9f;

    constexpr const COLOR TestColor[] =
    {
        COLOR::MozillaCharcoal,
        COLOR::MozillaSand,
        COLOR::BlenderAxisRed,
        COLOR::BlenderAxisGreen,
        COLOR::BlenderAxisBlue,
        COLOR::BananaYellow,
        COLOR::CherryBlossomPink,
        COLOR::SkyBlue,
    };

    constexpr const size_t COLOR_COUNT = sizeof(TestColor) / sizeof(COLOR);
}

class GPUDrivenCullApp : public WorkObject
{
private:

    ECSContext * ecs_context   = nullptr;
    Entity *     camera_entity = nullptr;

    using MaterialDataAccessor = graph::GlobalSSBODataAccessor;

    Geometry *                   cube_geometry = nullptr;
    PrimitiveAsset               cube_asset{};
    graph::mtl::MaterialRecipe   cube_recipe{};

    // 每实体一行材质行（同配方、不同颜色）——**行不同不成批**：批键 = {program, pipeline}，
    // 行只影响 per-item 材质绑定 ⇒ 仍是 **1 个批**（CoverageVariants 已验证同一现象）。
    MaterialDataAccessor color_rows[CUBE_COUNT]{};

    bool  cull_off      = false;
    bool  dispatch_only = false;
    bool  static_camera = false;
    float orbit_speed   = 18.0f;

    std::string dump_tag;
    uint32_t    dump_frame  = 0;
    uint32_t    run_frames  = 0;
    uint32_t    frame_seen  = 0;
    bool        selfcheck       = false;
    uint32_t    selfcheck_frame = 0;

    // 帧摘要常量（A6-d 判据重建试点，计划档 §10 §三.3）：**冻结相机 + 定帧**下的实跑记录。
    // 它是「删 CPU 账后画面没变」的判据：少画一批 / 错画 / 全清屏都会让摘要变。
    static constexpr uint32_t kSelfW           = 1280;
    static constexpr uint32_t kSelfH           = 720;
    static constexpr uint32_t kSelfMinDistinct = 2;      // 判据不空转守卫（全清屏 distinct==1 ⇒ FAIL）
    static constexpr uint64_t kSelfFNV         = 0x90E0745C2C7B8599ull;  // 冻结相机 + 接管下实跑记录
                                                        // （改渲染结果必须同步复核并重录）

private:

    bool InitMaterial()
    {
        auto * domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        for (uint i = 0; i < CUBE_COUNT; ++i)
        {
            color_rows[i] = domain_manager->GetAccessor<graph::ssbo::EmissiveSurfaceRow>();
            if (!color_rows[i])
                return false;

            graph::ssbo::EmissiveSurfaceRow row{};
            row.color = GetColor4f(TestColor[i % COLOR_COUNT], 1.0f);
            if (!color_rows[i].Write(row))
                return false;
        }

        cube_recipe.recipe_name = "GPUDrivenCull.Cube";
        cube_recipe.mtl_def_id  = "DebugNormalColor";
        cube_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();

        if (!(cube_recipe.material_ssbo_binding = color_rows[0].GetGlobalSSBOBinding()).IsValid())
            return false;

        return true;
    }

    bool CreateCubeGeometry()
    {
        using namespace inline_geometry;

        auto * geometry_manager = GetManager<GeometryManager>();
        auto * device           = GetDevice();
        auto * ssbo_registry    = GetManager<GlobalSSBOBufferRegistry>();

        if (!geometry_manager || !device || !ssbo_registry)
            return false;

        auto pc = std::make_unique<GeometryCreater>(device, CreateCubeGeometryVertexFormat());

        CubeCreateInfo cci;
        cci.segments_x = 1;
        cci.segments_y = 1;
        cci.segments_z = 1;

        cube_geometry = CreateCube(pc.get(), &cci);
        if (!cube_geometry)
            return false;

        // 池行必须就位：剔除与绘制都按 geometry_id 索引 MeshDrawParams 行
        //（与 GeometryAABB **一号双表**，行号 = geometry_id）。
        cube_geometry->EnsureMeshDrawParams(ssbo_registry, device);
        geometry_manager->Add(cube_geometry);

        if (cube_geometry->GetGeometryID() == 0)
        {
            GLogError(u8"[GPUDrivenCull] 立方体几何未拿到 MeshDrawParams 行号（geometry_id==0）");
            return false;
        }

        return true;
    }

    bool InitScene()
    {
        cube_asset = PrimitiveAsset(cube_geometry, &cube_recipe, PrimitiveType::Triangles);

        const float offset = float(GRID_DIM - 1) * 0.5f * GRID_SPACING;

        for (uint i = 0; i < CUBE_COUNT; ++i)
        {
            char name[64];
            snprintf(name, sizeof(name), "Cube_%u", i);

            Entity * e = ecs_context->CreateEntity<Entity>(name);

            const float x = float(i % GRID_DIM) * GRID_SPACING - offset;   // 纵深
            const float y = float(i / GRID_DIM) * GRID_SPACING - offset;   // 横向

            auto transform = ecs_context->GetTransform(
                ecs_context->CreateTransform(e->GetEntityID(), Mobility::Static));
            transform.SetLocalPosition(glm::vec3(x, y, 0.0f));
            transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform.SetLocalScale(glm::vec3(CUBE_SCALE));
            transform.SetMobility(Mobility::Static);

            auto * geometry_data = e->GetContext()->GetOrCreateGeometryData(e);
            hgl::ecs::MaterialData * material_data = e->GetContext()->GetOrCreateMaterialData(e);

            geometry_data->GetOwner()->GetContext()->GetOrCreateGeometryData(
                geometry_data->GetOwnerID())->SetPrimitiveAsset(&cube_asset);

            MaterialData::MaterialDataAuthoringResource row_resource{};
            row_resource = color_rows[i].GetGlobalSSBOBinding();
            material_data->SetDataResource(row_resource);
        }

        printf("[GPUDrivenCull] %u 立方体（%ux%u 方阵；同几何 / 同配方 ⇒ 期望 **1 个批**）\n",
               CUBE_COUNT, GRID_DIM, GRID_DIM);
        fflush(stdout);

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");

        // 相机 = 世界级资源：经世界访问器创建/取回（实体只是宿主）
        auto * camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target       = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance     = 26.0f;
        camera->yaw          = 0.0f;
        camera->pitch        = -32.0f;

        // 取景覆盖（dump 模式也走这里 ⇒ 两次运行的像素可比）
        if (const char * v = getenv("GC_YAW"))   camera->yaw      = (float)atof(v);
        if (const char * v = getenv("GC_PITCH")) camera->pitch    = (float)atof(v);
        if (const char * v = getenv("GC_DIST"))  camera->distance = (float)atof(v);
        if (const char * v = getenv("GC_TX"))    camera->target.x = (float)atof(v);
        if (const char * v = getenv("GC_TY"))    camera->target.y = (float)atof(v);
        if (const char * v = getenv("GC_TZ"))    camera->target.z = (float)atof(v);

        camera->is_main_camera = true;
        camera->matrix_dirty   = true;
        camera->viewport_info  = GetViewportInfo();

        return true;
    }

    bool InitCull()
    {
        if (cull_off)
        {
            printf("[GPUDrivenCull] GC_CULL_OFF=1 ⇒ 引擎剔除关闭（纯 CPU 账基线）\n");
            fflush(stdout);
            return true;
        }

        auto gpu_cull = ecs_context->GetSystem<GPUCullSystem>();
        if (!gpu_cull)
        {
            printf("[ERROR] InitCull: GPUCullSystem 未注册\n");
            return false;
        }

        gpu_cull->SetEnabled(true);                          // dispatch 剔除 + 计数

        if (!dispatch_only)
            gpu_cull->SetDrawOverrideEnabled(true);           // 形态 B：gather 输出接管绘制

        printf("[GPUDrivenCull] 引擎 GPU 剔除：dispatch=on、绘制接管=%s\n",
               dispatch_only ? "off（GC_DISPATCH_ONLY）" : "on");
        fflush(stdout);

        return true;
    }

public:

    ~GPUDrivenCullApp()
    {
        SAFE_CLEAR(cube_geometry)
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

        cull_off      = getenv("GC_CULL_OFF")      != nullptr;
        dispatch_only = getenv("GC_DISPATCH_ONLY") != nullptr;
        static_camera = getenv("GC_STATIC")        != nullptr;

        if (const char * v = getenv("GC_ORBIT_SPEED")) orbit_speed = (float)atof(v);

        if (!InitMaterial())       return false;
        if (!CreateCubeGeometry()) return false;
        if (!InitScene())          return false;
        if (!InitCamera())         return false;
        if (!InitCull())           return false;

        if (const char * tag = getenv("GC_DUMP")) dump_tag = tag;
        if (const char * v   = getenv("GC_FRAMES")) run_frames = (uint32_t)atoi(v);
        if (getenv("GC_SELFCHECK")) selfcheck = true;

        return true;
    }

    void Tick(double delta_time) override
    {
        // 轨道相机：让立方体随视锥进出，肉眼可见剔除在动。
        // GC_STATIC=1 冻结 ⇒ 取景确定，供像素判据使用。
        // 注意：不缓存相机组件指针（pass 相机一上场就变），每帧经世界访问器现取。
        if (!static_camera && ecs_context && camera_entity)
        {
            auto * camera = ecs_context->GetCameraByEntity(camera_entity);
            if (camera)
            {
                camera->yaw         += orbit_speed * float(delta_time);
                camera->matrix_dirty = true;
            }
        }

        WorkObject::Tick(delta_time);

        // GC_DUMP=<tag>：第 8 帧装一次性帧内回读钩子（钩子在本帧提交后、present 前执行
        // ⇒ 读到本帧画面），落原始 packed 字节后退出（同 CoverageVariants/AlphaModes 先例）。
        if (!dump_tag.empty() && ++dump_frame == 8)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>(ecs_context->GetRenderTarget());
            if (!rt)
            {
                printf("[GCDUMP] no swapchain RT\n");
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
                    printf("[GCDUMP] readback failed\n");
                    std::exit(2);
                }

                char name[256];
                snprintf(name, sizeof(name), "gcdump_%s_%ux%u_%s.raw",
                         tag.c_str(), info.width, info.height,
                         GetVulkanFormat(info.format) ? GetVulkanFormat(info.format)->name : "unknown");

                FILE * fp = fopen(name, "wb");
                if (!fp)
                {
                    printf("[GCDUMP] open %s failed\n", name);
                    std::exit(2);
                }

                fwrite(pixels.data(), 1, pixels.size(), fp);
                fclose(fp);
                printf("[GCDUMP] wrote %s (%zu bytes)\n", name, pixels.size());
                std::exit(0);
            });
        }

        // GC_SELFCHECK=1：**帧摘要自检**（A6-d 判据重建，计划档 §10 §三.3；共用件 example/Shared/FrameDigest.h）——
        // 第 8 帧帧内回读颜色靶 ⇒ {不同字节取值数, FNV-1a 64} 比对下方内联常量：
        // PASS 退出 0 / FAIL 退出 1。判据必须建在**冻结相机 + 定帧**上（GC_STATIC=1）。
        if (selfcheck && ++selfcheck_frame == 8)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>(ecs_context->GetRenderTarget());
            if (!rt)
            {
                printf("[GCSELF] no swapchain RT\n");
                std::exit(2);
            }

            ulre_example::FrameDigest expect;
            expect.width                = kSelfW;
            expect.height               = kSelfH;
            expect.distinct_byte_values = kSelfMinDistinct;
            expect.fnv1a64              = kSelfFNV;

            ulre_example::InstallFrameDigestHook(rt, expect, "GCSELF");
        }

        // GC_FRAMES=N：定帧退出（CI / 像素对照）
        if (run_frames > 0 && ++frame_seen >= run_frames)
        {
            printf("[GC] done frames=%u\n", frame_seen);
            fflush(stdout);
            std::exit(0);
        }
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<GPUDrivenCullApp>(
        OS_TEXT("GPU-Driven Frustum Cull: 8x8 cubes, engine GPUCullSystem takeover"),
        argc, argv, 1280, 720);
}