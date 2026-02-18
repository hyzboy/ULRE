// Billboard ECS Example - Refactored with BillboardComponent
//
// This example demonstrates rendering a billboard using the ECS architecture.
// It showcases the integration of BillboardComponent into the existing ECS system.

#include<hgl/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/color/Color.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/BillboardComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/BillboardRenderSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<iostream>
#include<memory>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

static float position_data[3] = { 0, 0, 0 };
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

    // Billboard resources
    MaterialInstance* mi_billboard = nullptr;
    Pipeline* pipeline_billboard = nullptr;
    Primitive* prim_billboard = nullptr;

    // Textures
    Texture2D* texture = nullptr;
    Sampler* sampler = nullptr;

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
     * Initialize billboard material and resources
     */
    bool InitBillboardResources()
    {
        auto* render_context = GetRenderContext();
        if (!render_context) return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context) return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager) return false;

        // Create billboard material with fixed size config
        mtl::BillboardMaterialCreateConfig cfg(PrimitiveType::Billboard);
        cfg.fixed_size = true;

        mi_billboard = material_manager->CreateMaterialInstance(mtl::inline_material::Billboard2D, &cfg);
        if (!mi_billboard) return false;

        std::cout << "[BillboardECS] Billboard MI: " << (void*)mi_billboard
                  << ", Material: " << (void*)mi_billboard->GetMaterial() << std::endl;

        // Create pipeline
        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline_billboard = render_pass ? render_pass->CreatePipeline(mi_billboard, InlinePipeline::Solid3D) : nullptr;
        if (!pipeline_billboard) return false;

        std::cout << "[BillboardECS] Billboard pipeline: " << (void*)pipeline_billboard << std::endl;

        return true;
    }

    /**
     * Load and bind texture for billboard
     */
    bool InitTexture()
    {
        auto* render_context = GetRenderContext();
        if (!render_context) return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context) return false;

        TextureManager* tex_manager = graphics_context->GetTextureManager();
        if (!tex_manager) return false;

        texture = tex_manager->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"), true);
        if (!texture) return false;

        std::cout << "[BillboardECS] Texture loaded: " << (void*)texture
                  << " (" << texture->GetWidth() << "x" << texture->GetHeight() << ")" << std::endl;

        auto* sampler_manager = graphics_context->GetSamplerManager();
        if (!sampler_manager) return false;

        sampler = sampler_manager->CreateSampler();
        std::cout << "[BillboardECS] Sampler created: " << (void*)sampler << std::endl;

        // Bind texture to material
        const bool bind_ok = mi_billboard->GetMaterial()->BindTextureSampler(DescriptorSetType::PerMaterial,
                                                                              mtl::SamplerName::BaseColor,
                                                                              texture,
                                                                              sampler);
        std::cout << "[BillboardECS] BindTextureSampler(BaseColor): " << (bind_ok ? "OK" : "FAILED") << std::endl;
        if (!bind_ok) return false;

        // Write material instance data (texture size for billboard calculation)
        math::Vector2u texture_size(texture->GetWidth(), texture->GetHeight());
        mi_billboard->WriteMIData(texture_size);
        std::cout << "[BillboardECS] Billboard MI data written (texture size)." << std::endl;

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

        // Create billboard geometry (single point)
        {
            auto pc = std::make_unique<GeometryCreater>(device, mi_billboard->GetVIL());

            pc->Init("Billboard", 1);

            if (!pc->WriteVAB(VAN::Position, VF_V3F, position_data))
                return false;

            prim_billboard = primitive_manager->CreatePrimitive(pc.get(), mi_billboard, pipeline_billboard);
            if (!prim_billboard) return false;

            std::cout << "[BillboardECS] Billboard primitive: " << (void*)prim_billboard << std::endl;
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
     * Ensure billboard render system is registered
     * NOTE: Using RegisterTickSystem because RegisterRenderSystem may not work correctly
     * if called after ECSContext is already active. Tick systems execute reliably.
     */
    bool EnsureBillboardRenderSystem()
    {
        if (!ecs_world) return false;

        auto billboard_system = ecs_world->GetSystem<BillboardRenderSystem>();
        if (!billboard_system)
        {
            std::cout << "[BillboardECS] Creating BillboardRenderSystem..." << std::endl;
            // Use RegisterTickSystem instead of RegisterRenderSystem for reliability
            billboard_system = ecs_world->RegisterTickSystem<BillboardRenderSystem>();
            billboard_system->SetWorld(ecs_world);
            billboard_system->SetCameraInfo(GetCameraInfo());

            std::cout << "[BillboardECS] BillboardRenderSystem created at " << (void*)billboard_system.get() << std::endl;

            if (ecs_world->IsActive())
            {
                billboard_system->OnDependenciesReady();
                billboard_system->Initialize();
                std::cout << "[BillboardECS] BillboardRenderSystem initialized and started" << std::endl;
            }
        }
        else
        {
            std::cout << "[BillboardECS] BillboardRenderSystem already exists at " << (void*)billboard_system.get() << std::endl;
        }

        return billboard_system != nullptr;
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
        if (!EnsureBillboardRenderSystem()) return false;

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
            
            billboard->SetPrimitive(prim_billboard);
            std::cout << "  -> Primitive set to " << (void*)prim_billboard << std::endl;
            
            billboard->SetVisible(true);
            std::cout << "  -> SetVisible(true)" << std::endl;
            
            billboard->SetFixedPixelSize(true);
            std::cout << "  -> SetFixedPixelSize(true)" << std::endl;
            
            billboard->SetPixelSize(256, 256);
            std::cout << "  -> SetPixelSize(256, 256)" << std::endl;
            
            billboard->SetFrontFace(VK_FRONT_FACE_CLOCKWISE);
            std::cout << "  -> SetFrontFace(CLOCKWISE)" << std::endl;
        }

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

    using WorkObject::WorkObject;

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
        if (!InitBillboardResources()) return false;
        if (!InitTexture()) return false;
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
