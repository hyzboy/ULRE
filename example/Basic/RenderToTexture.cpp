#include<hgl/WorkManager.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetSingle.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/color/Color.h>
#include<hgl/log/Log.h>
#include <memory>

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
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/RenderSystemCore.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
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
    ECSContext *      ecs_world   = nullptr;
    std::unique_ptr<hgl::ecs::RenderSystemCore> render_core;

    RenderContext *   render_context = nullptr;

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
        render_core.reset();

        if(ecs_world)
        {
            ecs_world->Shutdown();
            delete ecs_world;
        }

        if (render_context)
        {
            auto *graphics_context = render_context->GetGraphicsContext();
            if (!graphics_context && ecs_world)
                graphics_context = ecs_world->GetGraphicsContext();

            if (primitive)
            {
                if (auto *primitive_manager = graphics_context ? graphics_context->GetPrimitiveManager() : nullptr)
                    primitive_manager->Release(primitive);
            }

            if (geometry)
            {
                if (auto *geometry_manager = graphics_context ? graphics_context->GetGeometryManager() : nullptr)
                    geometry_manager->Release(geometry);
            }

            if (auto *material_manager = graphics_context ? graphics_context->GetMaterialManager() : nullptr)
            {
                if (mi)
                    material_manager->Destroy(mi);
                if (mtl)
                    material_manager->Destroy(mtl);
            }
        }

        delete rt;

        primitive = nullptr;
        geometry = nullptr;
        mi = nullptr;
        mtl = nullptr;
        pipeline = nullptr;
        rt = nullptr;
    }

    bool Init(WorkObject *owner, uint32_t w, uint32_t h)
    {
        if(!owner) return false;
        auto *ecs = owner->GetECSContext();
        if (!ecs) return false;

        auto *graphics = ecs->GetGraphicsContext();
        if (!graphics) return false;

        auto *device = graphics->GetDevice();
        if (!device) return false;

        const VulkanDevAttr *dev_attr = device->GetDevAttr();
        if (!dev_attr) return false;

        const VkFormat color_fmt = dev_attr->surface_format.format;
        const VkFormat depth_fmt = dev_attr->physical_device->GetDepthFormat();

        FramebufferInfo fbi(color_fmt, depth_fmt);
        fbi.SetExtent(w, h);

        rt = RenderTargetManager::CreateRTFromGraphicsContext(graphics, ecs, &fbi);
        if (!rt) return false;

        LogTextureInfo("offscreen_rt_color0_init", rt->GetColorTexture(0));

        ecs_world = new ECSContext("OffscreenECSWorld");
        if (!ecs_world) return false;

        ecs_world->SetRenderContext(owner->GetRenderContext());
        ecs_world->InitializeGraphics(owner->GetDevice(), rt);

        auto render_target_system = ecs_world->RegisterRenderSystem<RenderTargetSystem>();
        auto render_collect_system = ecs_world->RegisterTickSystem<RenderPrimitiveCollectSystem>();
        auto render_batch_system = ecs_world->RegisterTickSystem<RenderPrimitiveBatchSystem>();
        auto render_submit_system = ecs_world->RegisterRenderSystem<RenderPrimitiveSubmitSystem>();
        ecs_world->RegisterTickSystem<InputSystem>();
        auto camera_system = ecs_world->RegisterTickSystem<CameraSystem>(ecs_world);

        render_target_system->SetRenderContext(owner->GetRenderContext());
        render_target_system->SetRenderTarget(rt);

        render_collect_system->SetWorld(ecs_world);

        render_batch_system->SetWorld(ecs_world);
        render_batch_system->SetDevice(device);

        render_submit_system->SetWorld(ecs_world);

        ecs_world->Initialize();

        if (camera_system)
        {
            camera_system->SetRenderContext(owner->GetRenderContext());
            camera_system->SetViewportInfo(rt->GetViewportInfo());
        }

        const auto *camera_info = camera_system ? camera_system->GetCameraInfo() : nullptr;
        render_collect_system->SetCameraInfo(camera_info);
        render_batch_system->SetCameraInfo(camera_info);

        render_core = std::make_unique<hgl::ecs::RenderSystemCore>(ecs_world);
        if (!render_core || !render_core->Initialize())
            return false;

        return true;
    }

    bool BuildSphere(WorkObject *owner)
    {
        if(!owner || !ecs_world) return false;

        auto* render_context = owner->GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        this->render_context = render_context;

        auto* device = graphics_context->GetDevice();
        if (!device)
            return false;

        mtl::Material3DCreateConfig cfg3d(PrimitiveType::Triangles,
                                          mtl::WithCamera::With,
                                          mtl::WithLocalToWorld::With,
                                          mtl::WithSky::Without);

        mtl::MaterialCreateInfo *mci = mtl::CreateGizmo3D(device->GetDevAttr(), &cfg3d);
        if (!mci) return false;

        mtl = material_manager->CreateMaterial("OffscreenPureColor3D", mci);
        if (!mtl) return false;

        auto* render_pass = rt ? rt->GetRenderPass() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(mtl, InlinePipeline::Solid3D) : nullptr;
        if (!pipeline) return false;

        Color4f sphere_color = GetColor4f(COLOR::SkyBlue, 1.0f);
        mi = material_manager->CreateMaterialInstance(mtl, (VIL*)nullptr, &sphere_color);
        if (!mi) return false;

        auto pc = std::make_unique<GeometryCreater>(device, mtl->GetDefaultVIL());

        geometry = inline_geometry::CreateSphere(pc.get(), 64);
        if (!geometry) return false;

        auto* geometry_manager = graphics_context->GetGeometryManager();
        if (!geometry_manager) return false;
        geometry_manager->Add(geometry);

        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!primitive_manager) return false;

        primitive = primitive_manager->CreatePrimitive(geometry, mi, pipeline);
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

        auto camera_system = ecs_world->GetSystem<CameraSystem>();
        camera->camera_data = camera_system ? camera_system->GetCamera() : nullptr;
        camera->camera_info = const_cast<graph::CameraInfo *>(camera_system ? camera_system->GetCameraInfo() : nullptr);
        camera->viewport_info = camera_system ? camera_system->GetViewportInfo() : nullptr;

        return true;
    }

    bool RenderOnce()
    {
        if(!render_core || !ecs_world) return false;

        LogTextureInfo("offscreen_before_render", rt ? rt->GetColorTexture(0) : nullptr);
        ecs_world->Tick(0.0f);

        render_core->SetClearColor(GetColor4f(COLOR::DarkSlateBlue, 1.0f));
        if (!render_core->BeginFrame())
            return false;

        ecs_world->Render(render_core->GetRenderCmd(), 0.0f);
        render_core->EndFrame();

        const bool ok = true;
        LogTextureInfo("offscreen_after_render", rt ? rt->GetColorTexture(0) : nullptr);
        return ok;
    }
};

class RenderToTextureApp final: public WorkObject
{
private:
    OffscreenSceneECS *      offscreen          = nullptr;

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
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        auto* sampler_manager = graphics_context->GetSamplerManager();
        if (!material_manager || !sampler_manager)
            return false;

        auto* device = graphics_context->GetDevice();
        if (!device)
            return false;

        mtl::Material3DCreateConfig cfg3d(PrimitiveType::Triangles,
                                          mtl::WithCamera::With,
                                          mtl::WithLocalToWorld::With,
                                          mtl::WithSky::With);

        mtl::MaterialCreateInfo *mci = mtl::CreateTextureBlinnPhong(device->GetDevAttr(), &cfg3d);
        if (!mci) return false;

        cube_mtl = material_manager->CreateMaterial("OnscreenCube", mci);
        if (!cube_mtl) return false;

        cube_sampler = sampler_manager->CreateSampler();

        cube_mtl->BindTextureSampler(DescriptorSetType::PerMaterial,
                                     mtl::SamplerName::BaseColor,
                                     offscreen && offscreen->rt ? offscreen->rt->GetColorTexture(0) : nullptr,
                                     cube_sampler);

        LogTextureInfo("onscreen_bind_basecolor", offscreen && offscreen->rt ? offscreen->rt->GetColorTexture(0) : nullptr);

        auto* render_target = render_context->GetCurrentRenderTarget();
        if (!render_target && ecs_world)
            render_target = ecs_world->GetRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        cube_pipeline = render_pass ? render_pass->CreatePipeline(cube_mtl, InlinePipeline::Solid3D) : nullptr;
        if (!cube_pipeline) return false;

        cube_mi = material_manager->CreateMaterialInstance(cube_mtl);
        if (!cube_mi) return false;

        auto pc = std::make_unique<GeometryCreater>(device, cube_mtl->GetDefaultVIL());

        inline_geometry::CubeCreateInfo cci_cube{};
        cci_cube.tex_coord = true;
        cci_cube.normal = true;

        Geometry *geometry = inline_geometry::CreateCube(pc.get(), &cci_cube);
        if (!geometry) return false;

        auto* geometry_manager = graphics_context->GetGeometryManager();
        if (!geometry_manager) return false;
        geometry_manager->Add(geometry);

        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!primitive_manager) return false;

        cube_primitive = primitive_manager->CreatePrimitive(geometry, cube_mi, cube_pipeline);
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
        SAFE_CLEAR(offscreen);

        if (auto *render_context = GetRenderContext())
        {
            auto *graphics_context = render_context->GetGraphicsContext();

            if (cube_primitive)
            {
                if (auto *primitive_manager = graphics_context ? graphics_context->GetPrimitiveManager() : nullptr)
                    primitive_manager->Release(cube_primitive);
            }

            if (auto *material_manager = graphics_context ? graphics_context->GetMaterialManager() : nullptr)
            {
                if (cube_mi)
                    material_manager->Destroy(cube_mi);
                if (cube_mtl)
                    material_manager->Destroy(cube_mtl);
            }

            if (cube_sampler)
            {
                if (auto *sampler_manager = graphics_context ? graphics_context->GetSamplerManager() : nullptr)
                    sampler_manager->Release(cube_sampler);
            }
        }

        cube_primitive = nullptr;
        cube_mi = nullptr;
        cube_mtl = nullptr;
        cube_sampler = nullptr;
        cube_pipeline = nullptr;
    }

    bool Init() override
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        auto environment_system = ecs_world->GetSystem<EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_world->RegisterRenderSystem<EnvironmentSystem>();

        if (environment_system)
        {
            environment_system->EditSkyInfo();
            environment_system->SyncSkyUBO();
        }

        if (!CreateOffscreenRT(512, 512))
            return false;

        if (!CreateRotatingCube())
            return false;

        if (!SetupMainCamera())
            return false;

        return true;
    }

    void Render(double delta_time) override
    {
        if (offscreen)
        {
            if (offscreen->RenderOnce())
            {
                LogTextureInfo("onscreen_after_offscreen_render",
                               offscreen->rt ? offscreen->rt->GetColorTexture(0) : nullptr);
            }
        }

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

