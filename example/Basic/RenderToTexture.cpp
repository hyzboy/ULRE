#include<hgl/framework/WorkManager.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetSingle.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/color/Color.h>
#include<hgl/log/Log.h>

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
#include<hgl/ecs/systems/tick/InputSystem.h>

#include <memory>
#include <numbers>

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

class OffscreenPass
{
private:
    hgl::example::OffscreenWorldRuntime runtime;

    RenderContext *render_context = nullptr;

    Material *mtl = nullptr;
    MaterialInstance *mi = nullptr;
    Pipeline *pipeline = nullptr;
    Geometry *geometry = nullptr;
    Primitive *primitive = nullptr;

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
            if (primitive)
            {
                if (auto *pm = gc->GetPrimitiveManager())
                    pm->Release(primitive);
            }

            if (geometry)
            {
                if (auto *gm = gc->GetGeometryManager())
                    gm->Release(geometry);
            }

            if (auto *mm = gc->GetMaterialManager())
            {
                if (mi)
                    mm->Destroy(mi);
                if (mtl)
                    mm->Destroy(mtl);
            }
        }

        primitive = nullptr;
        geometry = nullptr;
        mi = nullptr;
        mtl = nullptr;
        pipeline = nullptr;
    }

    Texture2D *GetColorTexture() const
    {
        return runtime.GetColorTexture(0);
    }

    bool Init(WorkObject *owner, const uint32_t width, const uint32_t height)
    {
        hgl::example::OffscreenWorldConfig cfg;
        cfg.width = width;
        cfg.height = height;
        cfg.world_name = "RenderToTexture_Offscreen";
        cfg.resource_prefix = "RenderToTexture:OffscreenRT";
        cfg.register_input_system = false;

        if (!runtime.Init(owner, cfg))
            return false;

        LogTextureInfo("offscreen_rt_color0_init", runtime.GetColorTexture(0));
        render_context = owner->GetRenderContext();
        return true;
    }

    bool BuildSphere(WorkObject *owner)
    {
        if (!owner || !runtime.GetWorld() || !runtime.GetRenderTarget())
            return false;

        RenderContext *rc = owner->GetRenderContext();
        if (!rc)
            return false;

        GraphicsContext *gc = rc->GetGraphicsContext();
        if (!gc)
            return false;

        auto *mm = gc->GetMaterialManager();
        auto *gm = gc->GetGeometryManager();
        auto *pm = gc->GetPrimitiveManager();
        auto *device = gc->GetDevice();
        if (!mm || !gm || !pm || !device)
            return false;

        mtl::Material3DCreateConfig cfg3d(PrimitiveType::Triangles,
                                          mtl::WithCamera::With,
                                          mtl::WithLocalToWorld::With,
                                          mtl::WithSky::Without);

        mtl = mm->CreateMaterial(mtl::MaterialPreset::Gizmo3D, &cfg3d);
        if (!mtl)
            return false;

        auto *rp = runtime.GetRenderTarget()->GetRenderPass();
        pipeline = rp ? rp->CreatePipeline(mtl, InlinePipeline::Solid3D) : nullptr;
        if (!pipeline)
            return false;

        Color4f sphere_color = GetColor4f(COLOR::SkyBlue, 1.0f);
        mi = mm->CreateMaterialInstance(mtl, (VIL*)nullptr, &sphere_color);
        if (!mi)
            return false;

        auto pc = std::make_unique<GeometryCreater>(device, mtl->GetDefaultVIL());
        geometry = inline_geometry::CreateSphere(pc.get(), 64);
        if (!geometry)
            return false;

        gm->Add(geometry);

        primitive = pm->CreatePrimitive(geometry, mi, pipeline);
        if (!primitive)
            return false;

        auto *world = runtime.GetWorld();
        Entity *sphere = world->CreateEntity<Entity>("OffscreenSphere");
        auto transform = sphere->AddComponent<TransformComponent>(Mobility::Static);
        auto prim_comp = sphere->AddComponent<PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitive(primitive);
        prim_comp->SetVisible(true);

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

        return true;
    }

    bool RenderOnce()
    {
        return runtime.RenderOnce(GetColor4f(COLOR::DarkSlateBlue, 1.0f));
    }
};

class RenderToTextureApp final: public WorkObject
{
private:
    OffscreenPass *offscreen = nullptr;

    ECSContext *ecs_context = nullptr;
    Entity *camera_entity = nullptr;
    Entity *cube_entity = nullptr;

    Material *cube_mtl = nullptr;
    MaterialInstance *cube_mi = nullptr;
    Pipeline *cube_pipeline = nullptr;
    Sampler *cube_sampler = nullptr;
    Primitive *cube_primitive = nullptr;

    Texture2D *fallback_albedo = nullptr;
    Texture2D *normal_tex = nullptr;
    Texture2D *roughness_tex = nullptr;

    std::shared_ptr<TransformComponent> cube_transform;
    float cube_theta = 0.0f;

private:
    bool SetupMainCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

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
        return true;
    }

    bool CreateOffscreenRT()
    {
        offscreen = new OffscreenPass();
        if (!offscreen)
            return false;

        if (!offscreen->Init(this, 512, 512))
            return false;

        if (!offscreen->BuildSphere(this))
            return false;

        // This sample's offscreen content is static. Render once at startup to
        // avoid cross-pass write/read jitter on the same texture each frame.
        if (!offscreen->RenderOnce())
            return false;

        return true;
    }

    bool CreateCube()
    {
        auto *rc = GetRenderContext();
        if (!rc)
            return false;

        auto *gc = rc->GetGraphicsContext();
        if (!gc)
            return false;

        auto *mm = gc->GetMaterialManager();
        auto *sm = gc->GetSamplerManager();
        auto *tm = gc->GetTextureManager();
        auto *gm = gc->GetGeometryManager();
        auto *pm = gc->GetPrimitiveManager();
        auto *device = gc->GetDevice();
        if (!mm || !sm || !tm || !gm || !pm || !device)
            return false;

        mtl::Material3DCreateConfig cfg3d(PrimitiveType::Triangles,
                                          mtl::WithCamera::With,
                                          mtl::WithLocalToWorld::With,
                                          mtl::WithSky::With);

        cube_mtl = mm->CreateMaterial(mtl::MaterialPreset::Standard, &cfg3d);
        if (!cube_mtl)
            return false;

        cube_sampler = sm->CreateSampler();
        if (!cube_sampler)
            return false;

        Texture2D *base_tex = offscreen ? offscreen->GetColorTexture() : nullptr;
        if (!base_tex)
        {
            fallback_albedo = tm->LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"), true);
            base_tex = fallback_albedo;
        }

        normal_tex = tm->LoadTexture2D(OS_TEXT("res/image/Brickwall/Normal.Tex2D"), true);
        roughness_tex = tm->LoadTexture2D(OS_TEXT("res/image/Brickwall/Roughness.Tex2D"), true);

        if (!base_tex || !normal_tex || !roughness_tex)
            return false;

        if (!cube_mtl->BindTextureSampler(mtl::SamplerName::SamplerSlot::BaseColor,
                                          base_tex,
                                          cube_sampler))
            return false;

        if (!cube_mtl->BindTextureSampler(mtl::SamplerName::SamplerSlot::Normal,
                                          normal_tex,
                                          cube_sampler))
            return false;

        if (!cube_mtl->BindTextureSampler(mtl::SamplerName::SamplerSlot::Roughness,
                                          roughness_tex,
                                          cube_sampler))
            return false;

        LogTextureInfo("onscreen_bind_basecolor", base_tex);

        auto *render_target = rc->GetCurrentRenderTarget();
        if (!render_target && ecs_context)
            render_target = ecs_context->GetRenderTarget();

        auto *rp = render_target ? render_target->GetRenderPass() : nullptr;
        cube_pipeline = rp ? rp->CreatePipeline(cube_mtl, InlinePipeline::Solid3D) : nullptr;
        if (!cube_pipeline)
            return false;

        mtl::StandardMaterialInstance cube_mi_data{};
        cube_mi_data.base_color = 0xFFFFFFFFu;
        cube_mi_data.metallic = 0.08f;
        cube_mi_data.roughness = 0.92f;
        cube_mi_data.normal_scale = 0.35f;

        cube_mi = mm->CreateMaterialInstance(cube_mtl, (VIL*)nullptr, &cube_mi_data);
        if (!cube_mi)
            return false;

        auto pc = std::make_unique<GeometryCreater>(device, cube_mtl->GetDefaultVIL());
        inline_geometry::CubeCreateInfo cci{};
        cci.tex_coord = true;
        cci.normal = true;

        Geometry *cube_geometry = inline_geometry::CreateCube(pc.get(), &cci);
        if (!cube_geometry)
            return false;

        gm->Add(cube_geometry);

        cube_primitive = pm->CreatePrimitive(cube_geometry, cube_mi, cube_pipeline);
        if (!cube_primitive)
            return false;

        cube_entity = ecs_context->CreateEntity<Entity>("RTTCube");
        cube_transform = cube_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto cube_prim_comp = cube_entity->AddComponent<PrimitiveComponent>();

        cube_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        cube_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        cube_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        cube_transform->SetMovable(true);

        cube_prim_comp->SetPrimitive(cube_primitive);
        cube_prim_comp->SetVisible(true);
        return true;
    }

public:
    ~RenderToTextureApp() override
    {
        SAFE_CLEAR(offscreen)

        if (auto *rc = GetRenderContext())
        {
            auto *gc = rc->GetGraphicsContext();

            if (cube_primitive)
            {
                if (auto *pm = gc ? gc->GetPrimitiveManager() : nullptr)
                    pm->Release(cube_primitive);
            }

            if (auto *mm = gc ? gc->GetMaterialManager() : nullptr)
            {
                if (cube_mi)
                    mm->Destroy(cube_mi);
                if (cube_mtl)
                    mm->Destroy(cube_mtl);
            }

            if (cube_sampler)
            {
                if (auto *sm = gc ? gc->GetSamplerManager() : nullptr)
                    sm->Release(cube_sampler);
            }
        }

        cube_primitive = nullptr;
        cube_mi = nullptr;
        cube_mtl = nullptr;
        cube_sampler = nullptr;
        cube_pipeline = nullptr;
        fallback_albedo = nullptr;
        normal_tex = nullptr;
        roughness_tex = nullptr;
    }

    bool Init() override
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

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
            return false;

        if (!CreateCube())
            return false;

        if (!SetupMainCamera())
            return false;

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

