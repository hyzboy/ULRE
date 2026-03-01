// Billboard (ECS)
//
// This example demonstrates rendering a billboard and a plane grid using ECS.

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/mtl/MaterialLibrary.h>
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
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<iostream>
#include<memory>
#include<cstdint>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

static const float billboard_position_data[12]=
{
    -0.5f, -0.5f, 0.0f,
     0.5f, -0.5f, 0.0f,
     0.5f,  0.5f, 0.0f,
    -0.5f,  0.5f, 0.0f
};

static const uint16_t billboard_index_data[6]={0,1,2,0,2,3};

static Color4f white_color(1,1,1,1);

class TestApp:public WorkObject
{
private:

    ECSContext *  ecs_world      = nullptr;
    Entity *      grid_entity    = nullptr;
    Entity *      billboard_entity = nullptr;
    Entity *      camera_entity  = nullptr;

    Material *          mtl_plane_grid      = nullptr;
    MaterialInstance *  mi_plane_grid       = nullptr;
    Pipeline *          pipeline_plane_grid = nullptr;
    Geometry *          geom_plane_grid     = nullptr;
    Primitive *         prim_plane_grid     = nullptr;

    MaterialInstance *  mi_billboard        = nullptr;
    Pipeline *          pipeline_billboard  = nullptr;
    Primitive *         prim_billboard      = nullptr;

    Texture2D *         texture             = nullptr;
    Sampler *           sampler             = nullptr;

private:

    bool InitPlaneGridMP()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);

        cfg.local_to_world = true;
        cfg.position_format = VAT_VEC2;

        mtl_plane_grid = material_manager->CreateMaterial(mtl::InlineMaterial::VertexLuminance3D, &cfg);
        if(!mtl_plane_grid)
            return false;

        std::cout << "[BillboardECS] PlaneGrid material: " << (void*)mtl_plane_grid << std::endl;

        VILConfig vil_config;
        vil_config.Add(VAN::Luminance, VF_V1UN8);

        mi_plane_grid = material_manager->CreateMaterialInstance(mtl_plane_grid, &vil_config, &white_color);
        if(!mi_plane_grid)
            return false;

        std::cout << "[BillboardECS] PlaneGrid MI: " << (void*)mi_plane_grid << std::endl;

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline_plane_grid = render_pass ? render_pass->CreatePipeline(mi_plane_grid, InlinePipeline::Solid3D) : nullptr;
        if(!pipeline_plane_grid)
            return false;

        std::cout << "[BillboardECS] PlaneGrid pipeline: " << (void*)pipeline_plane_grid << std::endl;

        return true;
    }

    bool InitBillboardMP()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        mtl::BillboardMaterialCreateConfig cfg(PrimitiveType::Billboard);
        cfg.fixed_size = true;

        mi_billboard = material_manager->CreateMaterialInstance(mtl::InlineMaterial::Billboard2D, &cfg);
        if(!mi_billboard)
            return false;

        std::cout << "[BillboardECS] Billboard MI: " << (void*)mi_billboard
                  << ", Material: " << (void*)mi_billboard->GetMaterial() << std::endl;

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        pipeline_billboard = render_pass ? render_pass->CreatePipeline(mi_billboard, InlinePipeline::Solid3D) : nullptr;
        if(!pipeline_billboard)
            return false;

        std::cout << "[BillboardECS] Billboard pipeline: " << (void*)pipeline_billboard << std::endl;

        return true;
    }

    bool InitTexture()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        TextureManager *tex_manager = graphics_context->GetTextureManager();
        if (!tex_manager)
            return false;

        texture = tex_manager->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"), true);
        if(!texture)
            return false;

        std::cout << "[BillboardECS] Texture loaded: " << (void*)texture
                  << " (" << texture->GetWidth() << "x" << texture->GetHeight() << ")" << std::endl;

        auto* sampler_manager = graphics_context->GetSamplerManager();
        if (!sampler_manager)
            return false;

        sampler = sampler_manager->CreateSampler();

        std::cout << "[BillboardECS] Sampler created: " << (void*)sampler << std::endl;

        const bool bind_ok = mi_billboard->GetMaterial()->BindTextureSampler(DescriptorSetType::PerMaterial,
                                                                              mtl::SamplerName::BaseColor,
                                                                              texture,
                                                                              sampler);
        std::cout << "[BillboardECS] BindTextureSampler(BaseColor): " << (bind_ok ? "OK" : "FAILED")
                  << std::endl;
        if(!bind_ok)
            return false;

        math::Vector2u texture_size(texture->GetWidth(), texture->GetHeight());
        mi_billboard->WriteMIData(texture_size);
        std::cout << "[BillboardECS] Billboard MI data written (texture size)." << std::endl;

        return true;
    }

    bool CreateRenderObject()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* device = graphics_context->GetDevice();
        if (!device)
            return false;

        auto* geometry_manager = graphics_context->GetGeometryManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!geometry_manager || !primitive_manager)
            return false;

        using namespace inline_geometry;

        {
            auto pc = std::make_unique<GeometryCreater>(device, mi_plane_grid->GetVIL());

            PlaneGridCreateInfo pgci;
            pgci.grid_size.Set(500, 500);
            pgci.sub_count.Set(5, 5);
            pgci.lum = 128;
            pgci.sub_lum = 192;

            geom_plane_grid = CreatePlaneGrid2D(pc.get(), &pgci);
            if(!geom_plane_grid)
                return false;

            geometry_manager->Add(geom_plane_grid);
            prim_plane_grid = primitive_manager->CreatePrimitive(geom_plane_grid, mi_plane_grid, pipeline_plane_grid);
            if(!prim_plane_grid)
                return false;

            std::cout << "[BillboardECS] PlaneGrid geometry: " << (void*)geom_plane_grid
                      << ", primitive: " << (void*)prim_plane_grid << std::endl;
        }

        {
            auto pc = std::make_unique<GeometryCreater>(device, mi_billboard->GetVIL());

            pc->Init("Billboard", 4, 6, IndexType::U16);

            if(!pc->WriteVAB(VAN::Position, VF_V3F, billboard_position_data))
                return false;

            if(!pc->WriteIBO(billboard_index_data))
                return false;

            prim_billboard = primitive_manager->CreatePrimitive(pc.get(), mi_billboard, pipeline_billboard);
            if(!prim_billboard)
                return false;

            std::cout << "[BillboardECS] Billboard primitive: " << (void*)prim_billboard << std::endl;
        }

        return true;
    }

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

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        if(!EnsureCameraSystem())
            return false;

        grid_entity = ecs_world->CreateEntity<Entity>("PlaneGrid");
        auto grid_transform = grid_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto grid_primitive = grid_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        grid_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        grid_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        grid_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        grid_transform->SetMovable(false);

        grid_primitive->SetPrimitive(prim_plane_grid);
        grid_primitive->SetVisible(true);

        billboard_entity = ecs_world->CreateEntity<Entity>("Billboard");
        auto billboard_transform = billboard_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto billboard_primitive = billboard_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        billboard_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        billboard_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        billboard_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        billboard_transform->SetMovable(false);

        billboard_primitive->SetPrimitive(prim_billboard);
        billboard_primitive->SetVisible(true);

        return true;
    }

    bool InitCamera()
    {
        if(!EnsureCameraSystem())
            return false;

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
        camera->camera_info = const_cast<graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    ~TestApp()
    {
        SAFE_CLEAR(geom_plane_grid);
        delete prim_plane_grid;
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if(!InitPlaneGridMP())
            return false;

        if(!InitBillboardMP())
            return false;

        if(!InitTexture())
            return false;

        if(!CreateRenderObject())
            return false;

        if(!InitECS())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);
    }
};//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Billboard (ECS)"),1280,720);
}

