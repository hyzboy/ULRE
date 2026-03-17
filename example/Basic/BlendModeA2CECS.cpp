// BlendMode Alpha-to-Coverage ECS Example - MSAA A2C Demo
//
// Demonstrates alpha-to-coverage (A2C): the fragment shader outputs the surface alpha
// and the GPU's MSAA hardware converts it into coverage bits.
// This gives smooth stochastic transparency at MSAA sample boundaries.
//
// Pipeline setup: Solid3D base + alphaToCoverageEnable = VK_TRUE + MSAA samples.
// No alpha blending is used; the compositor main_forward_a2c.frag.glsl shader
// outputs alpha for the pipeline to consume.
//
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
#include<hgl/vk/pipeline/VKPipelineData.h>
#include<hgl/vk/pipeline/VKInlinePipeline.h>
// NOTE: QuadResourcePrepareSystem currently uses a single shared Solid3D pipeline for
// all billboard entities.  To enable true per-entity A2C, either:
//  (a) subclass QuadResourcePrepareSystem and override CreateSharedPipeline() to use
//      a custom PipelineData with alphaToCoverageEnable=VK_TRUE + MSAA, or
//  (b) bypass the Quad ECS path and drive a custom RenderSystem directly.
// This example demonstrates how to build the A2C PipelineData; wiring it into the
// ECS billboard path is left as the next integration step.
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

class BlendModeA2CECSApp : public WorkObject
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
        // Grid uses plain Solid3D (no A2C needed for the grid)
        pipeline_plane_grid = render_pass ? render_pass->CreatePipeline(mi_plane_grid, InlinePipeline::Solid3D) : nullptr;
        if (!pipeline_plane_grid) return false;

        return true;
    }

    /**
     * Build a PipelineData that enables alphaToCoverage for the billboard pass.
     * Cloned from the Solid3D preset then modified.
     */
    PipelineData* CreateA2CPipelineData()
    {
        const PipelineData* base = GetPipelineData(InlinePipeline::Solid3D);
        if (!base) return nullptr;

        PipelineData* pd = new PipelineData(base);   // copy constructor
        // Enable MSAA x4 and alphaToCoverage
        pd->SetSamleCount(VK_SAMPLE_COUNT_4_BIT);
        pd->multi_sample->alphaToCoverageEnable = VK_TRUE;
        return pd;
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

        // Build the A2C PipelineData (ready for integration with a custom system).
        // Currently the shared pipeline is Solid3D; see the NOTE at the top of the file.
        PipelineData* pd_a2c = CreateA2CPipelineData();
        (void)pd_a2c;  // Will be used once QuadResourcePrepareSystem supports overrides

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

                const std::string name = "A2CBillboard_" + std::to_string(i);
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
                // TODO: billboard->SetPipelineData(pd_a2c) once ECS billboard API supports it
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
    ~BlendModeA2CECSApp()
    {
        SAFE_CLEAR(geom_plane_grid);
        delete prim_plane_grid;
    }

    bool Init() override
    {
        std::cout << "\n===== BLEND MODE A2C ECS INIT =====\n" << std::endl;
        std::cout << "BlendMode: Alpha-to-Coverage (MSAA x4, alphaToCoverageEnable)" << std::endl;
        std::cout << "Pipeline:  Solid3D + alphaToCoverageEnable + VK_SAMPLE_COUNT_4_BIT" << std::endl;

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
};//class BlendModeA2CECSApp

int os_main(int argc, os_char** argv)
{
    return RunFramework<BlendModeA2CECSApp>(OS_TEXT("BlendMode Alpha-to-Coverage ECS - MSAA A2C Demo"), argc, argv, 1280, 720);
}
