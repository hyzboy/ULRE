// Billboard Perspective ECS Example - Demonstrates perspective-aware billboards
//
// This example shows billboards with perspective scaling (near-large, far-small).
// Unlike the fixed-size billboard example, these billboards use world-space size
// and will appear larger when closer to the camera and smaller when farther away.
//
// Key differences from BillboardECS.cpp:
// - SetFixedPixelSize(false) instead of true
// - SetWorldSize() instead of SetPixelSize()
// - Multiple billboards at different distances to show perspective effect

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

class BillboardPerspectiveECSApp : public WorkObject
{
private:

    ECSContext* ecs_world = nullptr;

    // Entities
    Entity* grid_entity = nullptr;
    Entity* billboard_near = nullptr;
    Entity* billboard_mid = nullptr;
    Entity* billboard_far = nullptr;
    Entity* camera_entity = nullptr;

    // PlaneGrid resources
    Material* mtl_plane_grid = nullptr;
    MaterialInstance* mi_plane_grid = nullptr;
    Pipeline* pipeline_plane_grid = nullptr;
    Geometry* geom_plane_grid = nullptr;
    Primitive* prim_plane_grid = nullptr;

private:

    /**
     * Initialize plane grid material and resources
     */
    bool InitPlaneGridResources()
    {
        auto* render_context = GetRenderContext();
        if (!render_context) return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context) return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager) return false;

        // Create material
        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);
        cfg.local_to_world = true;
        cfg.position_format = VAT_VEC2;

        mtl_plane_grid = material_manager->CreateMaterial(mtl::MaterialPreset::VertexLuminance3D, &cfg);
        if (!mtl_plane_grid) return false;

        std::cout << "[BillboardPerspective] PlaneGrid material: " << (void*)mtl_plane_grid << std::endl;

        // Create material instance
        VILConfig vil_config;
        vil_config.Add(VAN::Luminance, VF_V1UN8);

        mi_plane_grid = material_manager->CreateMaterialInstance(mtl_plane_grid, &vil_config, &white_color);
        if (!mi_plane_grid) return false;

        std::cout << "[BillboardPerspective] PlaneGrid MI: " << (void*)mi_plane_grid << std::endl;

        // Create pipeline
        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline_plane_grid = render_pass ? render_pass->CreatePipeline(mi_plane_grid, InlinePipeline::Solid3D) : nullptr;
        if (!pipeline_plane_grid) return false;

        std::cout << "[BillboardPerspective] PlaneGrid pipeline: " << (void*)pipeline_plane_grid << std::endl;

        return true;
    }

    /**
     * Create render geometry and primitives
     */
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

        // Create plane grid geometry
        {
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

            std::cout << "[BillboardPerspective] PlaneGrid geometry: " << (void*)geom_plane_grid
                      << ", primitive: " << (void*)prim_plane_grid << std::endl;
        }

        return true;
    }

    /**
     * Ensure camera system is registered and initialized
     */
    bool EnsureCameraSystem()
    {
        if (!ecs_world) return false;

        auto camera_system = ecs_world->GetSystem<CameraSystem>();
        if (!camera_system)
        {
            camera_system = ecs_world->RegisterTickSystem<CameraSystem>(ecs_world);
            if (ecs_world->IsActive())
            {
                camera_system->OnDependenciesReady();
                camera_system->Initialize();
            }
        }

        return camera_system != nullptr;
    }

    /**
     * Ensure render systems are registered
     */
    bool EnsureRenderSystems()
    {
        if (!ecs_world) return false;

        // Register QuadResourcePrepareSystem (shared resources)
        auto quad_prepare_system = ecs_world->GetSystem<QuadResourcePrepareSystem>();
        if (!quad_prepare_system)
        {
            std::cout << "[BillboardPerspective] Creating QuadResourcePrepareSystem..." << std::endl;
            quad_prepare_system = ecs_world->RegisterRenderSystem<QuadResourcePrepareSystem>();
            quad_prepare_system->SetWorld(ecs_world);

            std::cout << "[BillboardPerspective] QuadResourcePrepareSystem created" << std::endl;

            if (ecs_world->IsActive())
            {
                quad_prepare_system->OnDependenciesReady();
                quad_prepare_system->Initialize();
                std::cout << "[BillboardPerspective] QuadResourcePrepareSystem initialized" << std::endl;
            }
        }

        // Register QuadMaterialBindingSystem (per-entity texture binding)
        auto quad_binding_system = ecs_world->GetSystem<QuadMaterialBindingSystem>();
        if (!quad_binding_system)
        {
            std::cout << "[BillboardPerspective] Creating QuadMaterialBindingSystem..." << std::endl;
            quad_binding_system = ecs_world->RegisterRenderSystem<QuadMaterialBindingSystem>();
            quad_binding_system->SetWorld(ecs_world);

            std::cout << "[BillboardPerspective] QuadMaterialBindingSystem created" << std::endl;

            if (ecs_world->IsActive())
            {
                quad_binding_system->OnDependenciesReady();
                quad_binding_system->Initialize();
                std::cout << "[BillboardPerspective] QuadMaterialBindingSystem initialized" << std::endl;
            }
        }

        // Register FacingTransformSystem (handles camera-facing rotation)
        auto facing_system = ecs_world->GetSystem<FacingTransformSystem>();
        if (!facing_system)
        {
            std::cout << "[BillboardPerspective] Creating FacingTransformSystem..." << std::endl;
            facing_system = ecs_world->RegisterTickSystem<FacingTransformSystem>();
            facing_system->SetWorld(ecs_world);
            facing_system->SetCameraInfo(GetCameraInfo());

            std::cout << "[BillboardPerspective] FacingTransformSystem created" << std::endl;

            if (ecs_world->IsActive())
            {
                facing_system->OnDependenciesReady();
                facing_system->Initialize();
                std::cout << "[BillboardPerspective] FacingTransformSystem initialized" << std::endl;
            }
        }

        return quad_prepare_system && quad_binding_system && facing_system;
    }

    /**
     * Initialize ECS entities and components
     */
    bool InitializeECS()
    {
        ecs_world = GetECSContext();
        if (!ecs_world) return false;

        std::cout << "\n[BillboardPerspective] === ECS INITIALIZATION START ===" << std::endl;
        std::cout << "[BillboardPerspective] ECSContext pointer: " << (void*)ecs_world << std::endl;
        std::cout << "[BillboardPerspective] Initial entity count: " << ecs_world->GetEntityCount() << std::endl;

        if (!EnsureCameraSystem()) return false;
        if (!EnsureRenderSystems()) return false;

        std::cout << "\n[BillboardPerspective] Creating PlaneGrid entity..." << std::endl;
        // Create plane grid entity
        {
            grid_entity = ecs_world->CreateEntity<Entity>("PlaneGrid");
            std::cout << "  -> PlaneGrid entity created" << std::endl;

            auto grid_transform = grid_entity->AddComponent<TransformComponent>(Mobility::Static);
            grid_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            grid_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            grid_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            grid_transform->SetMovable(false);

            auto grid_primitive = grid_entity->AddComponent<PrimitiveComponent>();
            grid_primitive->SetPrimitive(prim_plane_grid);
            grid_primitive->SetVisible(true);
        }

        std::cout << "\n[BillboardPerspective] Creating spiral billboards (count=100, Z=0)..." << std::endl;
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

                const std::string name = "BillboardSpiral_" + std::to_string(i);
                Entity* billboard_entity = ecs_world->CreateEntity<Entity>(name.c_str());
                if (!billboard_entity)
                    return false;

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
                billboard->SetTexture(OS_TEXT("res/image/lena.Tex2D"));
            }
        }

        std::cout << "\n[BillboardPerspective] Final entity count: " << ecs_world->GetEntityCount() << std::endl;
        std::cout << "[BillboardPerspective] === ECS INITIALIZATION COMPLETE ===\n" << std::endl;

        return true;
    }

    /**
     * Initialize camera
     */
    bool InitializeCamera()
    {
        if (!EnsureCameraSystem()) return false;

        camera_entity = ecs_world->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 5.0f, 0.0f);  // Look at mid billboard
        camera->distance = 50.0f;  // A bit farther to see all three billboards
        camera->yaw = 45.0f;
        camera->pitch = -15.0f;  // Less steep angle
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo*>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        std::cout << "[BillboardPerspective] Camera configured: distance=50, yaw=45, pitch=-15" << std::endl;

        return true;
    }

public:
    ~BillboardPerspectiveECSApp()
    {
        SAFE_CLEAR(geom_plane_grid);
        delete prim_plane_grid;
    }

    bool Init() override
    {
        std::cout << "\n\n===== BILLBOARD PERSPECTIVE ECS APP INITIALIZATION START =====\n" << std::endl;

        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if (!InitPlaneGridResources()) return false;
        if (!CreateGeometryAndPrimitives()) return false;
        if (!InitializeECS()) return false;
        if (!InitializeCamera()) return false;

        std::cout << "\n[BillboardPerspective] ===== APP INITIALIZATION COMPLETE =====\n" << std::endl;
        std::cout << "\nPERSPECTIVE EFFECT DEMO:" << std::endl;
        std::cout << "  Near billboard (left):   Closer to camera, appears larger" << std::endl;
        std::cout << "  Mid billboard (center):  Medium distance" << std::endl;
        std::cout << "  Far billboard (right):   Farther from camera, appears smaller\n" << std::endl;

        return true;
    }

    void Tick(double delta_time) override
    {
        static int frame_count = 0;
        frame_count++;

        if (frame_count <= 3)
        {
            std::cout << "\n[BillboardPerspective] Frame " << frame_count << " starting..." << std::endl;
            if (ecs_world)
            {
                std::cout << "  -> Entity count: " << ecs_world->GetEntityCount() << std::endl;
            }
        }

        WorkObject::Tick(delta_time);

        if (frame_count <= 3)
        {
            std::cout << "[BillboardPerspective] Frame " << frame_count << " end" << std::endl;
        }
    }
};//class BillboardPerspectiveECSApp:public WorkObject

int os_main(int argc, os_char** argv)
{
    return RunFramework<BillboardPerspectiveECSApp>(OS_TEXT("Billboard Perspective ECS Example - Near Large, Far Small"), argc, argv, 1280, 720);
}
