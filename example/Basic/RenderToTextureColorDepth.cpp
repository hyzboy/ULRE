#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetSingle.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/module/OffscreenWorld.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/SamplerManager.h>
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
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/RenderSystemCore.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>

#include <memory>
#include <cstring>
#include <numbers>

/**
 * RenderToTextureColorDepth —— 离屏渲染的 color 与 depth 并排显示
 *
 * 以 RenderToTexture 为基线（该示例已验证可正常显示），在其之上加一路深度显示：
 * 离屏世界渲染一帧（颜色附件 + 深度附件），主画面并排放两个立方体，
 * 左边贴离屏的 **颜色** 纹理，右边贴离屏的 **深度** 纹理。
 * 这样一眼即可确认「离屏渲染是否正常」以及「深度是否真的有内容」。
 *
 * 关于深度可视化（之前 ShadowMap 示例"什么都看不到"的根因）：
 *   透视投影下深度值是非线性的，物体若落在 near~far 的远端附近，全部深度值会挤在
 *   0.95~1.0，做成贴图后是一片近乎均匀的白，肉眼分辨不出任何形状。
 *   本例把离屏相机的 near/far **收紧到刚好包住物体**（distance ± 球半径附近），
 *   让深度值铺满 [0,1]，深度图才有对比度。
 *
 *   另一个注意点：采样深度纹理时 GLSL texture() 返回 vec4(depth, 0, 0, 1)——
 *   只有 R 通道有效，所以深度贴图呈红色调，这是正常现象而非渲染错误。
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
    Sampler *sphere_sampler = nullptr;
    Texture2D *sphere_base_tex = nullptr;
    Texture2D *sphere_normal_tex = nullptr;
    Texture2D *sphere_roughness_tex = nullptr;
    Entity *sphere_entity = nullptr;
    std::shared_ptr<PrimitiveComponent> sphere_primitive_comp;

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

            if (sphere_sampler)
            {
                if (auto *sm = gc->GetSamplerManager())
                    sm->Release(sphere_sampler);
                sphere_sampler = nullptr;
            }
        }

        geometry = nullptr;
    }

    Texture2D *GetColorTexture() const
    {
        return offscreen ? offscreen->GetColorTexture(0) : nullptr;
    }

    Texture2D *GetDepthTexture() const
    {
        return offscreen ? offscreen->GetDepthTexture() : nullptr;
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
                info.sky.SetTime(8, 30, 0);
                info.sky.sun_intensity = 4.0f;

                offscreen_env_profile = env_manager->Create("RTTColorDepth.OffscreenBright", info);
                if (offscreen->GetRenderTarget())
                    offscreen->GetRenderTarget()->SetEnvironmentProfile(offscreen_env_profile);
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
        auto *sm = owner->GetManager<SamplerManager>();
        auto *tm = owner->GetManager<TextureManager>();
        auto *device = gc->GetDevice();
        if (!gm || !sm || !tm || !device)
            return LogStageFail("OffscreenPass::BuildSphere", "required managers/device missing");

        // 离屏球用 Lit 材质：消费天光/太阳方向，让 RT 纹理内容有光照
        sphere_material_data.base_color = GetColor4f(COLOR::SkyBlue, 1.0f);
        sphere_material_data.metallic = 0.08f;
        sphere_material_data.roughness = 0.92f;
        sphere_material_data.normal_scale = 0.35f;

        sphere_sampler = sm->CreateSampler();
        if (!sphere_sampler)
            return LogStageFail("OffscreenPass::BuildSphere", "CreateSampler failed");

        sphere_base_tex = tm->LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"), true);
        sphere_normal_tex = tm->LoadTexture2D(OS_TEXT("res/image/Brickwall/Normal.Tex2D"), true);
        sphere_roughness_tex = tm->LoadTexture2D(OS_TEXT("res/image/Brickwall/Roughness.Tex2D"), true);
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

        sphere_recipe.recipe_name = "RTTColorDepth.OffscreenSphere";
        sphere_recipe.mtl_def_id = "Lit";
        sphere_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        if (!(sphere_recipe.material_ssbo_binding = material_data_ssbo_accessor.GetGlobalSSBOBinding()).IsValid())
            return LogStageFail("OffscreenPass::BuildSphere", "register material SSBO binding failed");

        sphere_asset = PrimitiveAsset(geometry, &sphere_recipe, PrimitiveType::Triangles);
        if (!sphere_asset.IsValid())
            return LogStageFail("OffscreenPass::BuildSphere", "create offscreen primitive asset failed");

        auto *world = offscreen->GetWorld();
        sphere_entity = world->CreateEntity<Entity>("OffscreenSphere");
        auto transform = sphere_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto prim_comp = sphere_entity->AddComponent<PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(kSphereRadius, kSphereRadius, kSphereRadius));
        transform->SetMovable(false);

        prim_comp->SetPrimitiveAsset(&sphere_asset);
        prim_comp->SetMaterialTextureResource("base_color", sphere_base_tex, sphere_sampler);
        prim_comp->SetMaterialTextureResource("normal", sphere_normal_tex, sphere_sampler);
        prim_comp->SetMaterialTextureResource("roughness", sphere_roughness_tex, sphere_sampler);
        hgl::ecs::PrimitiveComponent::MaterialDataAuthoringResource sphere_struct{};
        sphere_struct = material_data_ssbo_accessor.GetGlobalSSBOBinding();
        prim_comp->SetMaterialDataResource(sphere_struct);
        prim_comp->SetVisible(true);

        sphere_primitive_comp = prim_comp;

        Entity *camera_entity = world->CreateEntity<Entity>("OffscreenCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();
        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0, 0, 0);
        camera->distance = kCameraDistance;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        // 关键：把 near/far 收紧到刚好包住球体（距相机 kCameraDistance ± kSphereRadius，
        // 再留一点余量）。默认的 0.1 / 1000 会把全部深度值压到 0.99 附近，
        // 深度贴图将是一片无法分辨的白——这正是之前 ShadowMap 示例看不到东西的原因。
        camera->near_plane = kCameraDistance - kSphereRadius - 0.5f;
        camera->far_plane  = kCameraDistance + kSphereRadius + 0.5f;

        GLogInfo("[RTTColorDepth][OffscreenPass::BuildSphere] tightened depth range: near=%.2f far=%.2f (camera distance=%.2f)",
                 camera->near_plane, camera->far_plane, camera->distance);

        auto camera_system = offscreen->GetCameraSystem();
        camera->camera_data = camera_system ? camera_system->GetCamera() : nullptr;
        camera->camera_info = const_cast<CameraInfo *>(camera_system ? camera_system->GetCameraInfo() : nullptr);
        camera->viewport_info = camera_system ? camera_system->GetViewportInfo() : nullptr;

        LogStage("OffscreenPass::BuildSphere", "success");
        return true;
    }

    bool RenderOnce()
    {
        LogStage("OffscreenPass::RenderOnce", "begin");

        // 清屏色已声明在 OffscreenWorldDesc::clear_color（见 Init），此处无需再传。
        // 内部走 ECSContext::RenderTo()，与主窗口路径共用同一套帧驱动。
        if (!offscreen)
            return LogStageFail("OffscreenPass::RenderOnce", "offscreen world is null");

        offscreen->Render();

        LogTextureInfo("offscreen_rt_color0_after_render", offscreen->GetColorTexture(0));
        LogTextureInfo("offscreen_rt_depth_after_render",  offscreen->GetDepthTexture());

        LogStage("OffscreenPass::RenderOnce", "success");
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

    Geometry *geometry = nullptr;

    PrimitiveAsset asset;
    graph::mtl::MaterialRecipe recipe{};

    using MaterialDataAccessor = graph::GlobalSSBODataAccessor;
    MaterialDataAccessor accessor{};
    graph::ssbo::PBRSurfaceRow material_data{};

    Sampler *sampler = nullptr;
    Texture2D *texture = nullptr;

    Entity *entity = nullptr;
    std::shared_ptr<TransformComponent> transform;
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
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0, 0, 0);
        camera->distance = 6.5f;        // 比 RenderToTexture 稍远，让两个立方体都进画面
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<CameraInfo *>(GetCameraInfo());
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

        // 本例离屏内容静态，启动时渲染一次，避免同一纹理跨 pass 的写读抖动
        if (!offscreen->RenderOnce())
            return LogStageFail("App::CreateOffscreenRT", "OffscreenPass::RenderOnce failed");

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

        auto *sm = GetManager<SamplerManager>();
        auto *gm = GetManager<GeometryManager>();
        auto *device = gc->GetDevice();
        if (!sm || !gm || !device)
            return LogStageFail("App::CreateDisplayCube", "required managers/device missing");

        if (!cube.texture)
            return LogStageFail("App::CreateDisplayCube", "offscreen texture is null");

        cube.sampler = sm->CreateSampler();
        if (!cube.sampler)
            return LogStageFail("App::CreateDisplayCube", "CreateSampler failed");

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

        cube.recipe.recipe_name = cube.recipe_name;
        cube.recipe.mtl_def_id = "Lit";
        cube.recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        if (!(cube.recipe.material_ssbo_binding = cube.accessor.GetGlobalSSBOBinding()).IsValid())
            return LogStageFail("App::CreateDisplayCube", "register material SSBO binding failed");

        cube.asset = PrimitiveAsset(cube.geometry, &cube.recipe, PrimitiveType::Triangles);
        if (!cube.asset.IsValid())
            return LogStageFail("App::CreateDisplayCube", "create cube primitive asset failed");

        cube.entity = ecs_context->CreateEntity<Entity>(cube.name);
        cube.transform = cube.entity->AddComponent<TransformComponent>(Mobility::Static);
        auto prim_comp = cube.entity->AddComponent<PrimitiveComponent>();

        cube.transform->SetLocalPosition(glm::vec3(cube.x_offset, 0.0f, 0.0f));
        cube.transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        cube.transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        cube.transform->SetMovable(true);

        prim_comp->SetPrimitiveAsset(&cube.asset);
        // 唯一区别：绑定的离屏纹理不同（颜色 / 深度）
        prim_comp->SetMaterialTextureResource("base_color", cube.texture, cube.sampler);
        hgl::ecs::PrimitiveComponent::MaterialDataAuthoringResource cube_struct{};
        cube_struct = cube.accessor.GetGlobalSSBOBinding();
        prim_comp->SetMaterialDataResource(cube_struct);
        prim_comp->SetVisible(true);

        LogTextureInfo(cube.tag, cube.texture);
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
        cubes[0].x_offset    = -kCubeSpacing;
        cubes[0].texture     = offscreen->GetColorTexture();

        cubes[1].name        = "Depth";
        cubes[1].recipe_name = "RTTColorDepth.DisplayDepth";
        cubes[1].tag         = "onscreen_bind_depth";
        cubes[1].x_offset    = +kCubeSpacing;
        cubes[1].texture     = offscreen->GetDepthTexture();

        if (!cubes[0].texture)
            return LogStageFail("App::CreateDisplayCubes", "offscreen color texture unavailable");

        if (!cubes[1].texture)
            return LogStageFail("App::CreateDisplayCubes", "offscreen depth texture unavailable");

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

        if (auto *gc = GetGraphicsContext())
        {
            if (auto *sm = gc->GetManager<SamplerManager>())
            {
                for (DisplayCube &cube : cubes)
                {
                    if (cube.sampler)
                    {
                        sm->Release(cube.sampler);
                        cube.sampler = nullptr;
                    }
                }
            }
        }

        for (DisplayCube &cube : cubes)
            cube.texture = nullptr;
    }

    bool Init() override
    {
        LogStage("App::Init", "begin");
        ecs_context = GetECSContext();
        if (!ecs_context)
            return LogStageFail("App::Init", "ECS context is null");

        ecs_context->SetResourceNamePrefix("RTTColorDepth:MainScene");

        auto environment_system = ecs_context->GetSystem<EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterRenderSystem<EnvironmentSystem>();

        if (environment_system)
        {
            // 默认 10:00 太阳仰角 60°，本例相机平视立方体侧面（与太阳点积≈0），
            // 只剩天顶环境光会显得整体偏暗。改 8:30（仰角 37.5°）让侧面吃到直射光。
            if (auto *sky = environment_system->EditSkyInfo())
            {
                sky->SetTime(8, 30, 0);
                environment_system->MarkSkyDirty();
            }
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
        // 逻辑更新写在 Tick（渲染前）——TransformSystem 在渲染帧内提交变换，
        // Tick 里改与本回调内改同帧等价，且不占用命令缓冲录制时间
        cube_theta += static_cast<float>(delta_time) * 0.8f;
        cube_theta = fmodf(cube_theta, 2.0f * std::numbers::pi_v<float>);

        const glm::quat rot_z = glm::angleAxis(cube_theta, glm::vec3(0.0f, 0.0f, 1.0f));
        const glm::quat rot_x = glm::angleAxis(cube_theta * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));

        for (DisplayCube &cube : cubes)
        {
            if (cube.transform)
                cube.transform->SetLocalRotation(rot_z * rot_x);
        }

        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<RenderToTextureColorDepthApp>(OS_TEXT("Render To Texture - Color vs Depth (ECS)"), argc, argv, 1280, 720);
}
