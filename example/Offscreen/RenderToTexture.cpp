// RenderToTexture —— 离屏世界回归床（离屏球画进 `rt:` 附件，主场景立方体再采样它）
//
// 前端只有两句话：
//   · 立方体材质 `base_color` 直接给 `rt:<RT 名>:color0`（纹理 URI），示例不碰裸纹理、
//     不做同步预渲染、不管加载时机；
//   · 离屏世界每帧驱动到 `IsContentReady()` 为真即停（内容静态），之后只主世界在画。
//
// env：
//   RTT_SELFCHECK=1     帧摘要自检（就绪后第 8 帧；PASS 退 0 / FAIL 退 1 / 读回失败退 2）
//   RTT_DUMP=<tag>      就绪后第 8 帧帧内回读主场景颜色靶落原始字节后退出
//                       （`rttdump_<tag>_WxH_fmt.raw`）

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
            GLogInfo("[RenderToTexture] %s tex=null", tag);
            return;
        }

        GLogInfo("[RenderToTexture] %s tex=%p image=%p view=%p layout=%u",
                tag,
                (void *)tex,
                (void *)tex->GetImage(),
                (void *)tex->GetVulkanImageView(),
                (uint32_t)tex->GetImageLayout());
    }

    bool LogStageFail(const char *stage, const char *reason)
    {
        GLogError("[RenderToTexture][%s] %s", stage, reason);
        return false;
    }

    void LogStage(const char *stage, const char *message)
    {
        GLogInfo("[RenderToTexture][%s] %s", stage, message);
    }

    // env 开关：Tick 的旋转冻结与摘要/落盘计数共用
    const char *s_dump_tag   = getenv("RTT_DUMP");
    const bool  s_selfcheck  = (getenv("RTT_SELFCHECK") != nullptr);
}

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

    bool InitMaterialDataSSBO(ECSContext *world)
    {
        GLogInfo("[RenderToTexture][OffscreenPass::InitMaterialDataSSBO] begin world=%p",
                 (void *)world);
        if (!world)
            return LogStageFail("OffscreenPass::InitMaterialDataSSBO", "invalid input pointers");

        auto *gc = world->GetGraphicsContext();
        if (!gc && render_context)
            gc = render_context->GetGraphicsContext();

        if (!gc)
        {
            GLogError("[RenderToTexture][OffscreenPass::InitMaterialDataSSBO] world_gc=%p render_context=%p rc_gc=%p",
                      (void *)(world ? world->GetGraphicsContext() : nullptr),
                      (void *)render_context,
                      (void *)(render_context ? render_context->GetGraphicsContext() : nullptr));
        }
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

    Texture2D *GetColorTexture() const
    {
        return offscreen ? offscreen->GetColorTexture(0) : nullptr;
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
        GLogInfo("[RenderToTexture][OffscreenPass::Init] begin owner=%p size=%ux%u",
                 (void *)owner, width, height);

        // 离屏世界：一行描述 + 一次 Create，RT / ECSContext / 渲染系统全部就位
        graph::OffscreenWorldDesc desc;
        desc.width  = width;
        desc.height = height;
        desc.name   = "RenderToTexture_Offscreen";
        desc.resource_prefix = "RenderToTexture:OffscreenRT";

        // 清屏色声明在 desc 里，成为 RT 上的权威值。
        // RTT 内容会被主场景 Lit 材质再乘一次光照（kd*NdotL/π ≈ 0.16），
        // 离屏用亮天蓝补偿，避免贴到立方体上整体发黑。
        desc.clear_color = GetColor4f(COLOR::LightSkyBlue, 1.0f);

        offscreen = graph::OffscreenWorld::Create(owner->GetGraphicsContext(),
                                                  owner->GetECSContext(),
                                                  desc);
        if (!offscreen)
            return LogStageFail("OffscreenPass::Init", "OffscreenWorld::Create failed");

        // RTT 内容会被主场景 Lit 材质再乘一次光照（kd*NdotL/π ≈ 0.16），
        // 离屏用高太阳强度 profile 补偿，避免贴到立方体上整体发黑。
        if (auto *gc = owner->GetGraphicsContext())
        {
            if (auto *env_manager = gc->GetEnvironmentManager())
            {
                graph::EnvironmentInfo info{};

                // T6b：光源参数已迁到**世界私有光源行**（不再跟 profile）—— profile 里只剩天空外观
                // （底色 / 光晕强度；光晕色恒为主光色，故不需要也写入）。
                const graph::SunLightSample sun = graph::EvaluateSunLight(8.0f, 30.0f, 0.0f);

                offscreen_env_profile = env_manager->Create("RenderToTexture.OffscreenBright", info);
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
        render_context = owner->GetRenderContext();
        GLogInfo("[RenderToTexture][OffscreenPass::Init] success world=%p rt=%p",
                 (void *)offscreen->GetWorld(), (void *)offscreen->GetRenderTarget());
        return true;
    }

    bool BuildSphere(WorkObject *owner)
    {
        GLogInfo("[RenderToTexture][OffscreenPass::BuildSphere] begin owner=%p world=%p rt=%p",
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

        // Bindless registration is deferred until ECS systems are ready.

        if (!InitMaterialDataSSBO(offscreen->GetWorld()))
            return LogStageFail("OffscreenPass::BuildSphere", "InitMaterialDataSSBO failed");

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateStandardGeometryVertexFormat());
        geometry = inline_geometry::CreateSphere(pc.get(), 64);
        if (!geometry)
            return LogStageFail("OffscreenPass::BuildSphere", "CreateSphere geometry failed");

        gm->Add(geometry);

        const mtl::MaterialRecipe *loaded_sphere_recipe =
            owner->GetRecipe("RenderToTexture.OffscreenSphere");
        if (!loaded_sphere_recipe)
            return LogStageFail("OffscreenPass::BuildSphere", "material recipe not found");
        sphere_recipe = *loaded_sphere_recipe;
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
        transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
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
        camera->distance = 6.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        auto camera_system = offscreen->GetCameraSystem();
        camera->viewport_info = camera_system ? camera_system->GetViewportInfo() : nullptr;

        LogStage("OffscreenPass::BuildSphere", "success");
        return true;
    }

    /// 驱动离屏世界一帧。就绪前每帧调用；一旦 `IsContentReady()` 为真即停止驱动。
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

class RenderToTextureApp final: public WorkObject
{
private:
    OffscreenPass *offscreen = nullptr;

    ECSContext *ecs_context = nullptr;
    Entity *camera_entity = nullptr;
    Entity *cube_entity = nullptr;

    PrimitiveAsset cube_asset;
    graph::mtl::MaterialRecipe cube_recipe{};
    using MaterialDataAccessor =
        graph::GlobalSSBODataAccessor;

    MaterialDataAccessor cube_material_data_ssbo_accessor{};
    graph::ssbo::PBRSurfaceRow cube_material_data{};

    /// 材质授权统一走句柄(TextureAsset)：底座直接给 `rt:` URI，其余给文件名。
    /// 就绪判定（含离屏内容是否已画出）由 ECS 侧统一负责，示例不再持有裸纹理。
    TextureAsset *base_asset = nullptr;
    TextureAsset *normal_asset = nullptr;
    TextureAsset *roughness_asset = nullptr;

    hgl::ecs::TransformAccessor cube_transform;
    float cube_theta = 0.0f;

private:
    bool SetupMainCamera()
    {
        LogStage("RenderToTextureApp::SetupMainCamera", "begin");
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return LogStageFail("RenderToTextureApp::SetupMainCamera", "ECS context/camera system unavailable");

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target = math::Vector3f(0, 0, 0);
        camera->distance = 5.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->viewport_info = GetViewportInfo();
        LogStage("RenderToTextureApp::SetupMainCamera", "success");
        return true;
    }

    bool CreateOffscreenRT()
    {
        LogStage("RenderToTextureApp::CreateOffscreenRT", "begin");
        offscreen = new OffscreenPass();
        if (!offscreen)
            return LogStageFail("RenderToTextureApp::CreateOffscreenRT", "new OffscreenPass failed");

        if (!offscreen->Init(this, 512, 512))
            return LogStageFail("RenderToTextureApp::CreateOffscreenRT", "OffscreenPass::Init failed");

        if (!offscreen->BuildSphere(this))
            return LogStageFail("RenderToTextureApp::CreateOffscreenRT", "OffscreenPass::BuildSphere failed");

        // 这里**不再**同步渲染一次：离屏世界的球体贴图是异步加载的，此刻任何一次
        // 绘制都只会得到清屏色，而且会把「已产出内容」这个状态永久钉死。
        // 由 Tick() 逐帧驱动离屏世界，待其内部资源齐备、内容真正画出来后自动停止。

        LogStage("RenderToTextureApp::CreateOffscreenRT", "success");
        return true;
    }

    bool CreateCube()
    {
        LogStage("RenderToTextureApp::CreateCube", "begin");
        auto *gc = GetGraphicsContext();
        if (!gc)
            return LogStageFail("RenderToTextureApp::CreateCube", "graphics context is null");

        auto *tm = GetManager<TextureManager>();
        auto *gm = GetManager<GeometryManager>();
        auto *device = gc->GetDevice();
        if (!tm || !gm || !device)
            return LogStageFail("RenderToTextureApp::CreateCube", "required managers/device missing");

        // 底座 = 离屏 RT 的颜色附件，用 URI 声明式引用；不拿裸纹理、不再 WrapTextureAsset。
        // 句柄在 RT 创建时已由 RenderTargetManager 绑定，此刻「容器就绪、内容未就绪」，
        // ECS 的 CanRender 会拦住立方体，直到离屏 pass 真正画出内容。
        base_asset      = tm->CreateTextureAsset(OS_TEXT("rt:RenderToTexture:OffscreenRT:color0"));
        normal_asset    = tm->CreateTextureAsset(OS_TEXT("res/image/Brickwall/Normal.Tex2D"), true);
        roughness_asset = tm->CreateTextureAsset(OS_TEXT("res/image/Brickwall/Roughness.Tex2D"), true);

        if (!base_asset || !normal_asset || !roughness_asset)
            return LogStageFail("RenderToTextureApp::CreateCube", "create texture asset failed");

        cube_material_data.base_color = Color4f(1.0f);
        cube_material_data.metallic = 0.08f;
        cube_material_data.roughness = 0.92f;
        cube_material_data.normal_scale = 0.35f;

        if (!InitMaterialDataSSBO())
            return LogStageFail("RenderToTextureApp::CreateCube", "InitMaterialDataSSBO failed");

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateStandardGeometryVertexFormat());
        inline_geometry::CubeCreateInfo cci{};
        cci.tex_coord = true;
        cci.ntb = NTBType::Normal;

        Geometry *cube_geometry = inline_geometry::CreateCube(pc.get(), &cci);
        if (!cube_geometry)
            return LogStageFail("RenderToTextureApp::CreateCube", "CreateCube geometry failed");

        gm->Add(cube_geometry);

        const mtl::MaterialRecipe *loaded_cube_recipe =
            GetRecipe("RenderToTexture.Cube");
        if (!loaded_cube_recipe)
            return LogStageFail("RenderToTextureApp::CreateCube", "material recipe not found");
        cube_recipe = *loaded_cube_recipe;
        if (!(cube_recipe.material_ssbo_binding = cube_material_data_ssbo_accessor.GetGlobalSSBOBinding()).IsValid())
            return LogStageFail("RenderToTextureApp::CreateCube", "register material SSBO binding failed");

        // 底座 = 离屏 RT 的颜色附件（`rtex:` 身份），其余两张是文件纹理。
        // 采样器由 Bindless 预设统一管理，材质侧不再持有 Sampler。
        if (!cube_recipe.AddTexture("base_color", base_asset, true)
         || !cube_recipe.AddTexture("normal", normal_asset, true)
         || !cube_recipe.AddTexture("roughness", roughness_asset, true))
            return LogStageFail("RenderToTextureApp::CreateCube", "bind recipe textures failed");

        cube_asset = PrimitiveAsset(cube_geometry, &cube_recipe, PrimitiveType::Triangles);
        if (!cube_asset.IsValid())
            return LogStageFail("RenderToTextureApp::CreateCube", "create cube primitive asset failed");

        cube_entity = ecs_context->CreateEntity<Entity>("RTTCube");
        cube_transform = ecs_context->GetTransform(ecs_context->CreateTransform(cube_entity->GetEntityID(), Mobility::Static));
        auto cube_prim_comp = cube_entity->GetContext()->GetOrCreateGeometryData(cube_entity);
        hgl::ecs::MaterialData *material_data_comp = cube_entity->GetContext()->GetOrCreateMaterialData(cube_entity);

        cube_transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        cube_transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        cube_transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        cube_transform.SetMobility(Mobility::Movable);

        cube_prim_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(cube_prim_comp->GetOwnerID())->SetPrimitiveAsset(&cube_asset);
        // 纹理已由配方声明（cube_recipe.AddTexture），实体端不再重复绑定
        hgl::ecs::MaterialData::MaterialDataAuthoringResource cube_struct{};
        cube_struct = cube_material_data_ssbo_accessor.GetGlobalSSBOBinding();
        material_data_comp->SetDataResource(cube_struct);
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删
        LogStage("RenderToTextureApp::CreateCube", "success");
        return true;
    }
    bool InitMaterialDataSSBO()
    {
        GLogInfo("[RenderToTexture][RenderToTextureApp::InitMaterialDataSSBO] begin ecs=%p",
                 (void *)ecs_context);
        if (!ecs_context)
            return LogStageFail("RenderToTextureApp::InitMaterialDataSSBO", "invalid input pointers");

        auto *domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return LogStageFail("RenderToTextureApp::InitMaterialDataSSBO", "resource domain manager is null");

        cube_material_data_ssbo_accessor = domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>();
        if (!cube_material_data_ssbo_accessor)
            return LogStageFail("RenderToTextureApp::InitMaterialDataSSBO", "CreateSSBO failed");

        const graph::ssbo::PBRSurfaceRow material_data = cube_material_data;
        if (!cube_material_data_ssbo_accessor.Write(material_data))
            return LogStageFail("RenderToTextureApp::InitMaterialDataSSBO", "write material data failed");

        LogStage("RenderToTextureApp::InitMaterialDataSSBO", "success");
        return true;
    }
public:
    ~RenderToTextureApp() override
    {
        SAFE_CLEAR(offscreen)
    }

    bool Init() override
    {
        LogStage("RenderToTextureApp::Init", "begin");
        ecs_context = GetECSContext();
        if (!ecs_context)
            return LogStageFail("RenderToTextureApp::Init", "ECS context is null");

        ecs_context->SetResourceNamePrefix("RenderToTexture:MainScene");

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
            return LogStageFail("RenderToTextureApp::Init", "CreateOffscreenRT failed");

        if (!CreateCube())
            return LogStageFail("RenderToTextureApp::Init", "CreateCube failed");

        if (!SetupMainCamera())
            return LogStageFail("RenderToTextureApp::Init", "SetupMainCamera failed");

        LogStage("RenderToTextureApp::Init", "success");
        return true;
    }

    void Tick(double delta_time) override
    {
        // RTT_SELFCHECK=1：**帧摘要自检**（§6.5；共用件 example/Shared/FrameDigest.h）——
        // 就绪后第 8 帧帧内回读主场景颜色靶 ⇒ {不同字节取值数, FNV-1a 64} 比对下方内联常量：
        // PASS 退出 0 / FAIL 退出 1 / 读回失败退出 2。
        // 判据意义 = **离屏世界回归床**：主场景立方体贴的是离屏球画出的 rt: 附件，
        // 离屏世界若再"永不绘制"（五层缺陷族任一回潮），贴图退化为纯清屏色或压根不渲染 ⇒ 摘要立变。
        // 摘要必须冻结（自旋停走 + 定帧），否则两次运行几何不同 ⇒ 假红。

        // 逻辑更新写在 Tick（渲染前）——TransformSystem 在渲染帧内提交变换，
        // Tick 里改与本回调内改同帧等价，且不占用命令缓冲录制时间
        if (!s_selfcheck && cube_transform.IsValid())
        {
            cube_theta += static_cast<float>(delta_time) * 0.8f;
            cube_theta = fmodf(cube_theta, 2.0f * std::numbers::pi_v<float>);

            const glm::quat rot_z = glm::angleAxis(cube_theta, glm::vec3(0.0f, 0.0f, 1.0f));
            const glm::quat rot_x = glm::angleAxis(cube_theta * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
            cube_transform.SetLocalRotation(rot_z * rot_x);
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
            static constexpr uint64_t kSelfFNV         = 0x2D730BDC6D96FD49ull;  // 冻结自旋 + 就绪后第 8 帧实测（三次连跑一致）
            // 生产端（离屏附件本体）摘要：主场景摘要是"经材质采样后的二手证据"，
            // 这里钉一手证据 —— 离屏世界若"永不绘制"，附件退化为纯清屏色 ⇒ 本值必变。
            static constexpr uint64_t kOffFNV          = 0xE4DE179225AD6E74ull;

            if (s_dump_tag && ++s_dump_frame == 8)
            {
                auto *rt = dynamic_cast<graph::SwapchainRenderTarget *>(
                    ecs_context ? ecs_context->GetRenderTarget() : nullptr);
                if (!rt)
                {
                    printf("[RTTDUMP] no swapchain RT\n");
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
                        printf("[RTTDUMP] readback failed\n");
                        std::exit(2);
                    }

                    char name[256];
                    snprintf(name, sizeof(name), "rttdump_%s_%ux%u_%s.raw",
                             tag, info.width, info.height,
                             GetVulkanFormat(info.format) ? GetVulkanFormat(info.format)->name : "unknown");

                    FILE *fp = fopen(name, "wb");
                    if (!fp)
                    {
                        printf("[RTTDUMP] open %s failed\n", name);
                        std::exit(2);
                    }

                    fwrite(pixels.data(), 1, pixels.size(), fp);
                    fclose(fp);
                    printf("[RTTDUMP] wrote %s (%zu bytes)\n", name, pixels.size());
                    std::exit(0);
                });
            }

            if (s_selfcheck && ++s_self_frame == 8)
            {
                auto *rt = dynamic_cast<graph::SwapchainRenderTarget *>(
                    ecs_context ? ecs_context->GetRenderTarget() : nullptr);
                if (!rt)
                {
                    printf("[RTTSELF] no swapchain RT\n");
                    std::exit(2);
                }

                // 生产端摘要：离屏附件本身（不经主场景采样）。主场景摘要红而这里绿 ⇒
                // 缺陷在"关联/采样"侧；两侧同时绿才算离屏管线整体成立。
                if (auto *ort = offscreen ? offscreen->GetRenderTarget() : nullptr)
                {
                    std::vector<uint8_t> px;
                    graph::TextureReadbackInfo oinfo;
                    if (graph::ReadbackColorTarget(ort, px, 0, &oinfo))
                    {
                        uint32_t d = 0;
                        {
                            uint8_t seen[256] = {};
                            for (uint8_t b : px) seen[b] = 1;
                            for (uint32_t i = 0; i < 256; ++i) d += seen[i];
                        }

                        hgl::hash::FNV1aHasher64 hasher;
                        hasher.AppendBytes(px.data(), px.size());
                        const uint64_t ofnv = hasher;

                        printf("[RTTORT] offscreen %ux%u %s bytes=%zu distinct=%u fnv=0x%016llX\n",
                               oinfo.width, oinfo.height,
                               GetVulkanFormatName(oinfo.format),
                               px.size(), d, (unsigned long long)ofnv);

                        if (ofnv != kOffFNV)
                        {
                            printf("[RTTORT] FAIL（期望 fnv=0x%016llX）\n", (unsigned long long)kOffFNV);
                            std::exit(1);
                        }
                    }
                    else
                    {
                        printf("[RTTORT] offscreen readback failed\n");
                        std::exit(2);
                    }
                }

                ulre_example::FrameDigest expect;
                expect.width                = kSelfW;
                expect.height               = kSelfH;
                expect.distinct_byte_values = kSelfMinDistinct;
                expect.fnv1a64              = kSelfFNV;

                ulre_example::InstallFrameDigestHook(rt, expect, "RTTSELF");
            }
        }
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<RenderToTextureApp>(OS_TEXT("Render To Texture"), argc, argv, 1280, 720);
}
