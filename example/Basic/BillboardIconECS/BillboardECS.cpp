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
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/graph/module/GeometryManager.h>
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
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include<hgl/ecs/systems/render/QuadMaterialBindingSystem.h>
#include<hgl/ecs/systems/transform/FacingTransformSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<iostream>
#include<memory>

#include "IconGradient.h"

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

static Color4f white_color(1, 1, 1, 1);

class BillboardECSApp : public WorkObject
{
private:

    ECSContext* ecs_context = nullptr;

    // Entities
    Entity* grid_entity = nullptr;
    Entity* billboard_entity = nullptr;
    Entity* camera_entity = nullptr;

    // PlaneGrid resources
    Geometry* geom_plane_grid = nullptr;

    inline static const mtl::MaterialAssetRecord kPlaneGridCfg {
        .id       = "billboard_ecs_plane_grid",
        .preset   = mtl::MaterialPreset::VertexLuminance2D,
        .prim     = PrimitiveType::Lines,
        .pipeline = GraphicsPipelinePreset::Solid3D,
    };

    // Billboard resources are managed by BillboardRenderSystem

    bool last_key_1 = false;
    bool last_key_2 = false;
    bool last_key_3 = false;

    GraphicsPipelinePreset current_pipeline = GraphicsPipelinePreset::Solid3D;
    int current_icon_index = 22;

private:

    static const char* GetPipelineName(GraphicsPipelinePreset preset)
    {
        switch (preset)
        {
        case GraphicsPipelinePreset::Solid3D:  return "Solid3D";
        case GraphicsPipelinePreset::Alpha3D:  return "Alpha3D";
        case GraphicsPipelinePreset::Dither3D: return "Dither3D";
        default:                               return "Unknown";
        }
    }

    bool ApplyBillboardMode(GraphicsPipelinePreset preset, int icon_index)
    {
        if (!ecs_context || !billboard_entity)
            return false;

        auto billboard = billboard_entity->GetComponent<BillboardComponent>();
        if (!billboard)
            return false;

        if (icon_index < 0)
            icon_index = 0;
        if (icon_index >= kIconCount)
            icon_index = kIconCount - 1;

        current_pipeline = preset;
        current_icon_index = icon_index;

        QuadResourcePrepareSystem::SetPresetForWorld(ecs_context, preset);
        // Single-channel gradient textures should drive billboard alpha as well.
        QuadResourcePrepareSystem::SetChannelHintForWorld(ecs_context, TextureChannelHint::Grayscale);

        billboard->SetTexture(kIconTextures[current_icon_index]);

        std::cout << "[BillboardECS] Mode switched: " << GetPipelineName(preset)
                  << ", texture index=" << current_icon_index << std::endl;
        return true;
    }

    void HandleRuntimeModeSwitch()
    {
        if (!ecs_context)
            return;

        auto input_system = ecs_context->GetSystem<InputSystem>();
        if (!input_system)
            return;

        const bool key_1 = input_system->IsKeyDown(io::KeyboardButton::_1);
        const bool key_2 = input_system->IsKeyDown(io::KeyboardButton::_2);
        const bool key_3 = input_system->IsKeyDown(io::KeyboardButton::_3);

        if (key_1 && !last_key_1)
            ApplyBillboardMode(GraphicsPipelinePreset::Solid3D, 22);
        else if (key_2 && !last_key_2)
            ApplyBillboardMode(GraphicsPipelinePreset::Alpha3D, 10);
        else if (key_3 && !last_key_3)
            ApplyBillboardMode(GraphicsPipelinePreset::Dither3D, 6);

        last_key_1 = key_1;
        last_key_2 = key_2;
        last_key_3 = key_3;
    }

    /**
     * Initialize plane grid material and resources
     */
    bool InitPlaneGridResources()
    {
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

        GraphicsGeometryFactory geometry_factory(graphics_context);

        using namespace inline_geometry;

        // Create plane grid geometry
        {
            GeometryVertexFormat gvf;
            gvf.Set(VAN::Position,  VF_V2F);
            gvf.Set(VAN::Luminance, VF_V1UN8);

            auto pc = geometry_factory.CreateCreater(gvf);
            if (!pc) return false;

            PlaneGridCreateInfo pgci;
            pgci.grid_size.Set(500, 500);
            pgci.sub_count.Set(5, 5);
            pgci.lum = 128;
            pgci.sub_lum = 192;

            geom_plane_grid = CreatePlaneGrid2D(pc.get(), &pgci);
            if (!geom_plane_grid) return false;

            if (!geometry_factory.RegisterGeometry(geom_plane_grid)) return false;

            std::cout << "[BillboardECS] PlaneGrid geometry: " << (void*)geom_plane_grid << std::endl;
        }

        return true;
    }

    /**
     * Ensure render systems are registered
     * - QuadResourcePrepareSystem: Prepares shared resources (geometry, material, sampler)
     * - QuadMaterialBindingSystem: Binds textures per quad entity
     * - FacingTransformSystem: Handles camera-facing rotation
     */
    bool EnsureRenderSystems()
    {
        if (!ecs_context) return false;

        // Register QuadResourcePrepareSystem (shared resources)
        auto quad_prepare_system = ecs_context->GetSystem<QuadResourcePrepareSystem>();
        if (!quad_prepare_system)
        {
            std::cout << "[BillboardECS] Creating QuadResourcePrepareSystem..." << std::endl;
            quad_prepare_system = ecs_context->RegisterRenderSystem<QuadResourcePrepareSystem>();
            quad_prepare_system->SetWorld(ecs_context);

            std::cout << "[BillboardECS] QuadResourcePrepareSystem created" << std::endl;

            if (ecs_context->IsActive())
            {
                quad_prepare_system->OnDependenciesReady();
                quad_prepare_system->Initialize();
                std::cout << "[BillboardECS] QuadResourcePrepareSystem initialized" << std::endl;
            }
        }

        // Register QuadMaterialBindingSystem (per-entity texture binding)
        auto quad_binding_system = ecs_context->GetSystem<QuadMaterialBindingSystem>();
        if (!quad_binding_system)
        {
            std::cout << "[BillboardECS] Creating QuadMaterialBindingSystem..." << std::endl;
            quad_binding_system = ecs_context->RegisterRenderSystem<QuadMaterialBindingSystem>();
            quad_binding_system->SetWorld(ecs_context);

            std::cout << "[BillboardECS] QuadMaterialBindingSystem created" << std::endl;

            if (ecs_context->IsActive())
            {
                quad_binding_system->OnDependenciesReady();
                quad_binding_system->Initialize();
                std::cout << "[BillboardECS] QuadMaterialBindingSystem initialized" << std::endl;
            }
        }

        // Register FacingTransformSystem (handles camera-facing rotation)
        auto facing_system = ecs_context->GetSystem<FacingTransformSystem>();
        if (!facing_system)
        {
            std::cout << "[BillboardECS] Creating FacingTransformSystem..." << std::endl;
            facing_system = ecs_context->RegisterTickSystem<FacingTransformSystem>();
            facing_system->SetWorld(ecs_context);
            facing_system->SetCameraInfo(GetCameraInfo());

            std::cout << "[BillboardECS] FacingTransformSystem created at " << (void*)facing_system.get() << std::endl;

            if (ecs_context->IsActive())
            {
                facing_system->OnDependenciesReady();
                facing_system->Initialize();
                std::cout << "[BillboardECS] FacingTransformSystem initialized and started" << std::endl;
            }
        }

        return quad_prepare_system && quad_binding_system && facing_system;
    }

    /**
     * Initialize ECS entities and components
     */
    bool InitializeECS()
    {
        ecs_context = GetECSContext();
        if (!ecs_context) return false;

        std::cout << "\n[BillboardECS] === ECS INITIALIZATION START ===" << std::endl;
        std::cout << "[BillboardECS] ECSContext pointer: " << (void*)ecs_context << std::endl;
        std::cout << "[BillboardECS] Initial entity count: " << ecs_context->GetEntityCount() << std::endl;

        if (!EnsureRenderSystems()) return false;

        QuadResourcePrepareSystem::SetPresetForWorld(ecs_context, GraphicsPipelinePreset::Solid3D);
        QuadResourcePrepareSystem::SetChannelHintForWorld(ecs_context, TextureChannelHint::Grayscale);

        std::cout << "\n[BillboardECS] Creating PlaneGrid entity..." << std::endl;
        // Create plane grid entity
        {
            grid_entity = ecs_context->CreateEntity<Entity>("PlaneGrid");
            std::cout << "  -> PlaneGrid entity created at " << (void*)grid_entity << std::endl;

            auto grid_transform = grid_entity->AddComponent<TransformComponent>(Mobility::Static);
            grid_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            grid_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            grid_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            grid_transform->SetMovable(false);
            std::cout << "  -> TransformComponent added" << std::endl;

            auto grid_primitive = grid_entity->AddComponent<PrimitiveComponent>();
            grid_primitive->SetUnresolvedGeometry(geom_plane_grid);
            grid_primitive->SetMaterialRecord(&kPlaneGridCfg, &white_color, sizeof(white_color));
            grid_primitive->SetVisible(true);
            std::cout << "  -> PrimitiveComponent added, visible=" << grid_primitive->IsVisible() << std::endl;
        }

        std::cout << "\n[BillboardECS] Creating Billboard entity..." << std::endl;
        // Create billboard entity with BillboardComponent
        // Note: BillboardComponent now acts as a convenience wrapper.
        // When attached, it automatically creates:
        // - QuadComponent (for quad rendering)
        // - FacingTransformComponent (for camera-facing rotation)
        {
            billboard_entity = ecs_context->CreateEntity<Entity>("Billboard");
            std::cout << "  -> Billboard entity created at " << (void*)billboard_entity << std::endl;

            auto billboard_transform = billboard_entity->AddComponent<TransformComponent>(Mobility::Static);
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

            billboard->SetTexture(kIconTextures[current_icon_index]);
            billboard->SetDomainTag("billboard_ecs");
            std::cout << "  -> SetTexture(gradient[" << current_icon_index << "])" << std::endl;
        }

        std::cout << "\n[BillboardECS] Runtime controls: [1]=Solid, [2]=Alpha, [3]=Dither" << std::endl;
        std::cout << "[BillboardECS] Default mode: Solid3D, texture=" << current_icon_index << "\n" << std::endl;

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
            auto quad_entity = ecs_context->CreateEntity<Entity>("StaticQuad");
            std::cout << "  -> StaticQuad entity created" << std::endl;

            auto quad_transform = quad_entity->AddComponent<TransformComponent>(Mobility::Static);
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
            auto target_entity = ecs_context->CreateEntity<Entity>("LookAtTarget");
            std::cout << "  -> LookAtTarget entity created" << std::endl;

            auto target_transform = target_entity->AddComponent<TransformComponent>(Mobility::Static);
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

        std::cout << "\n[BillboardECS] Final entity count: " << ecs_context->GetEntityCount() << std::endl;
        std::cout << "[BillboardECS] === ECS INITIALIZATION COMPLETE ===\n" << std::endl;

        return true;
    }

    /**
     * Initialize camera
     */
    bool InitializeCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
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
            if (ecs_context)
            {
                std::cout << "  -> Entity count: " << ecs_context->GetEntityCount() << std::endl;
            }
        }

        WorkObject::Tick(delta_time);
        HandleRuntimeModeSwitch();

        if (frame_count <= 3)
        {
            std::cout << "[BillboardECS] Frame " << frame_count << " end" << std::endl;
        }
    }
};//class BillboardECSApp:public WorkObject

int os_main(int argc, os_char** argv)
{
    return RunFramework<BillboardECSApp>(OS_TEXT("Billboard ECS Example"), argc, argv, 1280, 720);
}

