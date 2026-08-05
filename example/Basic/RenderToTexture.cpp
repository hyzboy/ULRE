#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetSingle.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/color/Color.h>
#include<hgl/log/Log.h>
#include<hgl/mtl/MaterialRecipe.h>

#include<hgl/graph/ssbo/StandardMaterialInstance.h>

#include "../common/OffscreenWorldRuntime.h"

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
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>

#include <memory>
#include <cstring>
#include <numbers>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateGizmo3DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }

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
}

class OffscreenPass
{
private:
    hgl::example::OffscreenWorldRuntime runtime;

    RenderContext *render_context = nullptr;

    Geometry *geometry = nullptr;
    PrimitiveAsset sphere_asset;
    graph::mtl::MaterialRecipe sphere_recipe{};
    graph::SSBOArrayAccessor<Color4f>* mi_ssbo_accessor = nullptr;
    Color4f sphere_color_data{};
    Entity *sphere_entity = nullptr;
    std::shared_ptr<PrimitiveComponent> sphere_primitive_comp;

    void DumpOffscreenState(const char *stage)
    {
        auto *world = runtime.GetWorld();
        if (!world)
        {
            std::printf("[RenderToTextureDiag][%s] world=null\n", stage ? stage : "<null>");
            return;
        }

        auto &cache = world->GetRenderFrameCache();
        std::printf("[RenderToTextureDiag][%s] renderableCount=%u renderItems=%zu materialBatches=%u\n",
                    stage ? stage : "<null>",
                    cache.renderableCount,
                    cache.renderItems.size(),
                    cache.materialBatches.GetCount());

        if (sphere_primitive_comp)
        {
            std::printf("[RenderToTextureDiag][%s] primitive visible=%d hasRecipeSource=%d hasRecipeOverride=%d primitiveAsset=%p\n",
                        stage ? stage : "<null>",
                        sphere_primitive_comp->IsVisible() ? 1 : 0,
                        sphere_primitive_comp->HasAnyMaterialRecipeSource() ? 1 : 0,
                        sphere_primitive_comp->HasMaterialRecipeOverride() ? 1 : 0,
                        (void *)sphere_primitive_comp->GetPrimitiveAsset());
        }

    }


    bool InitMISSBO(ECSContext *world)
    {
        GLogInfo("[RenderToTexture][OffscreenPass::InitMISSBO] begin world=%p",
                 (void *)world);
        if (!world)
            return LogStageFail("OffscreenPass::InitMISSBO", "invalid input pointers");

        auto *gc = world->GetGraphicsContext();
        if (!gc && render_context)
            gc = render_context->GetGraphicsContext();

        if (!gc)
        {
            GLogError("[RenderToTexture][OffscreenPass::InitMISSBO] world_gc=%p render_context=%p rc_gc=%p",
                      (void *)(world ? world->GetGraphicsContext() : nullptr),
                      (void *)render_context,
                      (void *)(render_context ? render_context->GetGraphicsContext() : nullptr));
        }
        if (!gc)
            return LogStageFail("OffscreenPass::InitMISSBO", "graphics context is null");

        auto *domain_manager = gc->GetResourceDomainManager();
        if (!domain_manager)
            return LogStageFail("OffscreenPass::InitMISSBO", "resource domain manager is null");

        mi_ssbo_accessor = domain_manager->AllocateArrayAccessor<Color4f>(
            graph::mtl::SSBOType::EmissiveSurface,
            "RenderToTexture:OffscreenPass:MIData",
            1);
        if (!mi_ssbo_accessor)
            return LogStageFail("OffscreenPass::InitMISSBO", "CreateSSBO failed");

        (*mi_ssbo_accessor)[0] = sphere_color_data;
        mi_ssbo_accessor->Commit();

        LogStage("OffscreenPass::InitMISSBO", "success");
        return true;
    }
public:
    ~OffscreenPass()
    {
        GraphicsContext *gc = render_context ? render_context->GetGraphicsContext() : nullptr;
        if (!gc)
        {
            if (auto *w = runtime.GetWorld())
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

        delete mi_ssbo_accessor;
        mi_ssbo_accessor = nullptr;
        geometry = nullptr;
    }

    Texture2D *GetColorTexture() const
    {
        return runtime.GetColorTexture(0);
    }

    bool Init(WorkObject *owner, const uint32_t width, const uint32_t height)
    {
        GLogInfo("[RenderToTexture][OffscreenPass::Init] begin owner=%p size=%ux%u",
                 (void *)owner, width, height);
        hgl::example::OffscreenWorldConfig cfg;
        cfg.width = width;
        cfg.height = height;
        cfg.world_name = "RenderToTexture_Offscreen";
        cfg.resource_prefix = "RenderToTexture:OffscreenRT";
        cfg.register_input_system = false;

        if (!runtime.Init(owner, cfg))
            return LogStageFail("OffscreenPass::Init", "runtime.Init failed");

        LogTextureInfo("offscreen_rt_color0_init", runtime.GetColorTexture(0));
        render_context = owner->GetRenderContext();
        GLogInfo("[RenderToTexture][OffscreenPass::Init] success world=%p rt=%p",
                 (void *)runtime.GetWorld(), (void *)runtime.GetRenderTarget());
        return true;
    }

    bool BuildSphere(WorkObject *owner)
    {
        GLogInfo("[RenderToTexture][OffscreenPass::BuildSphere] begin owner=%p world=%p rt=%p",
                 (void *)owner, (void *)runtime.GetWorld(), (void *)runtime.GetRenderTarget());
        if (!owner || !runtime.GetWorld() || !runtime.GetRenderTarget())
            return LogStageFail("OffscreenPass::BuildSphere", "owner/world/render target missing");

        GraphicsContext *gc = owner->GetGraphicsContext();
        if (!gc)
            return LogStageFail("OffscreenPass::BuildSphere", "graphics context is null");

        auto *gm = owner->GetManager<GeometryManager>();
        auto *device = gc->GetDevice();
        if (!gm || !device)
            return LogStageFail("OffscreenPass::BuildSphere", "required managers/device missing");

        sphere_color_data = GetColor4f(COLOR::SkyBlue, 1.0f);

        if (!InitMISSBO(runtime.GetWorld()))
            return LogStageFail("OffscreenPass::BuildSphere", "InitMISSBO failed");

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateGizmo3DGeometryVertexFormat());
        geometry = inline_geometry::CreateSphere(pc.get(), 64);
        if (!geometry)
            return LogStageFail("OffscreenPass::BuildSphere", "CreateSphere geometry failed");

        gm->Add(geometry);

        sphere_recipe.recipe_name = "RenderToTexture.OffscreenSphere";
        sphere_recipe.mtl_def_id = "DebugNormalColor";
        sphere_recipe.pipeline_preset = PipelinePreset::Solid3D;
        sphere_recipe.domain = "RenderToTexture.Offscreen";
        sphere_asset = PrimitiveAsset(geometry, &sphere_recipe, PrimitiveType::Triangles);
        if (!sphere_asset.IsValid())
            return LogStageFail("OffscreenPass::BuildSphere", "create offscreen primitive asset failed");

        auto *world = runtime.GetWorld();
        sphere_entity = world->CreateEntity<Entity>("OffscreenSphere");
        auto transform = sphere_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto prim_comp = sphere_entity->AddComponent<PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitiveAsset(&sphere_asset);
        hgl::ecs::PrimitiveComponent::MaterialSSBONamedAuthoringResource sphere_struct{};
        sphere_struct.ssbo_name = graph::mtl::DefaultMaterialSSBOName;
        sphere_struct.ssbo_id = mi_ssbo_accessor->GetSSBOId();
        sphere_struct.ssbo_element_index = 0;
        sphere_struct.use_ssbo_element_index = true;
        sphere_struct.shared_across_instances = true;
        prim_comp->SetMaterialSSBOResource(sphere_struct);
        prim_comp->SetVisible(true);

        sphere_primitive_comp = prim_comp;

        Entity *camera_entity = world->CreateEntity<Entity>("OffscreenCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();
        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0, 0, 0);
        camera->distance = 6.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        auto camera_system = runtime.GetCameraSystem();
        camera->camera_data = camera_system ? camera_system->GetCamera() : nullptr;
        camera->camera_info = const_cast<CameraInfo *>(camera_system ? camera_system->GetCameraInfo() : nullptr);
        camera->viewport_info = camera_system ? camera_system->GetViewportInfo() : nullptr;

        LogStage("OffscreenPass::BuildSphere", "success");
        return true;
    }

    bool RenderOnce()
    {
        LogStage("OffscreenPass::RenderOnce", "begin");
        DumpOffscreenState("pre-renderonce");
        if (!runtime.RenderOnce(GetColor4f(COLOR::DarkSlateBlue, 1.0f)))
            return LogStageFail("OffscreenPass::RenderOnce", "runtime.RenderOnce failed");

        DumpOffscreenState("post-renderonce");
        LogStage("OffscreenPass::RenderOnce", "success");
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
    graph::SSBOArrayAccessor<ssbo::StandardMaterialInstance>* cube_mi_ssbo_accessor = nullptr;
    Sampler *cube_sampler = nullptr;
    ssbo::StandardMaterialInstance cube_mi_data{};

    Texture2D *base_tex = nullptr;
    Texture2D *fallback_albedo = nullptr;
    Texture2D *normal_tex = nullptr;
    Texture2D *roughness_tex = nullptr;
    std::unique_ptr<BindlessTextureManager> bindless_texture_manager;

    std::shared_ptr<TransformComponent> cube_transform;
    float cube_theta = 0.0f;

private:
    bool SetupMainCamera()
    {
        LogStage("RenderToTextureApp::SetupMainCamera", "begin");
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return LogStageFail("RenderToTextureApp::SetupMainCamera", "ECS context/camera system unavailable");

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0, 0, 0);
        camera->distance = 5.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<CameraInfo *>(GetCameraInfo());
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

        // This sample's offscreen content is static. Render once at startup to
        // avoid cross-pass write/read jitter on the same texture each frame.
        if (!offscreen->RenderOnce())
            return LogStageFail("RenderToTextureApp::CreateOffscreenRT", "OffscreenPass::RenderOnce failed");

        LogStage("RenderToTextureApp::CreateOffscreenRT", "success");
        return true;
    }

    bool CreateCube()
    {
        LogStage("RenderToTextureApp::CreateCube", "begin");
        auto *gc = GetGraphicsContext();
        if (!gc)
            return LogStageFail("RenderToTextureApp::CreateCube", "graphics context is null");

        auto *sm = GetManager<SamplerManager>();
        auto *tm = GetManager<TextureManager>();
        auto *gm = GetManager<GeometryManager>();
        auto *device = gc->GetDevice();
        if (!sm || !tm || !gm || !device)
            return LogStageFail("RenderToTextureApp::CreateCube", "required managers/device missing");

        cube_sampler = sm->CreateSampler();
        if (!cube_sampler)
            return LogStageFail("RenderToTextureApp::CreateCube", "CreateSampler failed");

        base_tex = offscreen ? offscreen->GetColorTexture() : nullptr;
        if (!base_tex)
        {
            LogStage("RenderToTextureApp::CreateCube", "offscreen texture unavailable, loading fallback albedo");
            fallback_albedo = tm->LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"), true);
            base_tex = fallback_albedo;
        }

        normal_tex = tm->LoadTexture2D(OS_TEXT("res/image/Brickwall/Normal.Tex2D"), true);
        roughness_tex = tm->LoadTexture2D(OS_TEXT("res/image/Brickwall/Roughness.Tex2D"), true);

        if (!base_tex || !normal_tex || !roughness_tex)
            return LogStageFail("RenderToTextureApp::CreateCube", "required textures missing");

        LogTextureInfo("onscreen_bind_basecolor", base_tex);

        cube_mi_data.base_color = 0xFFFFFFFFu;
        cube_mi_data.metallic = 0.08f;
        cube_mi_data.roughness = 0.92f;
        cube_mi_data.normal_scale = 0.35f;

        if (!InitCubeMISSBO())
            return LogStageFail("RenderToTextureApp::CreateCube", "InitCubeMISSBO failed");

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateStandardGeometryVertexFormat());
        inline_geometry::CubeCreateInfo cci{};
        cci.tex_coord = true;
        cci.normal = true;

        Geometry *cube_geometry = inline_geometry::CreateCube(pc.get(), &cci);
        if (!cube_geometry)
            return LogStageFail("RenderToTextureApp::CreateCube", "CreateCube geometry failed");

        gm->Add(cube_geometry);

        cube_recipe.recipe_name = "RenderToTexture.Cube";
        cube_recipe.mtl_def_id = "Lit";
        cube_recipe.pipeline_preset = PipelinePreset::Solid3D;
        cube_recipe.domain = "RenderToTexture.MainScene";
        cube_asset = PrimitiveAsset(cube_geometry, &cube_recipe, PrimitiveType::Triangles);
        if (!cube_asset.IsValid())
            return LogStageFail("RenderToTextureApp::CreateCube", "create cube primitive asset failed");

        cube_entity = ecs_context->CreateEntity<Entity>("RTTCube");
        cube_transform = cube_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto cube_prim_comp = cube_entity->AddComponent<PrimitiveComponent>();

        cube_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        cube_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        cube_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        cube_transform->SetMovable(true);

        cube_prim_comp->SetPrimitiveAsset(&cube_asset);
        cube_prim_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::BaseColor, base_tex, cube_sampler);
        cube_prim_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::Normal, normal_tex, cube_sampler);
        cube_prim_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::Roughness, roughness_tex, cube_sampler);
        hgl::ecs::PrimitiveComponent::MaterialSSBONamedAuthoringResource cube_struct{};
        cube_struct.ssbo_name = graph::mtl::DefaultMaterialSSBOName;
        cube_struct.ssbo_id = cube_mi_ssbo_accessor->GetSSBOId();
        cube_struct.ssbo_element_index = 0;
        cube_struct.use_ssbo_element_index = true;
        cube_struct.shared_across_instances = true;
        cube_prim_comp->SetMaterialSSBOResource(cube_struct);
        cube_prim_comp->SetVisible(true);
        LogStage("RenderToTextureApp::CreateCube", "success");
        return true;
    }
    bool InitCubeMISSBO()
    {
        GLogInfo("[RenderToTexture][RenderToTextureApp::InitCubeMISSBO] begin ecs=%p",
                 (void *)ecs_context);
        if (!ecs_context)
            return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "invalid input pointers");

        auto *domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "resource domain manager is null");

        cube_mi_ssbo_accessor = domain_manager->AllocateArrayAccessor<ssbo::StandardMaterialInstance>(
            graph::mtl::SSBOType::ClearCoatSurface,
            "RenderToTexture:MainScene:MIData",
            1);
        if (!cube_mi_ssbo_accessor)
            return LogStageFail("RenderToTextureApp::InitCubeMISSBO", "CreateSSBO failed");

        (*cube_mi_ssbo_accessor)[0] = cube_mi_data;
        cube_mi_ssbo_accessor->Commit();

        LogStage("RenderToTextureApp::InitCubeMISSBO", "success");
        return true;
    }
public:
    ~RenderToTextureApp() override
    {
        SAFE_CLEAR(offscreen)

        if (auto *gc = GetGraphicsContext())
        {
            if (cube_sampler)
            {
                if (auto *sm = gc->GetManager<SamplerManager>())
                    sm->Release(cube_sampler);
            }
        }

        SAFE_CLEAR(cube_mi_ssbo_accessor)
        cube_sampler = nullptr;
        base_tex = nullptr;
        fallback_albedo = nullptr;
        normal_tex = nullptr;
        roughness_tex = nullptr;
    }

    bool Init() override
    {
        LogStage("RenderToTextureApp::Init", "begin");
        ecs_context = GetECSContext();
        if (!ecs_context)
            return LogStageFail("RenderToTextureApp::Init", "ECS context is null");

        ecs_context->SetResourceNamePrefix("RenderToTexture:MainScene");

        auto environment_system = ecs_context->GetSystem<EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterRenderSystem<EnvironmentSystem>();

        if (environment_system)
        {
            environment_system->EditSkyInfo();
            environment_system->SyncSkyUBO();
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

    void Render(double delta_time) override
    {
        if (cube_transform)
        {
            cube_theta += static_cast<float>(delta_time) * 0.8f;
            cube_theta = fmodf(cube_theta, 2.0f * std::numbers::pi_v<float>);

            const glm::quat rot_z = glm::angleAxis(cube_theta, glm::vec3(0.0f, 0.0f, 1.0f));
            const glm::quat rot_x = glm::angleAxis(cube_theta * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
            cube_transform->SetLocalRotation(rot_z * rot_x);
        }

        WorkObject::Render(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<RenderToTextureApp>(OS_TEXT("Render To Texture (ECS)"), argc, argv, 1280, 720);
}
