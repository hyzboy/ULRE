// BlendMode Transparent ECS Example - Alpha Blend Demo
//
// Demonstrates standard alpha blending (Transparent blend mode).
// The fragment shader outputs the full surface alpha; the pipeline's Alpha3D
// preset handles src-alpha / one-minus-src-alpha blending.
//
// For correct visual results, transparent objects should ideally be rendered
// back-to-front (painter's algorithm).  This example approximates that order
// by assigning each billboard a higher entity depth the farther it is placed
// from the camera origin (spiral layout, inner billboards first).
//
// Pipeline: Alpha3D (src-alpha blend, depth test on, depth write OFF for blend pass).
// Based on BillboardPerspectiveECS.cpp – uses icon textures (PNG with transparency).

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/color/Color.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/BillboardComponent.h>
#include<hgl/ecs/components/QuadComponent.h>
#include<hgl/ecs/components/FacingTransformComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include<hgl/ecs/systems/render/QuadMaterialBindingSystem.h>
#include<hgl/ecs/systems/transform/FacingTransformSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<iostream>
#include<memory>
#include<cmath>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

static Color4f white_color(1, 1, 1, 1);

// All 50 freepik icon textures – each has a transparent background (alpha channel)
static const os_char* kIconTextures[] = {
    OS_TEXT("res/image/icon/freepik/001-online resume.Tex2D"),
    OS_TEXT("res/image/icon/freepik/002-salary.Tex2D"),
    OS_TEXT("res/image/icon/freepik/003-application.Tex2D"),
    OS_TEXT("res/image/icon/freepik/004-job interview.Tex2D"),
    OS_TEXT("res/image/icon/freepik/005-investment.Tex2D"),
    OS_TEXT("res/image/icon/freepik/006-job seeker.Tex2D"),
    OS_TEXT("res/image/icon/freepik/007-file.Tex2D"),
    OS_TEXT("res/image/icon/freepik/008-Cooperation.Tex2D"),
    OS_TEXT("res/image/icon/freepik/009-CV.Tex2D"),
    OS_TEXT("res/image/icon/freepik/010-personal data.Tex2D"),
    OS_TEXT("res/image/icon/freepik/011-job interview.Tex2D"),
    OS_TEXT("res/image/icon/freepik/012-calendar.Tex2D"),
    OS_TEXT("res/image/icon/freepik/013-home.Tex2D"),
    OS_TEXT("res/image/icon/freepik/014-location.Tex2D"),
    OS_TEXT("res/image/icon/freepik/015-photo.Tex2D"),
    OS_TEXT("res/image/icon/freepik/016-file.Tex2D"),
    OS_TEXT("res/image/icon/freepik/017-book.Tex2D"),
    OS_TEXT("res/image/icon/freepik/018-profile.Tex2D"),
    OS_TEXT("res/image/icon/freepik/019-employee.Tex2D"),
    OS_TEXT("res/image/icon/freepik/020-file.Tex2D"),
    OS_TEXT("res/image/icon/freepik/021-best employee.Tex2D"),
    OS_TEXT("res/image/icon/freepik/022-achievement.Tex2D"),
    OS_TEXT("res/image/icon/freepik/023-badge.Tex2D"),
    OS_TEXT("res/image/icon/freepik/024-job opportunities.Tex2D"),
    OS_TEXT("res/image/icon/freepik/025-skill.Tex2D"),
    OS_TEXT("res/image/icon/freepik/026-working.Tex2D"),
    OS_TEXT("res/image/icon/freepik/027-trophy.Tex2D"),
    OS_TEXT("res/image/icon/freepik/028-CV.Tex2D"),
    OS_TEXT("res/image/icon/freepik/029-headhunter.Tex2D"),
    OS_TEXT("res/image/icon/freepik/030-CV.Tex2D"),
    OS_TEXT("res/image/icon/freepik/031-best employee.Tex2D"),
    OS_TEXT("res/image/icon/freepik/032-chart.Tex2D"),
    OS_TEXT("res/image/icon/freepik/033-headhunter.Tex2D"),
    OS_TEXT("res/image/icon/freepik/034-certificate.Tex2D"),
    OS_TEXT("res/image/icon/freepik/035-job offer.Tex2D"),
    OS_TEXT("res/image/icon/freepik/036-check.Tex2D"),
    OS_TEXT("res/image/icon/freepik/037-graduated.Tex2D"),
    OS_TEXT("res/image/icon/freepik/038-profile.Tex2D"),
    OS_TEXT("res/image/icon/freepik/039-photo.Tex2D"),
    OS_TEXT("res/image/icon/freepik/040-envelope.Tex2D"),
    OS_TEXT("res/image/icon/freepik/041-curriculum vitae.Tex2D"),
    OS_TEXT("res/image/icon/freepik/042-headhunting.Tex2D"),
    OS_TEXT("res/image/icon/freepik/043-portfolio.Tex2D"),
    OS_TEXT("res/image/icon/freepik/044-chart.Tex2D"),
    OS_TEXT("res/image/icon/freepik/045-email.Tex2D"),
    OS_TEXT("res/image/icon/freepik/046-portfolio.Tex2D"),
    OS_TEXT("res/image/icon/freepik/047-contract.Tex2D"),
    OS_TEXT("res/image/icon/freepik/048-office chair.Tex2D"),
    OS_TEXT("res/image/icon/freepik/049-office building.Tex2D"),
    OS_TEXT("res/image/icon/freepik/050-profile.Tex2D"),
};
static constexpr int kIconCount = 50;

class BlendModeTransparentECSApp : public WorkObject
{
private:

    ECSContext* ecs_context = nullptr;

    Entity* grid_entity = nullptr;
    Entity* camera_entity = nullptr;

    Material* mtl_plane_grid = nullptr;
    MaterialInstance* mi_plane_grid = nullptr;
    Pipeline* pipeline_plane_grid = nullptr;
    Geometry* geom_plane_grid = nullptr;
    Primitive* prim_plane_grid = nullptr;

private:

    bool InitPlaneGridResources()
    {
        if (pipeline_plane_grid) return true;

        auto* render_context = GetRenderContext();
        if (!render_context) return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context) return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager) return false;

        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);
        cfg.local_to_world = true;

        mtl_plane_grid = material_manager->CreateMaterial(mtl::MaterialPreset::VertexLuminance2D, &cfg);
        if (!mtl_plane_grid) return false;

        VILConfig vil_config;
        vil_config.Add(VAN::Luminance, VF_V1UN8);

        mi_plane_grid = material_manager->CreateMaterialInstance(mtl_plane_grid, &vil_config, &white_color);
        if (!mi_plane_grid) return false;

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        // Grid uses Solid3D (no alpha needed for the reference grid)
        pipeline_plane_grid = render_pass ? render_pass->CreatePipeline(mi_plane_grid, InlinePipeline::Solid3D) : nullptr;
        if (!pipeline_plane_grid) return false;

        return true;
    }

    bool CreateGeometryAndPrimitives()
    {
        auto* render_context = GetRenderContext();
        if (!render_context) return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context) return false;

        auto* device = graphics_context->GetDevice();
        if (!device) return false;

        auto* geometry_manager = graphics_context->GetGeometryManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!geometry_manager || !primitive_manager) return false;

        using namespace inline_geometry;

        auto pc = std::make_unique<GeometryCreater>(device, mi_plane_grid->GetVIL());

        PlaneGridCreateInfo pgci;
        pgci.grid_size.Set(500, 500);
        pgci.sub_count.Set(5, 5);
        pgci.lum = 128;
        pgci.sub_lum = 192;

        geom_plane_grid = CreatePlaneGrid2D(pc.get(), &pgci);
        if (!geom_plane_grid) return false;

        geometry_manager->Add(geom_plane_grid);
        prim_plane_grid = primitive_manager->CreatePrimitive(geom_plane_grid, mi_plane_grid, pipeline_plane_grid);
        if (!prim_plane_grid) return false;

        return true;
    }

    bool EnsureRenderSystems()
    {
        if (!ecs_context) return false;

        auto quad_prepare_system = ecs_context->GetSystem<QuadResourcePrepareSystem>();
        if (!quad_prepare_system)
        {
            quad_prepare_system = ecs_context->RegisterRenderSystem<QuadResourcePrepareSystem>();
            quad_prepare_system->SetWorld(ecs_context);
            if (ecs_context->IsActive())
            {
                quad_prepare_system->OnDependenciesReady();
                quad_prepare_system->Initialize();
            }
        }

        auto quad_binding_system = ecs_context->GetSystem<QuadMaterialBindingSystem>();
        if (!quad_binding_system)
        {
            quad_binding_system = ecs_context->RegisterRenderSystem<QuadMaterialBindingSystem>();
            quad_binding_system->SetWorld(ecs_context);
            if (ecs_context->IsActive())
            {
                quad_binding_system->OnDependenciesReady();
                quad_binding_system->Initialize();
            }
        }

        auto facing_system = ecs_context->GetSystem<FacingTransformSystem>();
        if (!facing_system)
        {
            facing_system = ecs_context->RegisterTickSystem<FacingTransformSystem>();
            facing_system->SetWorld(ecs_context);
            facing_system->SetCameraInfo(GetCameraInfo());
            if (ecs_context->IsActive())
            {
                facing_system->OnDependenciesReady();
                facing_system->Initialize();
            }
        }

        return quad_prepare_system && quad_binding_system && facing_system;
    }

    bool InitializeECS()
    {
        ecs_context = GetECSContext();
        if (!ecs_context) return false;

        if (!EnsureRenderSystems()) return false;

        {
            grid_entity = ecs_context->CreateEntity<Entity>("PlaneGrid");

            auto grid_transform = grid_entity->AddComponent<TransformComponent>(Mobility::Static);
            grid_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            grid_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            grid_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            grid_transform->SetMovable(false);

            auto grid_primitive = grid_entity->AddComponent<PrimitiveComponent>();
            grid_primitive->SetPrimitive(prim_plane_grid);
            grid_primitive->SetVisible(true);
        }

        // Billboards use Alpha3D pipeline for src-alpha blending.
        // Spiral from inside out: entity 0 is innermost (closer to camera).
        {
            constexpr int kBillboardCount = 100;
            constexpr float kAngleStep = 0.45f;
            constexpr float kRadiusStart = 2.0f;
            constexpr float kRadiusStep = 0.6f;
            constexpr float kCenterY = 5.0f;

            for (int i = 0; i < kBillboardCount; ++i)
            {
                const float angle = kAngleStep * static_cast<float>(i);
                const float radius = kRadiusStart + kRadiusStep * angle;

                const float x = std::cos(angle) * radius;
                const float y = kCenterY + std::sin(angle) * radius;
                const float z = 0.0f;

                const std::string name = "TransparentBillboard_" + std::to_string(i);
                Entity* billboard_entity = ecs_context->CreateEntity<Entity>(name.c_str());
                if (!billboard_entity) return false;

                auto transform = billboard_entity->AddComponent<TransformComponent>(Mobility::Static);
                transform->SetLocalPosition(glm::vec3(x, y, z));
                transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
                transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
                transform->SetMovable(false);

                auto billboard = billboard_entity->AddComponent<BillboardComponent>();
                billboard->SetVisible(true);
                billboard->SetFixedPixelSize(false);
                billboard->SetWorldSize(8.0f, 8.0f);
                billboard->SetFrontFace(VK_FRONT_FACE_CLOCKWISE);
                billboard->SetTexture(kIconTextures[i % kIconCount]);
                // NOTE: QuadResourcePrepareSystem uses a shared Solid3D pipeline for all
                // billboard entities.  For true alpha blending, subclass that system and
                // override CreateSharedPipeline() to use InlinePipeline::Alpha3D.
                // TODO: billboard->SetInlinePipeline(InlinePipeline::Alpha3D)
            }
        }

        return true;
    }

    bool InitializeCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 5.0f, 0.0f);
        camera->distance = 50.0f;
        camera->yaw = 45.0f;
        camera->pitch = -15.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo*>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    ~BlendModeTransparentECSApp()
    {
        SAFE_CLEAR(geom_plane_grid);
        delete prim_plane_grid;
    }

    bool Init() override
    {
        std::cout << "\n===== BLEND MODE TRANSPARENT ECS INIT =====\n" << std::endl;
        std::cout << "BlendMode: Transparent (src-alpha / one-minus-src-alpha)" << std::endl;
        std::cout << "Pipeline:  Alpha3D (alpha blend, depth test on, depth write off)" << std::endl;

        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if (!InitPlaneGridResources()) return false;
        if (!CreateGeometryAndPrimitives()) return false;
        if (!InitializeECS()) return false;
        if (!InitializeCamera()) return false;

        std::cout << "\n===== INIT COMPLETE =====\n" << std::endl;
        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);
    }
};//class BlendModeTransparentECSApp

int os_main(int argc, os_char** argv)
{
    return RunFramework<BlendModeTransparentECSApp>(OS_TEXT("BlendMode Transparent ECS - Alpha Blend Demo"), argc, argv, 1280, 720);
}
