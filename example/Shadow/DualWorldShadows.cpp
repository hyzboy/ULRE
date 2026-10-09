/**
 * DualWorldShadows —— 两世界同帧 · 光照/阴影数据世界私有隔离台架（设计档 V7）
 *
 * 判据（`DWS_SELFCHECK=1`，全自动、单进程、退出码给结论）：
 *   同一 app 帧里两个世界各自渲染：主世界（交换链）+ 离屏世界（OffscreenWorld 的 RT）。
 *   两个世界各有**自己的**主光方向与 **自己的** CSM（影子数据），同一帧各自出图。
 *
 *     S0（基线）            : A = d1 , B = d2
 *     ── 翻转**离屏世界**的主光 ──
 *     S1：断言 ① A 的摘要**逐位不变**（B 的光改变了，A 的画面不受影响）
 *              ② B 的摘要**必须改变**（守卫：证明这次翻转真的作用到了 B）
 *     ── 翻转**主世界**的主光 ──
 *     S2：断言 ③ B 的摘要**逐位不变**（A 的光改变了，B 的画面不受影响）
 *              ④ A 的摘要**必须改变**
 *
 *   ②④ 是"判据不空转"守卫；①③ 才是隔离判据 —— 数据若退回**共享载体**
 *   （光源行表 / 影子数据挂 env profile 或设备级单例），翻转一个世界的光就会改到另一个世界的
 *   画面，①③ 必红。⇒ 这条台架是 T6b/P4「世界私有」的产品判据（结构契约见 Test 22/46/47）。
 *
 * 本例顺带钉住两条引擎前提（V7 实测暴露出来的）：
 *   · 级联 RT 数 = `CascadedShadowConfig::cascade_count`（离屏帧槽带全设备共 4 条，
 *     建满 4 条则"两个世界各自开 CSM"在容量上不可达）；
 *   · 影子数据的设备上传点 = **本世界自己的帧 RT**（不只交换链）——否则离屏世界的
 *     `addr_shadow` 永远是零值、离屏世界不可能有阴影。
 *
 * 不跑自检时：两个世界的主光各自缓慢旋转（相位不同）⇒ 肉眼可同时看到两套独立阴影。
 */

#include<hgl/framework/WorkManager.h>      // WorkObject / RunFramework
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/graph/module/OffscreenWorld.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/graph/render/shadow/CascadedShadowController.h>
#include<hgl/color/Color.h>
#include<hgl/log/Log.h>
#include<hgl/mtl/MaterialRecipe.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/systems/render/LightSystem.h>
#include<hgl/ecs/systems/render/MainLightShadowSystem.h>
#include<hgl/ecs/systems/render/ViewUBOCommitSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include "../Shared/FrameDigest.h"   // 帧摘要（判据共用件：ComputeFrameDigest 对任意 RT 回读）

#include<memory>
#include<cmath>
#include<cstdio>
#include<cstdlib>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateStandardGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }

    bool LogStageFail(const char *stage, const char *reason)
    {
        GLogError("[DualWorldShadows][%s] %s", stage, reason);
        return false;
    }

    void LogStage(const char *stage, const char *message)
    {
        GLogInfo("[DualWorldShadows][%s] %s", stage, message);
    }

    /// 两世界的主光方向（**指向光源**，与 shader 侧 `main_light.direction_intensity.xyz` 同约定）
    /// @note `math::Vector3f` 不是字面类型 ⇒ 用 `const`（匿名命名空间内 = 内部链接，等价只读）
    const math::Vector3f kMainLightDirA  ( 0.55f,  0.35f, 0.75f);
    const math::Vector3f kMainLightDirB  (-0.65f,  0.45f, 0.62f);
    /// 翻转后的方向（自检用；两个世界各翻一次，用于证明"翻转真的作用到了自己"）
    const math::Vector3f kMainLightDirA2 (-0.50f, -0.70f, 0.50f);
    const math::Vector3f kMainLightDirB2 ( 0.70f,  0.80f, 0.35f);
}

class DualWorldShadowsApp final : public WorkObject
{
private:
    ECSContext *ecs_context = nullptr;

    // ── 共享几何 / 材质（几何与材质行是**设备级**资源，两个世界共用；与"世界私有"无关）──
    Geometry *shape_geometry[4] = {};        ///< 0=cube 1=sphere 2=cylinder 3=torus
    PrimitiveAsset shape_asset[4];
    Geometry *ground_geometry = nullptr;
    PrimitiveAsset ground_asset;

    graph::mtl::MaterialRecipe lit_recipe{};
    graph::GlobalSSBODataAccessor material_accessor{};
    graph::ssbo::PBRSurfaceRow    material_row{};
    Sampler *common_sampler = nullptr;

    std::shared_ptr<MainLightShadowSystem> main_csm;
    std::shared_ptr<MainLightShadowSystem> child_csm;

    // ── 离屏世界（第二个 ECS 世界）──
    std::unique_ptr<graph::OffscreenWorld> child;
    ECSContext *child_world = nullptr;
    PrimitiveAsset child_screen_asset;
    Geometry *child_screen_geometry = nullptr;
    Entity *child_screen_entity = nullptr;

    // ── 隔离自检状态 ──
    bool     selfcheck = false;
    bool     fail      = false;
    uint32_t frame_index = 0;

    struct Sample { uint64_t a = 0; uint64_t b = 0; uint32_t da = 0; uint32_t db = 0; };
    Sample s0, s1, s2;

private:

    // ─────────────────────────────────────────────────────────────
    // 世界级配置（光 / 阴影）—— 两个世界各调一次，各写**自己的**世界
    // ─────────────────────────────────────────────────────────────

    bool ConfigureWorldLight(ECSContext *world, const math::Vector3f &to_light, float intensity)
    {
        if (!world)
            return LogStageFail("ConfigureWorldLight", "world is null");

        auto light_system = world->GetSystem<LightSystem>();
        if (!light_system)
            light_system = world->RegisterSystem<LightSystem>();

        if (!light_system)
            return LogStageFail("ConfigureWorldLight", "LightSystem unavailable");

        // 写的是**本世界**的光源行（世界私有表；世界 A 的写入不影响世界 B）
        light_system->SetMainLightDirection(to_light);
        light_system->SetMainLightColor(Color4f(1.0f, 0.94f, 0.88f, 1.0f));
        light_system->SetMainLightIntensity(intensity);
        light_system->SetMainLightAngleDeg(0.53f);
        return true;
    }

    bool EnableWorldCSM(ECSContext *world, const char *tag, uint32_t cascade_count, float max_distance)
    {
        if (!world)
            return LogStageFail(tag, "world is null");

        auto csm = world->GetSystem<MainLightShadowSystem>();
        if (!csm)
            csm = world->RegisterSystem<MainLightShadowSystem>();

        if (!csm)
            return LogStageFail(tag, "MainLightShadowSystem unavailable");

        CascadedShadowConfig cfg{};
        cfg.cascade_count      = cascade_count;      // ★ 级联 RT 数按此值创建（离屏帧槽带容量）
        cfg.max_distance       = max_distance;
        cfg.split_distances[0] = max_distance;       // 单级联 ⇒ 它自己覆盖全距离
        cfg.use_custom_splits  = true;
        cfg.c0_dynamic_overlay = false;              // 单级联 = 常规级联（不是"近景动态层"）
        cfg.shadow_map_size    = 1024.0f;
        cfg.caster_depth_margin= 120.0f;
        cfg.bias               = -0.003f;
        cfg.pcf_radius         = 1.5f;
        cfg.darkness           = 0.15f;
        cfg.blend_width        = 0.05f;

        if (!csm->EnableMainLightShadow(cfg, 1024))
            return LogStageFail(tag, "EnableMainLightShadow failed");

        GLogInfo("[DualWorldShadows][%s] CSM 就绪：级联数=%u（占 %u 条离屏帧槽带）",
                 tag, cascade_count, cascade_count);

        if (world == ecs_context)
            main_csm = csm;
        else
            child_csm = csm;

        return true;
    }

    // ─────────────────────────────────────────────────────────────
    // 资源（一次建好，两个世界共用）
    // ─────────────────────────────────────────────────────────────

    bool CreateSharedResources()
    {
        auto *gc = GetGraphicsContext();
        if (!gc)
            return LogStageFail("CreateSharedResources", "graphics context is null");

        auto *gm      = GetManager<GeometryManager>();
        auto *sm      = GetManager<SamplerManager>();
        auto *device  = gc->GetDevice();
        auto *registry = gc->GetGlobalSSBOBufferRegistry();
        if (!gm || !sm || !device || !registry)
            return LogStageFail("CreateSharedResources", "manager/sampler/device/registry missing");

        common_sampler = sm->CreateSampler();
        if (!common_sampler)
            return LogStageFail("CreateSharedResources", "CreateSampler failed");

        // 材质行（设备级全局池）：所有本台架实体共用一行
        material_accessor = registry->GetAccessor<graph::ssbo::PBRSurfaceRow>();
        if (!material_accessor)
            return LogStageFail("CreateSharedResources", "material row allocator failed");

        material_row.base_color  = Color4f(0.78f, 0.78f, 0.80f, 1.0f);
        material_row.metallic    = 0.05f;
        material_row.roughness   = 0.85f;
        material_row.normal_scale= 0.0f;
        if (!material_accessor.Write(material_row))
            return LogStageFail("CreateSharedResources", "material row write failed");

        lit_recipe.recipe_name = "DualWorldShadows.Lit";
        lit_recipe.mtl_def_id  = "Lit";
        lit_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        if (!(lit_recipe.material_ssbo_binding = material_accessor.GetGlobalSSBOBinding()).IsValid())
            return LogStageFail("CreateSharedResources", "material SSBO binding invalid");

        auto create_geometry = [&](auto &&creator) -> Geometry *
        {
            auto pc = std::make_unique<GeometryCreater>(device, CreateStandardGeometryVertexFormat());
            return pc ? creator(pc.get()) : nullptr;
        };

        shape_geometry[0] = create_geometry([](GeometryCreater *pc)
        {
            inline_geometry::CubeCreateInfo cci;
            cci.segments_x = 1; cci.segments_y = 1; cci.segments_z = 1;
            return inline_geometry::CreateCube(pc, &cci);
        });
        shape_geometry[1] = create_geometry([](GeometryCreater *pc)
        {
            return inline_geometry::CreateSphere(pc, 48);
        });
        shape_geometry[2] = create_geometry([](GeometryCreater *pc)
        {
            inline_geometry::CylinderCreateInfo cci;
            cci.radius = 0.8f; cci.halfExtend = 1.2f; cci.numberSlices = 32;
            return inline_geometry::CreateCylinder(pc, &cci);
        });
        shape_geometry[3] = create_geometry([](GeometryCreater *pc)
        {
            inline_geometry::TorusCreateInfo tci;
            tci.innerRadius = 0.6f; tci.outerRadius = 1.1f;
            tci.numberSlices = 64; tci.numberStacks = 20;
            return inline_geometry::CreateTorus(pc, &tci);
        });
        ground_geometry = create_geometry([](GeometryCreater *pc)
        {
            return inline_geometry::CreatePlaneSqaure(pc);
        });

        for (uint32_t i = 0; i < 4; ++i)
        {
            if (!shape_geometry[i])
                return LogStageFail("CreateSharedResources", "shape geometry creation failed");

            gm->Add(shape_geometry[i]);
            shape_asset[i] = PrimitiveAsset(shape_geometry[i], &lit_recipe, PrimitiveType::Triangles);
            if (!shape_asset[i].IsValid())
                return LogStageFail("CreateSharedResources", "shape primitive asset invalid");
        }

        if (!ground_geometry)
            return LogStageFail("CreateSharedResources", "ground geometry creation failed");

        gm->Add(ground_geometry);
        ground_asset = PrimitiveAsset(ground_geometry, &lit_recipe, PrimitiveType::Triangles);
        if (!ground_asset.IsValid())
            return LogStageFail("CreateSharedResources", "ground primitive asset invalid");

        // 展示离屏世界画面的屏幕（主世界里的 Static 四边形；Static ⇒ 不进 cascade 0 的 caster 集）
        child_screen_geometry = create_geometry([](GeometryCreater *pc)
        {
            return inline_geometry::CreatePlaneSqaure(pc);
        });
        if (!child_screen_geometry)
            return LogStageFail("CreateSharedResources", "screen geometry creation failed");

        gm->Add(child_screen_geometry);
        child_screen_asset = PrimitiveAsset(child_screen_geometry, &lit_recipe, PrimitiveType::Triangles);
        if (!child_screen_asset.IsValid())
            return LogStageFail("CreateSharedResources", "screen primitive asset invalid");

        return true;
    }

    /// 在 `world` 里放一个本台架的实体（几何 = 共享 asset；材质行 = 共享行）
    bool PlaceEntity(ECSContext *world,
                     const char *name,
                     const PrimitiveAsset &asset,
                     const math::Vector3f &position,
                     float scale,
                     Mobility mobility,
                     Entity **out_entity = nullptr,
                     float yaw_deg = 0.0f)
    {
        if (!world || !asset.IsValid())
            return false;

        Entity *entity = world->CreateEntity<Entity>(name);
        if (!entity)
            return false;

        auto transform = world->GetTransform(world->CreateTransform(entity->GetEntityID(), mobility));
        transform.SetLocalPosition(glm::vec3(position.x, position.y, position.z));
        // 世界是 Z 轴向上：绕 Z 的 yaw 决定平面朝哪儿（地面 0°；展示屏幕转向相机）
        transform.SetLocalRotation(glm::quat(glm::vec3(0.0f, 0.0f, glm::radians(yaw_deg))));
        transform.SetLocalScale(glm::vec3(scale, scale, 1.0f));
        transform.SetMobility(mobility);

        auto *geometry_data = world->GetOrCreateGeometryData(entity);
        if (!geometry_data)
            return false;

        geometry_data->SetPrimitiveAsset(&asset);

        auto *material_data = world->GetOrCreateMaterialData(entity);
        if (!material_data)
            return false;

        material_data->SetDataResource(material_accessor.GetGlobalSSBOBinding());

        if (out_entity)
            *out_entity = entity;

        return true;
    }

    bool BuildWorldScene(ECSContext *world, const char *tag)
    {
        if (!PlaceEntity(world, "Ground", ground_asset, math::Vector3f(0.0f, 0.0f, 0.0f), 600.0f, Mobility::Static, nullptr, 0.0f))
            return LogStageFail("BuildWorldScene", "ground entity failed");

        return true;
    }

    bool SetupWorldCamera(ECSContext *world, float distance, float yaw, float pitch,
                          const math::Vector3f &target, const char *name)
    {
        if (!world || !world->EnsureCameraSystem())
            return LogStageFail("SetupWorldCamera", "camera system unavailable");

        Entity *camera_entity = world->CreateEntity<Entity>(name);
        auto *camera = world->GetOrCreateCamera(camera_entity);
        if (!camera)
            return LogStageFail("SetupWorldCamera", "GetOrCreateCamera failed");

        camera->control_mode  = CameraControlMode::ViewModel;
        camera->target        = target;
        camera->distance      = distance;
        camera->yaw           = yaw;
        camera->pitch         = pitch;
        camera->is_main_camera= true;
        camera->matrix_dirty  = true;
        return true;
    }

    // ─────────────────────────────────────────────────────────────
    // 初始化
    // ─────────────────────────────────────────────────────────────

    bool CreateOffscreenWorld()
    {
        graph::OffscreenWorldDesc desc;
        desc.width           = 512;
        desc.height          = 512;
        desc.name            = "DualWorldShadows_Child";
        desc.resource_prefix = "DualWorldShadows:ChildRT";
        desc.clear_color     = GetColor4f(COLOR::LightSkyBlue, 1.0f);

        child = graph::OffscreenWorld::Create(GetGraphicsContext(), ecs_context, desc);
        if (!child || !child->IsValid())
            return LogStageFail("CreateOffscreenWorld", "OffscreenWorld::Create failed");

        child_world = child->GetWorld();
        if (!child_world)
            return LogStageFail("CreateOffscreenWorld", "child world is null");

        // 离屏世界**必须**自带影子数据的提交入口：`addr_shadow` 的写点在
        // `ViewUBOCommitSystem`（RenderBufferCommit 相）—— 只注册 RT/Collect/UBO/Camera
        // 的子世界不会提交，影子数据永远是建表零值（离屏世界无阴影）。
        if (!child_world->GetSystem<ViewUBOCommitSystem>())
            if (!child_world->RegisterSystem<ViewUBOCommitSystem>())
                return LogStageFail("CreateOffscreenWorld", "register ViewUBOCommitSystem failed");

        child_world->SetResourceNamePrefix("DualWorldShadows:Child");

        // ── 子世界场景：地面 + 3 个 caster（Movable ⇒ 进 cascade 0 的 caster 集）──
        if (!BuildWorldScene(child_world, "child"))
            return LogStageFail("CreateOffscreenWorld", "child scene failed");

        if (!PlaceEntity(child_world, "ChildCube",     shape_asset[0], math::Vector3f(-1.4f, 0.0f, 1.0f), 1.1f, Mobility::Movable))
            return LogStageFail("CreateOffscreenWorld", "child cube failed");

        if (!PlaceEntity(child_world, "ChildSphere",   shape_asset[1], math::Vector3f( 1.8f, 1.6f, 1.3f), 1.0f, Mobility::Movable))
            return LogStageFail("CreateOffscreenWorld", "child sphere failed");

        if (!PlaceEntity(child_world, "ChildCylinder", shape_asset[2], math::Vector3f( 0.4f, -2.2f, 1.4f), 1.0f, Mobility::Movable))
            return LogStageFail("CreateOffscreenWorld", "child cylinder failed");

        if (!SetupWorldCamera(child_world, 9.0f, 40.0f, -18.0f, math::Vector3f(0.0f, 0.0f, 1.0f), "ChildCamera"))
            return LogStageFail("CreateOffscreenWorld", "child camera failed");

        // ── 子世界的光 + 影子（各写**自己**的世界）──
        if (!ConfigureWorldLight(child_world, kMainLightDirB, 1.6f))
            return LogStageFail("CreateOffscreenWorld", "child light failed");

        if (!EnableWorldCSM(child_world, "CreateOffscreenWorld", 1, 90.0f))
            return LogStageFail("CreateOffscreenWorld", "child CSM failed");

        return true;
    }

    bool BuildMainScene()
    {
        if (!BuildWorldScene(ecs_context, "main"))
            return LogStageFail("BuildMainScene", "main scene failed");

        if (!PlaceEntity(ecs_context, "MainCube",     shape_asset[0], math::Vector3f( 0.0f,  0.0f, 1.2f), 1.3f, Mobility::Movable))
            return LogStageFail("BuildMainScene", "main cube failed");

        if (!PlaceEntity(ecs_context, "MainSphere",   shape_asset[1], math::Vector3f( 3.2f,  2.0f, 1.4f), 1.1f, Mobility::Movable))
            return LogStageFail("BuildMainScene", "main sphere failed");

        if (!PlaceEntity(ecs_context, "MainCylinder", shape_asset[2], math::Vector3f(-3.0f, -1.2f, 1.8f), 1.2f, Mobility::Movable))
            return LogStageFail("BuildMainScene", "main cylinder failed");

        if (!PlaceEntity(ecs_context, "MainTorus",    shape_asset[3], math::Vector3f( 1.2f, -4.0f, 1.3f), 1.2f, Mobility::Movable))
            return LogStageFail("BuildMainScene", "main torus failed");

        // 屏幕：显示离屏世界的颜色纹理（目视对照用；Lit 材质 ⇒ 会被主世界的光照再乘一次）
        // 屏幕：显示离屏世界的画面（面向相机；相机 yaw=45 ⇒ 平面转 -30° 朝向它）。
        // ⚠ **自检模式不建这块屏幕**：主世界采样 B 的纹理是**合法数据流**，留着它
        // 「B 变 ⇒ A 变」就无法区分"合法显示依赖"与"跨世界数据泄漏"，隔离判据 ① 必然假红
        //（实测：留着屏幕时 S0→S1 主世界摘要跟着变，报 FAIL ①）。
        if (selfcheck)
        {
            GLogInfo("[DualWorldShadows][BuildMainScene] 自检模式：不建 ChildScreen"
                     "（避免主世界合法引用 B 的纹理而掩盖真正的泄漏）");
            return true;
        }

        if (!PlaceEntity(ecs_context, "ChildScreen", child_screen_asset,
                         math::Vector3f(5.0f, 1.0f, 4.6f), 4.6f, Mobility::Static, &child_screen_entity, -35.0f))
            return LogStageFail("BuildMainScene", "child screen failed");

        return true;
    }

    bool BindChildScreenTexture()
    {
        // 屏幕实体读子世界 RT 的颜色纹理（引擎的材质纹理引用链负责注册/去重）
        Texture2D *child_color = child ? child->GetColorTexture(0) : nullptr;
        if (!child_color)
            return LogStageFail("BindChildScreenTexture", "child color texture is null");

        if (!child_screen_entity)
            return LogStageFail("BindChildScreenTexture", "ChildScreen entity missing");

        auto *material_data = ecs_context->GetOrCreateMaterialData(child_screen_entity);
        if (!material_data)
            return LogStageFail("BindChildScreenTexture", "screen material data missing");

        material_data->SetTextureResource("base_color", child_color, common_sampler);
        return true;
    }

public:
    bool Init() override
    {
        LogStage("Init", "begin");

        ecs_context = GetECSContext();
        if (!ecs_context)
            return LogStageFail("Init", "ECS context is null");

        ecs_context->SetResourceNamePrefix("DualWorldShadows:MainScene");

        selfcheck = (std::getenv("DWS_SELFCHECK") != nullptr);

        if (!CreateSharedResources())
            return LogStageFail("Init", "CreateSharedResources failed");

        if (!BuildMainScene())
            return LogStageFail("Init", "BuildMainScene failed");

        if (!SetupWorldCamera(ecs_context, 17.0f, 45.0f, -22.0f, math::Vector3f(0.0f, 0.0f, 1.0f), "MainCamera"))
            return LogStageFail("Init", "main camera failed");

        if (!ConfigureWorldLight(ecs_context, kMainLightDirA, 1.5f))
            return LogStageFail("Init", "main light failed");

        // 主世界：1 级联（== 1 条离屏帧槽带）；子世界另占 1 条（RT）+ 1 条（级联）
        // ⇒ 全设备共 4 条带，用 3 条。这是"两个世界各自开 CSM"能成立的关键前提。
        if (!EnableWorldCSM(ecs_context, "Init", 1, 120.0f))
            return LogStageFail("Init", "main CSM failed");

        if (!CreateOffscreenWorld())
            return LogStageFail("Init", "CreateOffscreenWorld failed");

        if (!selfcheck)
            if (!BindChildScreenTexture())
                return LogStageFail("Init", "BindChildScreenTexture failed");

        // 离屏世界先出一帧，保证屏幕贴图第一帧就有内容
        child->Render(0.0f);

        LogStage("Init", "success");
        return true;
    }

    // ─────────────────────────────────────────────────────────────
    // 隔离自检（DWS_SELFCHECK=1）
    // ─────────────────────────────────────────────────────────────

    /// 采样点：钩子帧 1 / 9 / 17 / 25（中间 8 帧 = 参数生效的稳定窗）
    static bool IsSampleFrame(uint32_t f) { return f == 1 || f == 9 || f == 17 || f == 25; }

    void InstallIsolationHook()
    {
        auto *rt = dynamic_cast<graph::SwapchainRenderTarget *>(
            ecs_context ? ecs_context->GetRenderTarget() : nullptr);
        if (!rt)
        {
            std::printf("[DWS] no swapchain RT\n");
            std::exit(2);
        }

        rt->SetInFrameReadbackHook([this, rt]()
        {
            static uint32_t hook_frame = 0;
            ++hook_frame;

            if (!IsSampleFrame(hook_frame))
            {
                InstallIsolationHook();     // 钩子一次性 ⇒ 自续装
                return;
            }

            ulre_example::FrameDigest da;
            ulre_example::FrameDigest db;

            if (!ulre_example::ComputeFrameDigest(rt, da))
            {
                std::printf("[DWS] swapchain readback failed\n");
                std::exit(2);
            }

            if (!child || !ulre_example::ComputeFrameDigest(child->GetRenderTarget(), db))
            {
                std::printf("[DWS] child RT readback failed\n");
                std::exit(2);
            }

            std::printf("[DWS] sample=%u  A(swapchain) %ux%u distinct=%u fnv=0x%016llX | "
                        "B(child) %ux%u distinct=%u fnv=0x%016llX\n",
                        hook_frame,
                        da.width, da.height, da.distinct_byte_values,
                        static_cast<unsigned long long>(da.fnv1a64),
                        db.width, db.height, db.distinct_byte_values,
                        static_cast<unsigned long long>(db.fnv1a64));
            std::fflush(stdout);

            if (da.distinct_byte_values < 2 || db.distinct_byte_values < 2)
            {
                std::printf("[DWS] FAIL: 摘要不空转守卫未过（A distinct=%u / B distinct=%u）\n",
                            da.distinct_byte_values, db.distinct_byte_values);
                std::exit(1);
            }

            if (hook_frame == 1)
            {
                s0 = Sample{da.fnv1a64, db.fnv1a64, da.distinct_byte_values, db.distinct_byte_values};

                // 基线已取 ⇒ 翻转**离屏世界**的主光（只动 B 的世界）
                if (!ConfigureWorldLight(child_world, kMainLightDirB2, 1.6f))
                {
                    std::printf("[DWS] flip child light failed\n");
                    std::exit(2);
                }
                std::printf("[DWS] --- 已翻转【离屏世界】主光（S0 -> S1）\n");
            }
            else if (hook_frame == 9)
            {
                s1 = Sample{da.fnv1a64, db.fnv1a64, da.distinct_byte_values, db.distinct_byte_values};

                if (s1.a != s0.a)
                {
                    fail = true;
                    std::printf("[DWS] FAIL ①：翻转**离屏世界**的光改变了**主世界**的画面"
                                "（A: 0x%016llX -> 0x%016llX）⇒ 光照/阴影数据不是世界私有\n",
                                static_cast<unsigned long long>(s0.a),
                                static_cast<unsigned long long>(s1.a));
                }

                if (s1.b == s0.b)
                {
                    fail = true;
                    std::printf("[DWS] FAIL ②（守卫）：翻转离屏世界的光**没有**改变离屏世界自己的画面"
                                "（B: 0x%016llX 未变）⇒ 本次翻转未生效，隔离判据无意义\n",
                                static_cast<unsigned long long>(s0.b));
                }

                // 翻转**主世界**的主光（只动 A 的世界）
                if (!ConfigureWorldLight(ecs_context, kMainLightDirA2, 1.5f))
                {
                    std::printf("[DWS] flip main light failed\n");
                    std::exit(2);
                }
                std::printf("[DWS] --- 已翻转【主世界】主光（S1 -> S2）\n");
            }
            else if (hook_frame == 17)
            {
                s2 = Sample{da.fnv1a64, db.fnv1a64, da.distinct_byte_values, db.distinct_byte_values};

                if (s2.b != s1.b)
                {
                    fail = true;
                    std::printf("[DWS] FAIL ③：翻转**主世界**的光改变了**离屏世界**的画面"
                                "（B: 0x%016llX -> 0x%016llX）⇒ 光照/阴影数据不是世界私有\n",
                                static_cast<unsigned long long>(s1.b),
                                static_cast<unsigned long long>(s2.b));
                }

                if (s2.a == s1.a)
                {
                    fail = true;
                    std::printf("[DWS] FAIL ④（守卫）：翻转主世界的光**没有**改变主世界自己的画面"
                                "（A: 0x%016llX 未变）⇒ 本次翻转未生效\n",
                                static_cast<unsigned long long>(s1.a));
                }
            }
            else if (hook_frame == 25)
            {
                std::printf("[DWS] ===== 两世界隔离自检 =====\n");
                std::printf("[DWS] A(swapchain): S0=0x%016llX S1=0x%016llX S2=0x%016llX\n",
                            static_cast<unsigned long long>(s0.a),
                            static_cast<unsigned long long>(s1.a),
                            static_cast<unsigned long long>(s2.a));
                std::printf("[DWS] B(child)    : S0=0x%016llX S1=0x%016llX S2=0x%016llX\n",
                            static_cast<unsigned long long>(s0.b),
                            static_cast<unsigned long long>(s1.b),
                            static_cast<unsigned long long>(s2.b));
                std::printf("[DWS] %s\n", fail ? "FAIL" : "PASS");
                std::fflush(stdout);
                std::exit(fail ? 1 : 0);
            }

            InstallIsolationHook();     // 自续装
        });
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        // 同一 app 帧内先出离屏世界的帧（主世界随后：屏幕采样它的纹理 / 自检读回它）
        if (child)
            child->Render(static_cast<float>(delta_time));

        if (!selfcheck)
        {
            // 交互/目视：两个世界的主光各自缓慢旋转（相位不同）⇒ 两套独立阴影同时可见
            static float theta = 0.0f;
            theta += static_cast<float>(delta_time) * 0.35f;

            const float a = theta;
            const float b = theta * 1.0f + 2.1f;      // 相位差

            if (auto light = ecs_context->GetSystem<LightSystem>())
                light->SetMainLightDirection(math::Vector3f(std::cos(a) * 0.8f, std::sin(a) * 0.8f, 0.75f));

            if (child_world)
                if (auto light = child_world->GetSystem<LightSystem>())
                    light->SetMainLightDirection(math::Vector3f(std::cos(b) * 0.8f, std::sin(b) * 0.8f, 0.62f));
        }
        else
        {
            ++frame_index;

            if (frame_index == 8)
                InstallIsolationHook();
        }
    }

public:
    ~DualWorldShadowsApp()
    {
        auto *gc = GetGraphicsContext();
        if (!gc)
            return;

        if (common_sampler)
        {
            if (auto *sm = gc->GetSamplerManager())
                sm->Release(common_sampler);

            common_sampler = nullptr;
        }

        if (auto *gm = gc->GetGeometryManager())
        {
            for (auto *&g : shape_geometry)
            {
                if (g) { gm->Release(g); g = nullptr; }
            }

            if (ground_geometry)       { gm->Release(ground_geometry);       ground_geometry = nullptr; }
            if (child_screen_geometry) { gm->Release(child_screen_geometry); child_screen_geometry = nullptr; }
        }
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<DualWorldShadowsApp>(
        OS_TEXT("Dual World Shadows (two worlds: lights & shadows are world-private)"),
        argc, argv, 1280, 720);
}
