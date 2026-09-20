#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/module/OffscreenWorld.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/MaterialSSBOBufferRegistry.h>
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
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/RenderSystemCore.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>

#include <memory>
#include <numbers>

/**
 * ShadowMap —— depth-only RenderTarget 的验证用例
 *
 * 验证目标（按链路顺序）：
 *   1. RenderTargetDesc::OffscreenDepthOnly 能建出零颜色附件的 RT
 *      —— color_count == 0、has_depth == true、深度纹理非空
 *   2. depth-only 目标的材质管线能通过 PipelineResolver 的校验并成功创建
 *      （原实现在 color_attachment_count == 0 时直接拒绝）
 *   3. 渲染结束后深度纹理处于可采样布局（SHADER_READ_ONLY_OPTIMAL）
 *      —— 这是 shadow map 能被采样的前提
 *   4. 该深度纹理可以直接绑定到主世界材质并采样显示
 *
 * 用法：离屏世界以"光源视角"渲染若干物体到 depth-only RT，主世界的立方体
 * 以该深度纹理作为 albedo 显示，直观呈现 shadow map 的内容。
 *
 * 实跑结果（Debug + Khronos validation layer）：
 *   - 零颜色附件目标创建成功：color_count=0、has_depth=1、深度格式 D32_SFLOAT
 *   - 深度在渲染前为 layout=3（DEPTH_STENCIL_ATTACHMENT_OPTIMAL），
 *     渲染后转为 layout=5（SHADER_READ_ONLY_OPTIMAL），即"可采样"
 *   - 该深度纹理被 BindlessTextureManager 正常注册（handle=1），
 *     主世界材质采样成功
 *   - validation 无 ERROR；仅有一条 MsgCode:0 提示：片段着色器写出的
 *     outColor 因目标没有颜色附件而被丢弃——这是 depth-only 渲染的预期行为
 *     （材质管线是共享的，后续若要消除该提示需引入不写颜色的深度专用管线）
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

    bool LogStageFail(const char *stage, const char *reason)
    {
        GLogError("[ShadowMap][%s] %s", stage, reason);
        return false;
    }

    void LogStage(const char *stage, const char *message)
    {
        GLogInfo("[ShadowMap][%s] %s", stage, message);
    }

    /// 打印深度目标的关键状态，作为"链路是否打通"的客观证据
    /// @param require_samplable 为真时要求深度已处于可采样布局（渲染后才成立；
    ///        刚创建、尚未渲染的目标停留在 attachment 布局是正常的）
    void ReportDepthTarget(const char *stage, IRenderTarget *rt, Texture2D *depth_tex, bool require_samplable)
    {
        if (!rt)
        {
            GLogError("[ShadowMap][%s] render target is null", stage);
            return;
        }

        GLogInfo("[ShadowMap][%s] color_count=%u has_depth=%d depth_tex=%p",
                 stage,
                 rt->GetColorCount(),
                 rt->hasDepth() ? 1 : 0,
                 (void *)depth_tex);

        if (!depth_tex)
        {
            GLogError("[ShadowMap][%s] depth texture is null", stage);
            return;
        }

        GLogInfo("[ShadowMap][%s] depth_image=%p view=%p format=%d layout=%u extent=%ux%u",
                 stage,
                 (void *)depth_tex->GetImage(),
                 (void *)depth_tex->GetVulkanImageView(),
                 (int)depth_tex->GetFormat(),
                 (uint32_t)depth_tex->GetImageLayout(),
                 depth_tex->GetWidth(),
                 depth_tex->GetHeight());

        // 可采样判据：渲染结束后深度必须离开 attachment 布局。
        // 5 == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        // 3 == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL（未转换，无法采样）
        const bool samplable = ((uint32_t)depth_tex->GetImageLayout() == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        if (!require_samplable)
        {
            GLogInfo("[ShadowMap][%s] depth layout before render (samplable=%d)", stage, samplable ? 1 : 0);
            return;
        }

        if (samplable)
            LogStage(stage, "depth layout is SHADER_READ_ONLY_OPTIMAL (samplable)");
        else
            GLogError("[ShadowMap][%s] depth layout is NOT samplable (layout=%u)",
                      stage, (uint32_t)depth_tex->GetImageLayout());
    }
}

/// 深度 Pass：以光源视角把场景渲染进 depth-only RT
class ShadowDepthPass
{
private:
    std::unique_ptr<graph::OffscreenWorld> offscreen;

    RenderContext *render_context = nullptr;

    Geometry *geometry = nullptr;
    PrimitiveAsset sphere_asset;
    graph::mtl::MaterialRecipe sphere_recipe{};

    using MaterialDataAccessor = graph::MaterialSSBODataAccessor;

    MaterialDataAccessor material_data_ssbo_accessor{};
    graph::ssbo::PBRSurfaceRow sphere_material_data{};
    Entity *sphere_entity = nullptr;

private:

    bool InitMaterialDataSSBO(ECSContext *world)
    {
        if (!world)
            return LogStageFail("ShadowDepthPass::InitMaterialDataSSBO", "invalid world");

        auto *gc = world->GetGraphicsContext();
        if (!gc && render_context)
            gc = render_context->GetGraphicsContext();

        if (!gc)
            return LogStageFail("ShadowDepthPass::InitMaterialDataSSBO", "graphics context is null");

        auto *domain_manager = gc->GetMaterialSSBOBufferRegistry();
        if (!domain_manager)
            return LogStageFail("ShadowDepthPass::InitMaterialDataSSBO", "material ssbo registry is null");

        material_data_ssbo_accessor = domain_manager->GetMaterialDataAccessor<graph::ssbo::PBRSurfaceRow>();
        if (!material_data_ssbo_accessor)
            return LogStageFail("ShadowDepthPass::InitMaterialDataSSBO", "create accessor failed");

        if (!material_data_ssbo_accessor.Write(sphere_material_data))
            return LogStageFail("ShadowDepthPass::InitMaterialDataSSBO", "write material data failed");

        return true;
    }

public:

    ~ShadowDepthPass()
    {
        GraphicsContext *gc = render_context ? render_context->GetGraphicsContext() : nullptr;
        if (!gc)
        {
            if (auto *w = offscreen ? offscreen->GetWorld() : nullptr)
                gc = w->GetGraphicsContext();
        }

        if (gc && geometry)
        {
            if (auto *gm = gc->GetGeometryManager())
                gm->Release(geometry);
        }

        geometry = nullptr;
    }

    Texture2D *GetDepthTexture() const
    {
        return offscreen ? offscreen->GetDepthTexture() : nullptr;
    }

    IRenderTarget *GetRenderTarget() const
    {
        return offscreen ? offscreen->GetRenderTarget() : nullptr;
    }

    bool Init(WorkObject *owner, const uint32_t width, const uint32_t height)
    {
        LogStage("ShadowDepthPass::Init", "begin");

        if (!owner)
            return LogStageFail("ShadowDepthPass::Init", "owner is null");

        // depth-only 离屏世界：零颜色附件，深度可采样
        graph::OffscreenWorldDesc desc;
        desc.width            = width;
        desc.height           = height;
        desc.name             = "ShadowMap_DepthWorld";
        desc.resource_prefix  = "ShadowMap:DepthRT";
        desc.depth_only       = true;
        desc.depth_format     = PF_D32F;     // 纯深度格式：顺带验证 stencil 附件声明的处理

        offscreen = graph::OffscreenWorld::Create(owner->GetGraphicsContext(),
                                                  owner->GetECSContext(),
                                                  desc);
        if (!offscreen)
            return LogStageFail("ShadowDepthPass::Init", "OffscreenWorld::Create failed");

        render_context = owner->GetRenderContext();

        if (auto *rt = GetRenderTarget())
        {
            if (rt->GetColorCount() != 0)
            {
                GLogError("[ShadowMap][ShadowDepthPass::Init] expected depth-only target but color_count=%u",
                          rt->GetColorCount());
                return false;
            }

            if (!rt->hasDepth())
                return LogStageFail("ShadowDepthPass::Init", "depth-only target has no depth attachment");
        }

        ReportDepthTarget("ShadowDepthPass::Init", GetRenderTarget(), GetDepthTexture(), false);

        LogStage("ShadowDepthPass::Init", "success");
        return true;
    }

    bool BuildScene(WorkObject *owner)
    {
        LogStage("ShadowDepthPass::BuildScene", "begin");

        if (!owner || !offscreen || !offscreen->IsValid())
            return LogStageFail("ShadowDepthPass::BuildScene", "owner/offscreen world missing");

        GraphicsContext *gc = owner->GetGraphicsContext();
        if (!gc)
            return LogStageFail("ShadowDepthPass::BuildScene", "graphics context is null");

        auto *gm = owner->GetManager<GeometryManager>();
        auto *device = gc->GetDevice();
        if (!gm || !device)
            return LogStageFail("ShadowDepthPass::BuildScene", "geometry manager/device missing");

        sphere_material_data.base_color   = GetColor4f(COLOR::White, 1.0f);
        sphere_material_data.metallic     = 0.0f;
        sphere_material_data.roughness    = 1.0f;
        sphere_material_data.normal_scale = 1.0f;

        if (!InitMaterialDataSSBO(offscreen->GetWorld()))
            return LogStageFail("ShadowDepthPass::BuildScene", "InitMaterialDataSSBO failed");

        auto pc = std::make_unique<GeometryCreater>(device, CreateStandardGeometryVertexFormat());
        geometry = inline_geometry::CreateSphere(pc.get(), 48);
        if (!geometry)
            return LogStageFail("ShadowDepthPass::BuildScene", "CreateSphere geometry failed");

        gm->Add(geometry);

        sphere_recipe.recipe_name = "ShadowMap.DepthSphere";
        sphere_recipe.mtl_def_id  = "Lit";
        sphere_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        if (!(sphere_recipe.material_ssbo_binding = material_data_ssbo_accessor.GetMaterialSSBOBinding()).IsValid())
            return LogStageFail("ShadowDepthPass::BuildScene", "register material SSBO binding failed");

        sphere_asset = PrimitiveAsset(geometry, &sphere_recipe, PrimitiveType::Triangles);
        if (!sphere_asset.IsValid())
            return LogStageFail("ShadowDepthPass::BuildScene", "create primitive asset failed");

        auto *world = offscreen->GetWorld();

        sphere_entity = world->CreateEntity<Entity>("DepthSphere");
        auto transform = sphere_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto prim_comp = sphere_entity->AddComponent<PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitiveAsset(&sphere_asset);
        hgl::ecs::PrimitiveComponent::MaterialDataAuthoringResource sphere_struct{};
        sphere_struct = material_data_ssbo_accessor.GetMaterialSSBOBinding();
        prim_comp->SetMaterialDataResource(sphere_struct);
        prim_comp->SetVisible(true);

        // 光源视角：从斜上方俯视原点，模拟方向光 shadow map 的拍摄角度
        Entity *camera_entity = world->CreateEntity<Entity>("DepthCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();
        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0, 0, 0);
        camera->distance = 5.0f;
        camera->yaw = 35.0f;
        camera->pitch = -50.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        auto camera_system = offscreen->GetCameraSystem();
        camera->camera_data   = camera_system ? camera_system->GetCamera() : nullptr;
        camera->camera_info   = const_cast<CameraInfo *>(camera_system ? camera_system->GetCameraInfo() : nullptr);
        camera->viewport_info = camera_system ? camera_system->GetViewportInfo() : nullptr;

        LogStage("ShadowDepthPass::BuildScene", "success");
        return true;
    }

    bool RenderDepth()
    {
        LogStage("ShadowDepthPass::RenderDepth", "begin");

        if (!offscreen)
            return LogStageFail("ShadowDepthPass::RenderDepth", "offscreen world is null");

        // 走 ECSContext::RenderTo()，与主窗口路径共用同一套帧驱动。
        // depth-only 目标没有颜色附件，清屏色参数无意义。
        offscreen->Render();

        ReportDepthTarget("ShadowDepthPass::RenderDepth", GetRenderTarget(), GetDepthTexture(), true);

        LogStage("ShadowDepthPass::RenderDepth", "success");
        return true;
    }
};

class ShadowMapApp final : public WorkObject
{
private:
    ShadowDepthPass *depth_pass = nullptr;

    ECSContext *ecs_context = nullptr;
    Entity *camera_entity = nullptr;
    Entity *plane_entity = nullptr;

    PrimitiveAsset plane_asset;
    graph::mtl::MaterialRecipe plane_recipe{};

    using MaterialDataAccessor = graph::MaterialSSBODataAccessor;

    MaterialDataAccessor plane_material_data_ssbo_accessor{};
    graph::ssbo::PBRSurfaceRow plane_material_data{};

    Sampler *plane_sampler = nullptr;
    Texture2D *depth_tex = nullptr;

    std::shared_ptr<TransformComponent> plane_transform;
    float plane_theta = 0.0f;

private:

    bool SetupMainCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return LogStageFail("ShadowMapApp::SetupMainCamera", "camera system unavailable");

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0, 0, 0);
        camera->distance = 5.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data   = GetCamera();
        camera->camera_info   = const_cast<CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

    bool InitPlaneMaterialSSBO()
    {
        auto *domain_manager = GetManager<MaterialSSBOBufferRegistry>();
        if (!domain_manager)
            return LogStageFail("ShadowMapApp::InitPlaneMaterialSSBO", "material ssbo registry is null");

        plane_material_data_ssbo_accessor = domain_manager->GetMaterialDataAccessor<graph::ssbo::PBRSurfaceRow>();
        if (!plane_material_data_ssbo_accessor)
            return LogStageFail("ShadowMapApp::InitPlaneMaterialSSBO", "create accessor failed");

        if (!plane_material_data_ssbo_accessor.Write(plane_material_data))
            return LogStageFail("ShadowMapApp::InitPlaneMaterialSSBO", "write material data failed");

        return true;
    }

    /// 主世界：一个立方体，albedo 直接取 depth-only RT 的深度纹理
    bool CreatePlane()
    {
        LogStage("ShadowMapApp::CreatePlane", "begin");

        auto *gc = GetGraphicsContext();
        if (!gc)
            return LogStageFail("ShadowMapApp::CreatePlane", "graphics context is null");

        auto *sm = GetManager<SamplerManager>();
        auto *gm = GetManager<GeometryManager>();
        auto *device = gc->GetDevice();
        if (!sm || !gm || !device)
            return LogStageFail("ShadowMapApp::CreatePlane", "required managers/device missing");

        plane_sampler = sm->CreateSampler();
        if (!plane_sampler)
            return LogStageFail("ShadowMapApp::CreatePlane", "CreateSampler failed");

        // 深度纹理直接作为 albedo 采样——这正是 depth-only 目标的价值所在
        depth_tex = depth_pass ? depth_pass->GetDepthTexture() : nullptr;
        if (!depth_tex)
            return LogStageFail("ShadowMapApp::CreatePlane", "depth texture unavailable from depth pass");

        plane_material_data.base_color   = Color4f(1.0f);
        plane_material_data.metallic     = 0.0f;
        plane_material_data.roughness    = 1.0f;
        plane_material_data.normal_scale = 1.0f;

        if (!InitPlaneMaterialSSBO())
            return LogStageFail("ShadowMapApp::CreatePlane", "InitPlaneMaterialSSBO failed");

        auto pc = std::make_unique<GeometryCreater>(device, CreateStandardGeometryVertexFormat());
        inline_geometry::CubeCreateInfo cci{};
        cci.tex_coord = true;
        cci.ntb = NTBType::Normal;

        Geometry *cube_geometry = inline_geometry::CreateCube(pc.get(), &cci);
        if (!cube_geometry)
            return LogStageFail("ShadowMapApp::CreatePlane", "CreateCube geometry failed");

        gm->Add(cube_geometry);

        plane_recipe.recipe_name = "ShadowMap.DisplayCube";
        plane_recipe.mtl_def_id  = "Lit";
        plane_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        if (!(plane_recipe.material_ssbo_binding = plane_material_data_ssbo_accessor.GetMaterialSSBOBinding()).IsValid())
            return LogStageFail("ShadowMapApp::CreatePlane", "register material SSBO binding failed");

        plane_asset = PrimitiveAsset(cube_geometry, &plane_recipe, PrimitiveType::Triangles);
        if (!plane_asset.IsValid())
            return LogStageFail("ShadowMapApp::CreatePlane", "create cube primitive asset failed");

        plane_entity = ecs_context->CreateEntity<Entity>("DisplayCube");
        plane_transform = plane_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto prim_comp = plane_entity->AddComponent<PrimitiveComponent>();

        plane_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        plane_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        plane_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        plane_transform->SetMovable(true);

        prim_comp->SetPrimitiveAsset(&plane_asset);
        prim_comp->SetMaterialTextureResource("base_color", depth_tex, plane_sampler);
        hgl::ecs::PrimitiveComponent::MaterialDataAuthoringResource plane_struct{};
        plane_struct = plane_material_data_ssbo_accessor.GetMaterialSSBOBinding();
        prim_comp->SetMaterialDataResource(plane_struct);
        prim_comp->SetVisible(true);

        LogStage("ShadowMapApp::CreatePlane", "success");
        return true;
    }

public:

    ~ShadowMapApp() override
    {
        SAFE_CLEAR(depth_pass)

        if (auto *gc = GetGraphicsContext())
        {
            if (plane_sampler)
            {
                if (auto *sm = gc->GetManager<SamplerManager>())
                    sm->Release(plane_sampler);
            }
        }

        plane_sampler = nullptr;
        depth_tex = nullptr;
    }

    bool Init() override
    {
        LogStage("ShadowMapApp::Init", "begin");

        ecs_context = GetECSContext();
        if (!ecs_context)
            return LogStageFail("ShadowMapApp::Init", "ECS context is null");

        ecs_context->SetResourceNamePrefix("ShadowMap:MainScene");

        auto environment_system = ecs_context->GetSystem<EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterRenderSystem<EnvironmentSystem>();

        if (environment_system)
        {
            if (auto *sky = environment_system->EditSkyInfo())
            {
                sky->SetTime(8, 30, 0);
                environment_system->MarkSkyDirty();
            }
        }

        // 深度 Pass：以光源视角渲染到 depth-only RT
        depth_pass = new ShadowDepthPass();
        if (!depth_pass->Init(this, 512, 512))
            return LogStageFail("ShadowMapApp::Init", "ShadowDepthPass::Init failed");

        if (!depth_pass->BuildScene(this))
            return LogStageFail("ShadowMapApp::Init", "ShadowDepthPass::BuildScene failed");

        if (!depth_pass->RenderDepth())
            return LogStageFail("ShadowMapApp::Init", "ShadowDepthPass::RenderDepth failed");

        // 主世界：采样该深度纹理
        if (!CreatePlane())
            return LogStageFail("ShadowMapApp::Init", "CreatePlane failed");

        if (!SetupMainCamera())
            return LogStageFail("ShadowMapApp::Init", "SetupMainCamera failed");

        GLogInfo("[ShadowMap][ShadowMapApp::Init] success depth_tex=%p layout=%u",
                 (void *)depth_tex,
                 depth_tex ? (uint32_t)depth_tex->GetImageLayout() : 0u);

        LogStage("ShadowMapApp::Init", "success");
        return true;
    }

    void Render(double delta_time) override
    {
        if (plane_transform)
        {
            plane_theta += static_cast<float>(delta_time) * 0.6f;
            plane_theta = fmodf(plane_theta, 2.0f * std::numbers::pi_v<float>);

            plane_transform->SetLocalRotation(glm::angleAxis(plane_theta, glm::vec3(0.0f, 1.0f, 0.0f)));
        }

        WorkObject::Render(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<ShadowMapApp>(OS_TEXT("Shadow Map (depth-only RenderTarget)"), argc, argv, 1280, 720);
}
