// RenderToTextureColorDepth —— 离屏世界回归床（color + depth 双附件）
//
// 以 RenderToTexture 为基线，多一路深度显示：离屏世界渲染一帧（颜色 + 深度附件），
// 主画面并排两个立方体，左贴 `rt:<RT 名>:color0`，右贴 `rt:<RT 名>:depth`。
// 前端同样只有两句话：材质直接给纹理 URI；离屏世界每帧驱动到 `IsContentReady()`
// 为真即停。示例不碰裸纹理、不做同步预渲染、不管加载时机。
//
// env：
//   RTTCD_SELFCHECK=1   帧摘要自检（就绪后第 8 帧；PASS 退 0 / FAIL 退 1 / 读回失败退 2）
//   RTTCD_DUMP=<tag>    就绪后第 8 帧帧内回读主场景颜色靶落原始字节后退出
//                       （`rttcddump_<tag>_WxH_fmt.raw`）

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetSingle.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/module/OffscreenWorld.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/asset/TextureAsset.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>

#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/color/Color.h>
#include<hgl/log/Log.h>
#include<hgl/mtl/MaterialRecipe.h>

#include<hgl/graph/ssbo/LitMaterialData.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/RenderSystemCore.h>
#include<hgl/ecs/systems/render/LightSystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>

#include<hgl/vk/VKRenderTargetSwapchain.h>
#include "../Shared/FrameDigest.h"   // 帧摘要自检（A6-d 判据重建共用件）

#include <memory>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <numbers>

/**
 * 关于深度可视化（之前 ShadowMap 示例"什么都看不到"的根因）：
 *   引擎深度为逆序 Z（近端=1、远端=0，实测 d≈near/z）：透视下 d 对距离高度非线性，
 *   默认 near=0.1 时全部深度值都落在 0.01 量级，与清屏值 0 分不开，做成贴图是一片黑。
 *   本例把离屏相机的 near/far **收紧到刚好包住物体**（distance ± 球半径附近），
 *   让深度值铺满 [0,1]，深度图才有对比度。
 *
 *   另一个注意点：本例深度立方体走 `UnlitTexture3D`（世界空间无光贴面，直接输出
 *   采样值、不经光照）。采样深度时 `texture()` 只有一个通道有效，引擎在**采样视图**
 *   上把 R 广播到 RGB ⇒ 深度图呈灰度；若走 `Lit` 这类 PBR 材质，深度值会被当成
 *   反照率再过一遍光照，环境高光把整个面染成天空色，看上去和颜色立方体一模一样
 *   ——这正是「两个立方体都在显示 COLOR」的根因。
 *
 *   深度 RT 的空区域是清屏值（远端 0），映射到立方体面上即为黑色，与场景背景重合，
 *   所以深度立方体看起来只有一块「浮着的灰色圆盘」（球面深度）+ 淡淡的棱边，
 *   这是深度数据的正常表现，不是渲染错误。
 */

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

    void LogTextureInfo(const char *tag, Texture2D *tex)
    {
        if (!tex)
        {
            GLogInfo("[RTTColorDepth] %s tex=null", tag);
            return;
        }

        GLogInfo("[RTTColorDepth] %s tex=%p image=%p view=%p layout=%u",
                tag,
                (void *)tex,
                (void *)tex->GetImage(),
                (void *)tex->GetVulkanImageView(),
                (uint32_t)tex->GetImageLayout());
    }

    bool LogStageFail(const char *stage, const char *reason)
    {
        GLogError("[RTTColorDepth][%s] %s", stage, reason);
        return false;
    }

    void LogStage(const char *stage, const char *message)
    {
        GLogInfo("[RTTColorDepth][%s] %s", stage, message);
    }

    // env 开关：Tick 的旋转冻结与摘要/落盘计数共用
    const char *s_dump_tag  = getenv("RTTCD_DUMP");
    const bool  s_selfcheck = (getenv("RTTCD_SELFCHECK") != nullptr);
}

/**
 * 离屏 Pass：一帧同时产出颜色与深度
 *
 * 与 RenderToTexture 的差别只有两点：
 *   1. 对外同时暴露颜色纹理与深度纹理
 *   2. 离屏相机的 near/far 收紧，使深度值在 [0,1] 内有对比度（否则深度贴图是一片白）
 */
class OffscreenPass
{
private:
    std::unique_ptr<graph::OffscreenWorld> offscreen;

    RenderContext *render_context = nullptr;
    graph::EnvProfileID offscreen_env_profile = graph::kEnvProfileDefault;

    Geometry *geometry = nullptr;
    PrimitiveAsset sphere_asset;
    graph::mtl::MaterialRecipe sphere_recipe{};
    using MaterialDataAccessor =
        graph::GlobalSSBODataAccessor;

    MaterialDataAccessor material_data_ssbo_accessor{};
    graph::ssbo::PBRSurfaceRow sphere_material_data{};
    TextureAsset* sphere_base_tex = nullptr;
    TextureAsset* sphere_normal_tex = nullptr;
    TextureAsset* sphere_roughness_tex = nullptr;
    Entity *sphere_entity = nullptr;
    hgl::ecs::GeometryData *sphere_primitive_comp = nullptr;

    /// 离屏相机距目标的距离，决定 near/far 收紧范围
    static constexpr float kCameraDistance = 6.0f;

    /// 球体半径（inline_geometry::CreateSphere 生成的是单位球）
    static constexpr float kSphereRadius = 1.0f;

private:

    bool InitMaterialDataSSBO(ECSContext *world)
    {
        GLogInfo("[RTTColorDepth][OffscreenPass::InitMaterialDataSSBO] begin world=%p", (void *)world);
        if (!world)
            return LogStageFail("OffscreenPass::InitMaterialDataSSBO", "invalid input pointers");

        auto *gc = world->GetGraphicsContext();
        if (!gc && render_context)
            gc = render_context->GetGraphicsContext();

        if (!gc)
            return LogStageFail("OffscreenPass::InitMaterialDataSSBO", "graphics context is null");

        auto *domain_manager = gc->GetGlobalSSBOBufferRegistry();
        if (!domain_manager)
            return LogStageFail("OffscreenPass::InitMaterialDataSSBO", "resource domain manager is null");

        material_data_ssbo_accessor = domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>();
        if (!material_data_ssbo_accessor)
            return LogStageFail("OffscreenPass::InitMaterialDataSSBO", "CreateSSBO failed");

        const graph::ssbo::PBRSurfaceRow material_data = sphere_material_data;
        if (!material_data_ssbo_accessor.Write(material_data))
            return LogStageFail("OffscreenPass::InitMaterialDataSSBO", "write material data failed");

        LogStage("OffscreenPass::InitMaterialDataSSBO", "success");
        return true;
    }

public:
    ~OffscreenPass()
    {
        GraphicsContext *gc = render_context ? render_context->GetGraphicsContext() : nullptr;
        if (!gc)
        {
            if (auto *w = offscreen ? offscreen->GetWorld() : nullptr)
                gc = w->GetGraphicsContext();
        }

        if (gc)
        {
            if (geometry)
            {
                if (auto *gm = gc->GetGeometryManager())
                    gm->Release(geometry);
            }
        }

        geometry = nullptr;
    }

    /// 离屏内容已产出（内部图元资源齐备并画过一帧）
    bool IsContentReady() const
    {
        return offscreen && offscreen->IsContentReady();
    }

    bool IsSceneContentReady() const
    {
        return offscreen && offscreen->IsSceneContentReady();
    }

    IRenderTarget *GetRenderTarget() const
    {
        return offscreen ? offscreen->GetRenderTarget() : nullptr;
    }

    bool Init(WorkObject *owner, const uint32_t width, const uint32_t height)
    {
        GLogInfo("[RTTColorDepth][OffscreenPass::Init] begin owner=%p size=%ux%u",
                 (void *)owner, width, height);

        // 离屏世界：一行描述 + 一次 Create，RT / ECSContext / 渲染系统全部就位
        graph::OffscreenWorldDesc desc;
        desc.width  = width;
        desc.height = height;
        desc.name   = "RTTColorDepth_Offscreen";
        desc.resource_prefix = "RTTColorDepth:OffscreenRT";

        // 清屏色声明在 desc 里，成为 RT 上的权威值。
        // RTT 内容会被主场景 Lit 材质再乘一次光照（kd*NdotL/π ≈ 0.16），
        // 离屏用亮天蓝补偿，避免贴到立方体上整体发黑。
        desc.clear_color = GetColor4f(COLOR::LightSkyBlue, 1.0f);

        offscreen = graph::OffscreenWorld::Create(owner->GetGraphicsContext(),
                                                  owner->GetECSContext(),
                                                  desc);
        if (!offscreen)
            return LogStageFail("OffscreenPass::Init", "OffscreenWorld::Create failed");

        // RTT 内容会被主场景 Lit 材质再乘一次光照，离屏用高太阳强度 profile 补偿
        if (auto *gc = owner->GetGraphicsContext())
        {
            if (auto *env_manager = gc->GetEnvironmentManager())
            {
                graph::EnvironmentInfo info{};

                // T6b：光源参数已迁到**世界私有光源行**（不再跟 profile）—— profile 里只剩天空外观
                // （底色 / 光晕强度；光晕色恒为主光色，故不需要也写入）。
                const graph::SunLightSample sun = graph::EvaluateSunLight(8.0f, 30.0f, 0.0f);

                offscreen_env_profile = env_manager->Create("RTTColorDepth.OffscreenBright", info);
                if (offscreen->GetRenderTarget())
                    offscreen->GetRenderTarget()->SetEnvironmentProfile(offscreen_env_profile);

                // 离屏世界的主光：8:30 的方向/颜色 + 强度 ×4。离屏世界没有 LightSystem
                // ⇒ 直接写它的**光源行**（行表归世界所有，不需要系统在场）。
                if (auto *world = offscreen->GetWorld())
                {
                    if (auto *row = world->GetLightRowTable().Row(graph::kMainLightRowIndex))
                    {
                        graph::LightRow light = *row;

                        light.direction_intensity = math::Vector4f(sun.direction.x, sun.direction.y, sun.direction.z, 4.0f);
                        light.color_angle.x = sun.color.r;
                        light.color_angle.y = sun.color.g;
                        light.color_angle.z = sun.color.b;

                        world->GetLightRowTable().SetRow(graph::kMainLightRowIndex, light);
                    }
                }
            }
        }

        LogTextureInfo("offscreen_rt_color0_init", offscreen->GetColorTexture(0));
        LogTextureInfo("offscreen_rt_depth_init",  offscreen->GetDepthTexture());
        render_context = owner->GetRenderContext();
        GLogInfo("[RTTColorDepth][OffscreenPass::Init] success world=%p rt=%p",
                 (void *)offscreen->GetWorld(), (void *)offscreen->GetRenderTarget());
        return true;
    }

    bool BuildSphere(WorkObject *owner)
    {
        GLogInfo("[RTTColorDepth][OffscreenPass::BuildSphere] begin owner=%p world=%p rt=%p",
                 (void *)owner,
                 (void *)(offscreen ? offscreen->GetWorld() : nullptr),
                 (void *)(offscreen ? offscreen->GetRenderTarget() : nullptr));
        if (!owner || !offscreen || !offscreen->IsValid())
            return LogStageFail("OffscreenPass::BuildSphere", "owner/offscreen world missing");

        GraphicsContext *gc = owner->GetGraphicsContext();
        if (!gc)
            return LogStageFail("OffscreenPass::BuildSphere", "graphics context is null");

        auto *gm = owner->GetManager<GeometryManager>();
        auto *tm = owner->GetManager<TextureManager>();
        auto *device = gc->GetDevice();
        if (!gm || !tm || !device)
            return LogStageFail("OffscreenPass::BuildSphere", "required managers/device missing");

        // 离屏球用 Lit 材质：消费天光/太阳方向，让 RT 纹理内容有光照
        sphere_material_data.base_color = GetColor4f(COLOR::SkyBlue, 1.0f);
        sphere_material_data.metallic = 0.08f;
        sphere_material_data.roughness = 0.92f;
        sphere_material_data.normal_scale = 0.35f;

        sphere_base_tex = tm->CreateTextureAsset(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"), true);
        sphere_normal_tex = tm->CreateTextureAsset(OS_TEXT("res/image/Brickwall/Normal.Tex2D"), true);
        sphere_roughness_tex = tm->CreateTextureAsset(OS_TEXT("res/image/Brickwall/Roughness.Tex2D"), true);
        if (!sphere_base_tex || !sphere_normal_tex || !sphere_roughness_tex)
            return LogStageFail("OffscreenPass::BuildSphere", "load brickwall textures failed");

        if (!InitMaterialDataSSBO(offscreen->GetWorld()))
            return LogStageFail("OffscreenPass::BuildSphere", "InitMaterialDataSSBO failed");

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateStandardGeometryVertexFormat());
        geometry = inline_geometry::CreateSphere(pc.get(), 64);
        if (!geometry)
            return LogStageFail("OffscreenPass::BuildSphere", "CreateSphere geometry failed");

        gm->Add(geometry);

        const mtl::MaterialRecipe *loaded_recipe =
            owner->GetRecipe("RTTColorDepth.OffscreenSphere");
        if (!loaded_recipe)
            return LogStageFail("OffscreenPass::BuildSphere", "material recipe not found");
        sphere_recipe = *loaded_recipe;
        if (!(sphere_recipe.material_ssbo_binding = material_data_ssbo_accessor.GetGlobalSSBOBinding()).IsValid())
            return LogStageFail("OffscreenPass::BuildSphere", "register material SSBO binding failed");

        // 纹理是材质原型固有属性 ⇒ 写进配方（实体端零纹理调用）。
        // 采样器由 Bindless 预设统一管理，材质侧不再持有 Sampler。
        if (!sphere_recipe.AddTexture("base_color", sphere_base_tex, true)
         || !sphere_recipe.AddTexture("normal", sphere_normal_tex, true)
         || !sphere_recipe.AddTexture("roughness", sphere_roughness_tex, true))
            return LogStageFail("OffscreenPass::BuildSphere", "bind recipe textures failed");

        sphere_asset = PrimitiveAsset(geometry, &sphere_recipe, PrimitiveType::Triangles);
        if (!sphere_asset.IsValid())
            return LogStageFail("OffscreenPass::BuildSphere", "create offscreen primitive asset failed");

        auto *world = offscreen->GetWorld();
        sphere_entity = world->CreateEntity<Entity>("OffscreenSphere");
        auto transform = world->GetTransform(world->CreateTransform(sphere_entity->GetEntityID(), Mobility::Static));
        auto prim_comp = sphere_entity->GetContext()->GetOrCreateGeometryData(sphere_entity);
        hgl::ecs::MaterialData *material_data_comp = sphere_entity->GetContext()->GetOrCreateMaterialData(sphere_entity);

        transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform.SetLocalScale(glm::vec3(kSphereRadius, kSphereRadius, kSphereRadius));
        transform.SetMobility(Mobility::Static);

        prim_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(prim_comp->GetOwnerID())->SetPrimitiveAsset(&sphere_asset);
        // 纹理已由配方声明（sphere_recipe.AddTexture），实体端不再重复绑定
        hgl::ecs::MaterialData::MaterialDataAuthoringResource sphere_struct{};
        sphere_struct = material_data_ssbo_accessor.GetGlobalSSBOBinding();
        material_data_comp->SetDataResource(sphere_struct);
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

        sphere_primitive_comp = prim_comp;

        Entity *camera_entity = world->CreateEntity<Entity>("OffscreenCamera");
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = world->GetOrCreateCamera(camera_entity);
        camera->control_mode = CameraControlMode::ViewModel;
        camera->target = math::Vector3f(0, 0, 0);
        camera->distance = kCameraDistance;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        // 关键：把 near/far 收紧到刚好包住球体（距相机 kCameraDistance ± kSphereRadius，
        // 再留一点余量）。引擎深度是「近端=1、远端=0」的逆序 Z（实测 d≈near/z），
        // 默认的 near=0.1 会让全部深度值落到 0.01 量级，与清屏值 0（无限远）几乎
        // 分不开，深度贴图就是一片黑——这正是之前 ShadowMap 示例看不到东西的原因。
        camera->near_plane = kCameraDistance - kSphereRadius - 0.5f;
        camera->far_plane  = kCameraDistance + kSphereRadius + 0.5f;

        GLogInfo("[RTTColorDepth][OffscreenPass::BuildSphere] tightened depth range: near=%.2f far=%.2f (camera distance=%.2f)",
                 camera->near_plane, camera->far_plane, camera->distance);

        auto camera_system = offscreen->GetCameraSystem();
        camera->viewport_info = camera_system ? camera_system->GetViewportInfo() : nullptr;

        LogStage("OffscreenPass::BuildSphere", "success");
        return true;
    }

    bool RenderOffscreenFrame()
    {
        // 清屏色已声明在 OffscreenWorldDesc::clear_color（见 Init），此处无需再传。
        // 内部走 ECSContext::RenderTo()，与主窗口路径共用同一套帧驱动。
        if (!offscreen)
            return LogStageFail("OffscreenPass::RenderOffscreenFrame", "offscreen world is null");

        offscreen->Render();
        return true;
    }
};

/**
 * 主画面里的显示立方体：一个立方体 + 其材质/纹理接线
 */
struct DisplayCube
{
    const char *name = nullptr;
    const char *recipe_name = nullptr;  ///< 两个立方体用不同配方名，避免材质缓存互相覆盖
    float x_offset = 0.0f;              ///< 世界坐标 X 偏移，用于并排摆放颜色与深度两个立方体
    const char *tag = nullptr;          ///< 日志标签
    const char *uri = nullptr;          ///< 纹理 URI（`rt:<RT 名>:<附件名>`）

    Geometry *geometry = nullptr;

    PrimitiveAsset asset;
    graph::mtl::MaterialRecipe recipe{};

    using MaterialDataAccessor = graph::GlobalSSBODataAccessor;
    MaterialDataAccessor accessor{};
    graph::ssbo::PBRSurfaceRow material_data{};

    /// 材质授权统一走句柄：离屏颜色/深度附件的 TextureAsset（RT 创建时已绑定）。
    /// 示例不再持有裸纹理 —— 就绪判定（含离屏内容是否已画出）由 ECS 侧统一负责。
    TextureAsset *texture_asset = nullptr;

    Entity *entity = nullptr;
    hgl::ecs::TransformAccessor transform;
};

class RenderToTextureColorDepthApp final: public WorkObject
{
private:
    OffscreenPass *offscreen = nullptr;

    ECSContext *ecs_context = nullptr;
    Entity *camera_entity = nullptr;

    /// [0] = 贴离屏颜色纹理，[1] = 贴离屏深度纹理
    DisplayCube cubes[2];

    float cube_theta = 0.0f;

    /// 两个立方体的 X 偏移
    static constexpr float kCubeSpacing = 1.4f;

private:
    bool SetupMainCamera()
    {
        LogStage("App::SetupMainCamera", "begin");
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return LogStageFail("App::SetupMainCamera", "ECS context/camera system unavailable");

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target = math::Vector3f(0, 0, 0);
        camera->distance = 6.5f;        // 比 RenderToTexture 稍远，让两个立方体都进画面
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->viewport_info = GetViewportInfo();
        LogStage("App::SetupMainCamera", "success");
        return true;
    }

    bool InitMaterialDataSSBO(DisplayCube &cube)
    {
        auto *domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return LogStageFail("App::InitMaterialDataSSBO", "resource domain manager is null");

        cube.accessor = domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>();
        if (!cube.accessor)
            return LogStageFail("App::InitMaterialDataSSBO", "CreateSSBO failed");

        if (!cube.accessor.Write(cube.material_data))
            return LogStageFail("App::InitMaterialDataSSBO", "write material data failed");

        return true;
    }

    bool CreateOffscreenRT()
    {
        LogStage("App::CreateOffscreenRT", "begin");
        offscreen = new OffscreenPass();
        if (!offscreen)
            return LogStageFail("App::CreateOffscreenRT", "new OffscreenPass failed");

        if (!offscreen->Init(this, 512, 512))
            return LogStageFail("App::CreateOffscreenRT", "OffscreenPass::Init failed");

        if (!offscreen->BuildSphere(this))
            return LogStageFail("App::CreateOffscreenRT", "OffscreenPass::BuildSphere failed");

        // 这里**不再**同步渲染一次：离屏世界的球体贴图是异步加载的，此刻任何一次
        // 绘制都只会得到清屏色，而且会把「已产出内容」这个状态永久钉死。
        // 由 Tick() 逐帧驱动离屏世界，待其内部资源齐备、内容真正画出来后自动停止。

        LogStage("App::CreateOffscreenRT", "success");
        return true;
    }

    /// 创建一个显示立方体：贴给定的离屏纹理
    bool CreateDisplayCube(DisplayCube &cube)
    {
        GLogInfo("[RTTColorDepth][App::CreateDisplayCube] begin name=%s x=%.2f",
                 cube.name, cube.x_offset);

        auto *gc = GetGraphicsContext();
        if (!gc)
            return LogStageFail("App::CreateDisplayCube", "graphics context is null");

        auto *gm = GetManager<GeometryManager>();
        auto *device = gc->GetDevice();
        if (!gm || !device)
            return LogStageFail("App::CreateDisplayCube", "required managers/device missing");

        if (!cube.texture_asset)
            return LogStageFail("App::CreateDisplayCube", "offscreen texture asset is null");

        // base_color 乘数保持白色：纹理内容原样进入光照
        cube.material_data.base_color = Color4f(1.0f);
        cube.material_data.metallic = 0.08f;
        cube.material_data.roughness = 0.92f;
        cube.material_data.normal_scale = 0.35f;

        if (!InitMaterialDataSSBO(cube))
            return LogStageFail("App::CreateDisplayCube", "InitMaterialDataSSBO failed");

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateStandardGeometryVertexFormat());
        inline_geometry::CubeCreateInfo cci{};
        cci.tex_coord = true;
        cci.ntb = NTBType::Normal;

        cube.geometry = inline_geometry::CreateCube(pc.get(), &cci);
        if (!cube.geometry)
            return LogStageFail("App::CreateDisplayCube", "CreateCube geometry failed");

        gm->Add(cube.geometry);

        const mtl::MaterialRecipe *loaded_recipe = GetRecipe(cube.recipe_name);
        if (!loaded_recipe)
            return LogStageFail("App::CreateDisplayCube", "material recipe not found");
        cube.recipe = *loaded_recipe;
        if (!(cube.recipe.material_ssbo_binding = cube.accessor.GetGlobalSSBOBinding()).IsValid())
            return LogStageFail("App::CreateDisplayCube", "register material SSBO binding failed");

        // 唯一区别：配方绑定的离屏纹理不同（颜色 / 深度）——`rtex:` 身份，`CanRender` 负责等内容就绪。
        // 采样器由 Bindless 预设统一管理，材质侧不再持有 Sampler。
        if (!cube.recipe.AddTexture("base_color", cube.texture_asset, true))
            return LogStageFail("App::CreateDisplayCube", "bind recipe texture failed");

        cube.asset = PrimitiveAsset(cube.geometry, &cube.recipe, PrimitiveType::Triangles);
        if (!cube.asset.IsValid())
            return LogStageFail("App::CreateDisplayCube", "create cube primitive asset failed");

        cube.entity = ecs_context->CreateEntity<Entity>(cube.name);
        cube.transform = ecs_context->GetTransform(ecs_context->CreateTransform(cube.entity->GetEntityID(), Mobility::Static));
        auto prim_comp = cube.entity->GetContext()->GetOrCreateGeometryData(cube.entity);
        hgl::ecs::MaterialData *material_data_comp = cube.entity->GetContext()->GetOrCreateMaterialData(cube.entity);

        cube.transform.SetLocalPosition(glm::vec3(cube.x_offset, 0.0f, 0.0f));
        cube.transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        cube.transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        cube.transform.SetMobility(Mobility::Movable);

        prim_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(prim_comp->GetOwnerID())->SetPrimitiveAsset(&cube.asset);
        // 纹理已由配方声明（cube.recipe.AddTexture），实体端不再重复绑定
        hgl::ecs::MaterialData::MaterialDataAuthoringResource cube_struct{};
        cube_struct = cube.accessor.GetGlobalSSBOBinding();
        material_data_comp->SetDataResource(cube_struct);
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

        GLogInfo("[RTTColorDepth] %s uri=%s", cube.tag, cube.uri ? cube.uri : "(null)");
        LogStage("App::CreateDisplayCube", "success");
        return true;
    }

    bool CreateDisplayCubes()
    {
        LogStage("App::CreateDisplayCubes", "begin");

        if (!offscreen)
            return LogStageFail("App::CreateDisplayCubes", "offscreen pass missing");

        cubes[0].name        = "Color";
        cubes[0].recipe_name = "RTTColorDepth.DisplayColor";
        cubes[0].tag         = "onscreen_bind_color";
        cubes[0].uri         = "rt:RTTColorDepth:OffscreenRT:color0";
        cubes[0].x_offset    = -kCubeSpacing;

        cubes[1].name        = "Depth";
        cubes[1].recipe_name = "RTTColorDepth.DisplayDepth";
        // 两个立方体都必须是「原样显示」的无光领域：
        //   · 走 Lit（PBR）会把附件值当反照率再过一遍光照 —— 颜色立方体被压暗
        //    （实测 Rmax 0.66 vs 附件 0.98），深度立方体则被环境高光染成天空色，
        //     看上去和颜色立方体几乎一样（这就是「两个都像在显示 COLOR」的根因）。
        //   · 走 UnlitTexture（2D NDC）顶点通路错，物体位置/尺寸全错。
        // 深度立方体另有一层：深度采样视图已把 R 广播到 RGB，故呈灰度。
        cubes[1].tag         = "onscreen_bind_depth";
        cubes[1].uri         = "rt:RTTColorDepth:OffscreenRT:depth";
        cubes[1].x_offset    = +kCubeSpacing;

        auto *texture_manager = GetManager<TextureManager>();
        if (!texture_manager)
            return LogStageFail("App::CreateDisplayCubes", "texture manager unavailable");

        // 只用 URI 声明"要哪个附件"，不拿裸纹理、不再 WrapTextureAsset：
        // 句柄在 RT 创建时已由 RenderTargetManager 绑定，此刻「容器就绪、内容未就绪」，
        // ECS 的 CanRender 会拦住两个立方体，直到离屏 pass 真正画出内容。
        cubes[0].texture_asset = texture_manager->CreateTextureAsset(OS_TEXT("rt:RTTColorDepth:OffscreenRT:color0"));
        cubes[1].texture_asset = texture_manager->CreateTextureAsset(OS_TEXT("rt:RTTColorDepth:OffscreenRT:depth"));

        if (!cubes[0].texture_asset)
            return LogStageFail("App::CreateDisplayCubes", "offscreen color texture asset unavailable");

        if (!cubes[1].texture_asset)
            return LogStageFail("App::CreateDisplayCubes", "offscreen depth texture asset unavailable");

        for (DisplayCube &cube : cubes)
        {
            if (!CreateDisplayCube(cube))
                return false;
        }

        LogStage("App::CreateDisplayCubes", "success");
        return true;
    }

public:
    ~RenderToTextureColorDepthApp() override
    {
        SAFE_CLEAR(offscreen)

        for (DisplayCube &cube : cubes)
            cube.texture_asset = nullptr;
    }

    bool Init() override
    {
        LogStage("App::Init", "begin");
        ecs_context = GetECSContext();
        if (!ecs_context)
            return LogStageFail("App::Init", "ECS context is null");

        ecs_context->SetResourceNamePrefix("RTTColorDepth:MainScene");

        auto light_system = ecs_context->GetSystem<LightSystem>();
        if (!light_system)
            light_system = ecs_context->RegisterSystem<LightSystem>();

        if (light_system)
        {
            // 默认 10:00 太阳仰角 60°，本例相机平视立方体侧面（与太阳点积≈0），
            // 只剩天顶环境光会显得整体偏暗。改 8:30（仰角 37.5°）让侧面吃到直射光。
            light_system->SetMainLightTime(8, 30, 0);
        }

        if (!CreateOffscreenRT())
            return LogStageFail("App::Init", "CreateOffscreenRT failed");

        if (!CreateDisplayCubes())
            return LogStageFail("App::Init", "CreateDisplayCubes failed");

        if (!SetupMainCamera())
            return LogStageFail("App::Init", "SetupMainCamera failed");

        LogStage("App::Init", "success");
        return true;
    }

    void Tick(double delta_time) override
    {
        // RTTCD_SELFCHECK=1：**帧摘要自检**（§6.5；共用件 example/Shared/FrameDigest.h）——
        // 就绪后第 8 帧帧内回读主场景颜色靶 ⇒ {不同字节取值数, FNV-1a 64} 比对下方内联常量：
        // PASS 退出 0 / FAIL 退出 1 / 读回失败退出 2。
        // 判据意义 = **离屏世界回归床**（color+depth 双输出版）：主场景两个立方体分别贴
        // `rt:...:color0` 与 `rt:...:depth`；离屏世界若再"永不绘制"，贴图退化为纯清屏色/纯白
        // ⇒ 摘要立变。摘要必须冻结（自旋停走 + 定帧），否则两次运行几何不同 ⇒ 假红。

        // 逻辑更新写在 Tick（渲染前）——TransformSystem 在渲染帧内提交变换，
        // Tick 里改与本回调内改同帧等价，且不占用命令缓冲录制时间
        if (!s_selfcheck)
        {
            cube_theta += static_cast<float>(delta_time) * 0.8f;
            cube_theta = fmodf(cube_theta, 2.0f * std::numbers::pi_v<float>);

            const glm::quat rot_z = glm::angleAxis(cube_theta, glm::vec3(0.0f, 0.0f, 1.0f));
            const glm::quat rot_x = glm::angleAxis(cube_theta * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));

            for (DisplayCube &cube : cubes)
            {
                if (cube.transform.IsValid())
                    cube.transform.SetLocalRotation(rot_z * rot_x);
            }
        }

        WorkObject::Tick(delta_time);

        // 离屏内容静态：驱动到内容就绪即可，之后停止（避免同一纹理跨 pass 每帧写/读抖动）。
        // 就绪前每帧都要驱动 —— 离屏世界内部的异步读盘由它自己的 TextureStreamingSystem
        // 推进，不调 Render() 就没有帧，资源也就永远不会到齐。
        //
        // 帧预算**不能固定**：异步读盘完成在第几帧取决于磁盘与解码速度（实测 2~10 帧浮动），
        // 写死帧数会让"贴图刚好没赶上"伪装成"离屏永不渲染"。判据就是内容就绪本身。
        if (offscreen && !offscreen->IsContentReady())
        {
            offscreen->RenderOffscreenFrame();
        }

        // 自检与落盘都只从「离屏内容已就绪」之后开始计数 —— 否则第 8 帧可能仍落在
        // 异步读盘尚未完成的窗口里，摘要就会把"还没加载完"当成"离屏没画"。
        if (offscreen && !offscreen->IsContentReady())
            return;

        {
            static int s_dump_frame = s_dump_tag ? 0 : -1;
            static uint32_t s_self_frame = 0;

            static constexpr uint32_t kSelfW           = 1280;
            static constexpr uint32_t kSelfH           = 720;
            static constexpr uint32_t kSelfMinDistinct = 50;  // 不空转守卫：实测 distinct=256；纯清屏帧只有个位数
            static constexpr uint64_t kSelfFNV         = 0xCF4D2D34F619C18Dull;  // 冻结自旋 + 就绪后第 8 帧实测（三次连跑一致）

            if (s_dump_tag && ++s_dump_frame == 8)
            {
                auto *rt = dynamic_cast<graph::SwapchainRenderTarget *>(
                    ecs_context ? ecs_context->GetRenderTarget() : nullptr);
                if (!rt)
                {
                    printf("[RTTCDDUMP] no swapchain RT\n");
                    std::exit(2);
                }

                const char *tag = s_dump_tag;
                graph::IRenderTarget *rt_ptr = rt;
                rt->SetInFrameReadbackHook([tag, rt_ptr]()
                {
                    std::vector<uint8_t> pixels;
                    graph::TextureReadbackInfo info;
                    if (!graph::ReadbackColorTarget(rt_ptr, pixels, 0, &info) || pixels.empty())
                    {
                        printf("[RTTCDDUMP] readback failed\n");
                        std::exit(2);
                    }

                    char name[256];
                    snprintf(name, sizeof(name), "rttcddump_%s_%ux%u_%s.raw",
                             tag, info.width, info.height,
                             GetVulkanFormat(info.format) ? GetVulkanFormat(info.format)->name : "unknown");

                    FILE *fp = fopen(name, "wb");
                    if (!fp)
                    {
                        printf("[RTTCDDUMP] open %s failed\n", name);
                        std::exit(2);
                    }

                    fwrite(pixels.data(), 1, pixels.size(), fp);
                    fclose(fp);
                    printf("[RTTCDDUMP] wrote %s (%zu bytes)\n", name, pixels.size());
                    std::exit(0);
                });
            }

            if (s_selfcheck && ++s_self_frame == 8)
            {
                auto *rt = dynamic_cast<graph::SwapchainRenderTarget *>(
                    ecs_context ? ecs_context->GetRenderTarget() : nullptr);
                if (!rt)
                {
                    printf("[RTTCDSELF] no swapchain RT\n");
                    std::exit(2);
                }

                ulre_example::FrameDigest expect;
                expect.width                = kSelfW;
                expect.height               = kSelfH;
                expect.distinct_byte_values = kSelfMinDistinct;
                expect.fnv1a64              = kSelfFNV;

                ulre_example::InstallFrameDigestHook(rt, expect, "RTTCDSELF");
            }
        }
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<RenderToTextureColorDepthApp>(OS_TEXT("Render To Texture - Color vs Depth"), argc, argv, 1280, 720);
}
