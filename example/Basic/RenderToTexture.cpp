#include<hgl/WorkManager.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKRenderTargetSingle.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/GeometryCreater.h>
#include<hgl/graph/SceneCaptureStage.h>
#include<hgl/graph/SceneRenderer.h>
#include<hgl/color/Color.h>
#include<hgl/log/Log.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveBatchSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveSubmitSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
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
}

class OffscreenSceneECS
{
public:
    IRenderTarget *   rt          = nullptr;
    SceneRenderer *   renderer    = nullptr;
    ECSContext *      ecs_world   = nullptr;

    Material *        mtl         = nullptr;
    MaterialInstance* mi          = nullptr;
    Pipeline *        pipeline    = nullptr;

    Geometry *        geometry    = nullptr;
    Primitive *       primitive   = nullptr;

    Entity *          sphere_entity = nullptr;
    Entity *          camera_entity = nullptr;

public:
    ~OffscreenSceneECS()
    {
        if(ecs_world)
        {
            ecs_world->Shutdown();
            delete ecs_world;
        }
        SAFE_CLEAR(renderer);
        rt = nullptr; // managed by manager

        delete primitive;
        delete geometry;
    }

    bool Init(WorkObject *owner, uint32_t w, uint32_t h)
    {
        if(!owner) return false;
        auto rf = owner->GetRenderFramework();
        if (!rf) return false;

        const VulkanDevAttr *dev_attr = rf->GetDevAttr();
        if (!dev_attr) return false;

        const VkFormat color_fmt = dev_attr->surface_format.format;
        const VkFormat depth_fmt = dev_attr->physical_device->GetDepthFormat();

        FramebufferInfo fbi(color_fmt, depth_fmt);
        fbi.SetExtent(w, h);

        rt = rf->GetRenderTargetManager()->CreateRT(&fbi);
        if (!rt) return false;

        LogTextureInfo("offscreen_rt_color0_init", rt->GetColorTexture(0));

        renderer = new SceneRenderer(rf, rt);
        if(!renderer) return false;
        renderer->SetClearColor(GetColor4f(COLOR::DarkSlateBlue, 1.0f));

        ecs_world = new ECSContext("OffscreenECSWorld");
        renderer->SetECSContext(ecs_world);

        auto render_collect_system = ecs_world->RegisterTickSystem<RenderPrimitiveCollectSystem>();
        auto render_batch_system = ecs_world->RegisterTickSystem<RenderPrimitiveBatchSystem>();
        auto render_submit_system = ecs_world->RegisterRenderSystem<RenderPrimitiveSubmitSystem>();
        auto input_system = ecs_world->RegisterTickSystem<InputSystem>();
        auto camera_system = ecs_world->RegisterTickSystem<CameraSystem>(ecs_world);

        render_collect_system->SetWorld(ecs_world);
        render_collect_system->SetCameraInfo(renderer->GetCameraInfo());

        render_batch_system->SetWorld(ecs_world);
        render_batch_system->SetDevice(rf->GetDevice());
        render_batch_system->SetCameraInfo(renderer->GetCameraInfo());

        render_submit_system->SetWorld(ecs_world);

        ecs_world->Initialize();

        return true;
    }

    bool BuildSphere(WorkObject *owner)
    {
        if(!owner || !renderer || !ecs_world) return false;

        mtl::Material3DCreateConfig cfg3d(PrimitiveType::Triangles,
                                          mtl::WithCamera::With,
                                          mtl::WithLocalToWorld::With,
                                          mtl::WithSky::Without);

        mtl::MaterialCreateInfo *mci = mtl::CreateGizmo3D(owner->GetDevAttr(), &cfg3d);
        if (!mci) return false;

        mtl = owner->CreateMaterial("OffscreenPureColor3D", mci);
        if (!mtl) return false;

        pipeline = renderer->CreatePipeline(mtl, InlinePipeline::Solid3D);
        if (!pipeline) return false;

        Color4f sphere_color = GetColor4f(COLOR::SkyBlue, 1.0f);
        mi = owner->CreateMaterialInstance(mtl, (VIL*)nullptr, &sphere_color);
        if (!mi) return false;

        auto pc = owner->GetGeometryCreater(mtl);
        if (!pc) return false;

        geometry = inline_geometry::CreateSphere(pc.get(), 64);
        if (!geometry) return false;

        owner->Add(geometry);

        primitive = owner->CreatePrimitive(geometry, mi, pipeline);
        if (!primitive) return false;

        sphere_entity = ecs_world->CreateEntity<Entity>("OffscreenSphere");
        auto transform = sphere_entity->AddComponent<TransformComponent>();
        auto prim_comp = sphere_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitive(primitive);
        prim_comp->SetVisible(true);

        camera_entity = ecs_world->CreateEntity<Entity>("OffscreenCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0,0,0);
        camera->distance = 6.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = renderer->GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo *>(renderer->GetCameraInfo());
        camera->viewport_info = renderer->GetViewportInfo();

        return true;
    }

    bool RenderOnce()
    {
        if(!renderer || !ecs_world) return false;

        LogTextureInfo("offscreen_before_render", rt ? rt->GetColorTexture(0) : nullptr);
        renderer->Tick(0.0);
        const bool ok = renderer->RenderSubmitAndWait();
        LogTextureInfo("offscreen_after_render", rt ? rt->GetColorTexture(0) : nullptr);
        return ok;
    }
};

class RenderToTextureApp final: public WorkObject
{
private:
    OffscreenSceneECS *      offscreen          = nullptr;
    SceneCaptureStage *      offscreen_stage    = nullptr;

    ECSContext *             ecs_world          = nullptr;
    Entity *                 cube_entity        = nullptr;
    Entity *                 camera_entity      = nullptr;

    Material *               cube_mtl           = nullptr;
    MaterialInstance *       cube_mi            = nullptr;
    Pipeline *               cube_pipeline      = nullptr;
    Sampler *                cube_sampler       = nullptr;
    Primitive *              cube_primitive     = nullptr;

    std::shared_ptr<TransformComponent> cube_transform;

    float                    cube_theta          = 0.0f;
    bool InstallOffscreenStage()
    {
        SceneRenderer *renderer = GetSceneRenderer();
        if(!renderer || !offscreen)
            return false;

        renderer->EnsureEcsPipeline();

        offscreen_stage = new SceneCaptureStage(
            [this](RenderStageContext &)->bool
            {
                if(!offscreen)
                    return false;

                if(offscreen->RenderOnce())
                {
                    LogTextureInfo("onscreen_after_offscreen_render",
                                   offscreen->rt ? offscreen->rt->GetColorTexture(0) : nullptr);
                    return true;
                }

                return false;
            },
            true,
            SceneCaptureStage::CaptureTarget::Texture2D);
        if(!renderer->GetEcsPipeline().InsertStageBefore(offscreen_stage, "BeginFrame"))
        {
            delete offscreen_stage;
            offscreen_stage = nullptr;
            return false;
        }

        return true;
    }

    void CleanupOffscreenStage()
    {
        SceneRenderer *renderer = GetSceneRenderer();
        if(renderer && offscreen_stage)
            renderer->GetEcsPipeline().RemoveStage(offscreen_stage);

        delete offscreen_stage;
        offscreen_stage = nullptr;
    }

private:
    bool EnsureCameraSystem()
    {
        if(!ecs_world)
            return false;

        auto camera_system = ecs_world->GetSystem<CameraSystem>();
        if(!camera_system)
        {
            camera_system = ecs_world->RegisterTickSystem<CameraSystem>(ecs_world);
            if(ecs_world->IsActive())
            {
                camera_system->OnDependenciesReady();
                camera_system->Initialize();
            }
        }

        return camera_system != nullptr;
    }

    bool SetupMainCamera()
    {
        if(!EnsureCameraSystem())
            return false;

        camera_entity = ecs_world->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0,0,0);
        camera->distance = 5.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

    bool CreateOffscreenRT(uint32_t w, uint32_t h)
    {
        offscreen = new OffscreenSceneECS();
        if(!offscreen) return false;

        if(!offscreen->Init(this, w, h))
            return false;

        if(!offscreen->BuildSphere(this))
            return false;

        return true;
    }

    bool CreateRotatingCube()
    {
        mtl::Material3DCreateConfig cfg3d(PrimitiveType::Triangles,
                                          mtl::WithCamera::With,
                                          mtl::WithLocalToWorld::With,
                                          mtl::WithSky::With);

        mtl::MaterialCreateInfo *mci = mtl::CreateTextureBlinnPhong(GetDevAttr(), &cfg3d);
        if (!mci) return false;

        cube_mtl = CreateMaterial("OnscreenCube", mci);
        if (!cube_mtl) return false;

        cube_sampler = CreateSampler();

        cube_mtl->BindTextureSampler(DescriptorSetType::PerMaterial,
                                     mtl::SamplerName::BaseColor,
                                     offscreen && offscreen->rt ? offscreen->rt->GetColorTexture(0) : nullptr,
                                     cube_sampler);

        LogTextureInfo("onscreen_bind_basecolor", offscreen && offscreen->rt ? offscreen->rt->GetColorTexture(0) : nullptr);

        cube_pipeline = CreatePipeline(cube_mtl, InlinePipeline::Solid3D);
        if (!cube_pipeline) return false;

        cube_mi = CreateMaterialInstance(cube_mtl);
        if (!cube_mi) return false;

        auto pc = GetGeometryCreater(cube_mtl);
        if (!pc) return false;

        inline_geometry::CubeCreateInfo cci_cube{};
        cci_cube.tex_coord = true;
        cci_cube.normal = true;

        Geometry *geometry = inline_geometry::CreateCube(pc.get(), &cci_cube);
        if (!geometry) return false;

        Add(geometry);

        cube_primitive = CreatePrimitive(geometry, cube_mi, cube_pipeline);
        if (!cube_primitive) return false;

        cube_entity = ecs_world->CreateEntity<Entity>("RotatingCube");
        cube_transform = cube_entity->AddComponent<TransformComponent>();
        auto cube_prim_comp = cube_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        cube_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        cube_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        cube_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        cube_transform->SetMovable(true);

        cube_prim_comp->SetPrimitive(cube_primitive);
        cube_prim_comp->SetVisible(true);

        return true;
    }

public:
    using WorkObject::WorkObject;

    ~RenderToTextureApp() override
    {
        CleanupOffscreenStage();
        SAFE_CLEAR(offscreen);
        delete cube_primitive;
    }

    bool Init() override
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        if (!CreateOffscreenRT(512, 512))
            return false;

        if (!CreateRotatingCube())
            return false;

        if (!SetupMainCamera())
            return false;

        if (!InstallOffscreenStage())
            return false;

        return true;
    }

    void Render(double delta_time) override
    {
        if (cube_transform)
        {
            cube_theta += float(delta_time) * 0.8f;
            cube_theta = fmodf(cube_theta, 2.0f * std::numbers::pi_v<float>);

            glm::quat rot_z = glm::angleAxis(cube_theta, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::quat rot_x = glm::angleAxis(cube_theta * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
            glm::quat rotation = rot_z * rot_x;

            cube_transform->SetLocalRotation(rotation);
        }

        WorkObject::Render(delta_time);
    }
};

int os_main(int, os_char **)
{
    return RunFramework<RenderToTextureApp>(OS_TEXT("Render To Texture (ECS)"), 1280, 720);
}

