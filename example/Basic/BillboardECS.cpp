// Billboard ECS Example - Refactored with decoupled Quad + FacingTransform
//
// This example demonstrates the new decoupled architecture:
// - QuadComponent: Handles quad geometry and rendering
// - FacingTransformComponent: Handles camera-facing rotation
// - BillboardComponent: Convenience wrapper that combines both
//
// The architecture allows for flexible reuse:
// - Use just QuadComponent for static sprites
// - Use just FacingTransformComponent for other entities that need rotation
// - Use BillboardComponent for the classic billboard use case

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
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
#include<hgl/ecs/systems/render/QuadRenderSystem.h>
#include<hgl/ecs/systems/transform/FacingTransformSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<iostream>
#include<memory>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

static Color4f white_color(1, 1, 1, 1);

class BillboardECSApp : public WorkObject
{
private:

    ECSContext* ecs_world = nullptr;

    // Entities
    Entity* grid_entity = nullptr;
    Entity* billboard_entity = nullptr;
    Entity* camera_entity = nullptr;

    // PlaneGrid resources
    Material* mtl_plane_grid = nullptr;
    MaterialInstance* mi_plane_grid = nullptr;
    Pipeline* pipeline_plane_grid = nullptr;
    Geometry* geom_plane_grid = nullptr;
    Primitive* prim_plane_grid = nullptr;

    // Billboard resources are managed by BillboardRenderSystem

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

        mtl_plane_grid = material_manager->LoadMaterial("Std3D/VertexLum3D", &cfg);
        if (!mtl_plane_grid) return false;

        std::cout << "[BillboardECS] PlaneGrid material: " << (void*)mtl_plane_grid << std::endl;

        // Create material instance
        VILConfig vil_config;
        vil_config.Add(VAN::Luminance, VF_V1UN8);

        mi_plane_grid = material_manager->CreateMaterialInstance(mtl_plane_grid, &vil_config, &white_color);
        if (!mi_plane_grid) return false;

        std::cout << "[BillboardECS] PlaneGrid MI: " << (void*)mi_plane_grid << std::endl;

        // Create pipeline
        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline_plane_grid = render_pass ? render_pass->CreatePipeline(mi_plane_grid, InlinePipeline::Solid3D) : nullptr;
        if (!pipeline_plane_grid) return false;

        std::cout << "[BillboardECS] PlaneGrid pipeline: " << (void*)pipeline_plane_grid << std::endl;

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

            std::cout << "[BillboardECS] PlaneGrid geometry: " << (void*)geom_plane_grid
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
     * - QuadRenderSystem: Handles texture loading and material creation
     * - FacingTransformSystem: Handles camera-facing rotation
     */
    bool EnsureRenderSystems()
    {
        if (!ecs_world) return false;

        // Register QuadRenderSystem (handles texture loading and material creation)
        auto quad_system = ecs_world->GetSystem<QuadRenderSystem>();
        if (!quad_system)
        {
            std::cout << "[BillboardECS] Creating QuadRenderSystem..." << std::endl;
            quad_system = ecs_world->RegisterTickSystem<QuadRenderSystem>();
            quad_system->SetWorld(ecs_world);

            std::cout << "[BillboardECS] QuadRenderSystem created at " << (void*)quad_system.get() << std::endl;

            if (ecs_world->IsActive())
            {
                quad_system->OnDependenciesReady();
                quad_system->Initialize();
                std::cout << "[BillboardECS] QuadRenderSystem initialized and started" << std::endl;
            }
        }

        // Register FacingTransformSystem (handles camera-facing rotation)
        auto facing_system = ecs_world->GetSystem<FacingTransformSystem>();
        if (!facing_system)
        {
            std::cout << "[BillboardECS] Creating FacingTransformSystem..." << std::endl;
            facing_system = ecs_world->RegisterTickSystem<FacingTransformSystem>();
            facing_system->SetWorld(ecs_world);
            facing_system->SetCameraInfo(GetCameraInfo());

            std::cout << "[BillboardECS] FacingTransformSystem created at " << (void*)facing_system.get() << std::endl;

            if (ecs_world->IsActive())
            {
                facing_system->OnDependenciesReady();
                facing_system->Initialize();
                std::cout << "[BillboardECS] FacingTransformSystem initialized and started" << std::endl;
            }
        }

        return quad_system && facing_system;
    }

    /**
     * Initialize ECS entities and components
     */
    bool InitializeECS()
    {
        ecs_world = GetECSContext();
        if (!ecs_world) return false;

        std::cout << "\n[BillboardECS] === ECS INITIALIZATION START ===" << std::endl;
        std::cout << "[BillboardECS] ECSContext pointer: " << (void*)ecs_world << std::endl;
        std::cout << "[BillboardECS] Initial entity count: " << ecs_world->GetEntityCount() << std::endl;

        if (!EnsureCameraSystem()) return false;
        if (!EnsureRenderSystems()) return false;

        std::cout << "\n[BillboardECS] Creating PlaneGrid entity..." << std::endl;
        // Create plane grid entity
        {
            grid_entity = ecs_world->CreateEntity<Entity>("PlaneGrid");
            std::cout << "  -> PlaneGrid entity created at " << (void*)grid_entity << std::endl;

            auto grid_transform = grid_entity->AddComponent<TransformComponent>();
            grid_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            grid_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            grid_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            grid_transform->SetMovable(false);
            std::cout << "  -> TransformComponent added" << std::endl;

            auto grid_primitive = grid_entity->AddComponent<PrimitiveComponent>();
            grid_primitive->SetPrimitive(prim_plane_grid);
            grid_primitive->SetVisible(true);
            std::cout << "  -> PrimitiveComponent added, visible=" << grid_primitive->IsVisible() << std::endl;
            std::cout << "  -> Primitive pointer: " << (void*)prim_plane_grid << std::endl;
        }

        std::cout << "\n[BillboardECS] Creating Billboard entity..." << std::endl;
        // Create billboard entity with BillboardComponent
        // Note: BillboardComponent now acts as a convenience wrapper.
        // When attached, it automatically creates:
        // - QuadComponent (for quad rendering)
        // - FacingTransformComponent (for camera-facing rotation)
        {
            billboard_entity = ecs_world->CreateEntity<Entity>("Billboard");
            std::cout << "  -> Billboard entity created at " << (void*)billboard_entity << std::endl;

            auto billboard_transform = billboard_entity->AddComponent<TransformComponent>();
            billboard_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            billboard_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            billboard_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            billboard_transform->SetMovable(false);
            std::cout << "  -> TransformComponent added" << std::endl;

            auto billboard = billboard_entity->AddComponent<BillboardComponent>();
            std::cout << "  -> BillboardComponent added at " << (void*)billboard.get() << std::endl;
            std::cout << "     (automatically created QuadComponent and FacingTransformComponent)" << std::endl;

            billboard->SetVisible(true);
            std::cout << "  -> SetVisible(true)" << std::endl;

            billboard->SetFixedPixelSize(true);
            std::cout << "  -> SetFixedPixelSize(true)" << std::endl;

            billboard->SetPixelSize(256, 256);
            std::cout << "  -> SetPixelSize(256, 256)" << std::endl;

            billboard->SetFrontFace(VK_FRONT_FACE_CLOCKWISE);
            std::cout << "  -> SetFrontFace(CLOCKWISE)" << std::endl;

            billboard->SetTexture(OS_TEXT("res/image/lena.Tex2D"));
            std::cout << "  -> SetTexture(lena.Tex2D)" << std::endl;
        }

        // ADVANCED: Example of using QuadComponent directly (without facing transform)
        //
        // This shows the flexibility of the decoupled architecture:
        // If you only need a static sprite (no camera-facing rotation),
        // you can use just QuadComponent.
        //
        // This is commented out by default, but you can uncomment to test:
        /*
        std::cout << "\n[BillboardECS] Creating Static Quad entity..." << std::endl;
        {
            auto quad_entity = ecs_world->CreateEntity<Entity>("StaticQuad");
            std::cout << "  -> StaticQuad entity created" << std::endl;

            auto quad_transform = quad_entity->AddComponent<TransformComponent>();
            quad_transform->SetLocalPosition(glm::vec3(2.0f, 0.0f, 0.0f));
            quad_transform->SetLocalScale(glm::vec3(2.0f, 2.0f, 1.0f));

            auto quad = quad_entity->AddComponent<QuadComponent>();
            quad->SetVisible(true);
            quad->SetPixelSize(128, 128);
            quad->SetTexturePath(OS_TEXT("res/image/sprite.Tex2D"));
            std::cout << "  -> QuadComponent added (no rotation, static)" << std::endl;
        }
        */

        // ADVANCED: Example of using FacingTransformComponent with a custom target
        //
        // This shows using FacingTransformComponent independently:
        // Any entity can face towards a specific position.
        //
        // This is commented out by default, but you can uncomment to test:
        /*
        std::cout << "\n[BillboardECS] Creating Look-At-Target entity..." << std::endl;
        {
            auto target_entity = ecs_world->CreateEntity<Entity>("LookAtTarget");
            std::cout << "  -> LookAtTarget entity created" << std::endl;

            auto target_transform = target_entity->AddComponent<TransformComponent>();
            target_transform->SetLocalPosition(glm::vec3(-2.0f, 0.0f, 0.0f));

            auto quad = target_entity->AddComponent<QuadComponent>();
            quad->SetVisible(true);
            quad->SetPixelSize(128, 128);
            quad->SetTexturePath(OS_TEXT("res/image/marker.Tex2D"));

            auto facing = target_entity->AddComponent<FacingTransformComponent>();
            facing->SetFacingMode(FacingMode::LookAtTarget);
            facing->SetTargetPosition(glm::vec3(2.0f, 0.0f, 0.0f));  // Look at the static quad
            std::cout << "  -> FacingTransformComponent added (looks at static quad)" << std::endl;
        }
        */

        std::cout << "\n[BillboardECS] Final entity count: " << ecs_world->GetEntityCount() << std::endl;
        std::cout << "[BillboardECS] === ECS INITIALIZATION COMPLETE ===\n" << std::endl;

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
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 45.0f;
        camera->yaw = 45.0f;
        camera->pitch = -25.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo*>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    ~BillboardECSApp()
    {
        SAFE_CLEAR(geom_plane_grid);
        delete prim_plane_grid;
    }

    bool Init() override
    {
        std::cout << "\n\n===== BILLBOARD ECS APP INITIALIZATION START =====\n" << std::endl;

        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if (!InitPlaneGridResources()) return false;
        if (!CreateGeometryAndPrimitives()) return false;
        if (!InitializeECS()) return false;
        if (!InitializeCamera()) return false;

        std::cout << "\n[BillboardECS] ===== APP INITIALIZATION COMPLETE =====\n" << std::endl;

        return true;
    }

    void Tick(double delta_time) override
    {
        static int frame_count = 0;
        frame_count++;

        if (frame_count <= 3)
        {
            std::cout << "\n[BillboardECS] Frame " << frame_count << " starting..." << std::endl;
            if (ecs_world)
            {
                std::cout << "  -> Entity count: " << ecs_world->GetEntityCount() << std::endl;
            }
        }

        WorkObject::Tick(delta_time);

        if (frame_count <= 3)
        {
            std::cout << "[BillboardECS] Frame " << frame_count << " end" << std::endl;
        }
    }
};//class BillboardECSApp:public WorkObject

int os_main(int, os_char**)
{
    return RunFramework<BillboardECSApp>(OS_TEXT("Billboard ECS Example"), 1280, 720);
}
