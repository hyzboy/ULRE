// Billboard (ECS)
//
// This example demonstrates rendering a billboard and a plane grid using ECS.

#include<hgl/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/graph/module/TextureManager.h>
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

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

static float position_data[3]=
{
    0,0,0
};

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
        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);

        cfg.local_to_world = true;
        cfg.position_format = VAT_VEC2;

        mtl_plane_grid = LoadMaterial("Std3D/VertexLum3D", &cfg);
        if(!mtl_plane_grid)
            return false;

        std::cout << "[BillboardECS] PlaneGrid material: " << (void*)mtl_plane_grid << std::endl;

        VILConfig vil_config;
        vil_config.Add(VAN::Luminance, VF_V1UN8);

        mi_plane_grid = CreateMaterialInstance(mtl_plane_grid, &vil_config, &white_color);
        if(!mi_plane_grid)
            return false;

        std::cout << "[BillboardECS] PlaneGrid MI: " << (void*)mi_plane_grid << std::endl;

        pipeline_plane_grid = CreatePipeline(mi_plane_grid, InlinePipeline::Solid3D);
        if(!pipeline_plane_grid)
            return false;

        std::cout << "[BillboardECS] PlaneGrid pipeline: " << (void*)pipeline_plane_grid << std::endl;

        return true;
    }

    bool InitBillboardMP()
    {
        mtl::BillboardMaterialCreateConfig cfg(PrimitiveType::Billboard);
        cfg.fixed_size = true;

        mi_billboard = CreateMaterialInstance(mtl::inline_material::Billboard2D, &cfg);
        if(!mi_billboard)
            return false;

        std::cout << "[BillboardECS] Billboard MI: " << (void*)mi_billboard
                  << ", Material: " << (void*)mi_billboard->GetMaterial() << std::endl;

        pipeline_billboard = CreatePipeline(mi_billboard, InlinePipeline::Solid3D);
        if(!pipeline_billboard)
            return false;

        std::cout << "[BillboardECS] Billboard pipeline: " << (void*)pipeline_billboard << std::endl;

        return true;
    }

    bool InitTexture()
    {
        TextureManager *tex_manager = GetTextureManager();

        texture = tex_manager->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"), true);
        if(!texture)
            return false;

        std::cout << "[BillboardECS] Texture loaded: " << (void*)texture
                  << " (" << texture->GetWidth() << "x" << texture->GetHeight() << ")" << std::endl;

        sampler = CreateSampler();

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
        using namespace inline_geometry;

        {
            auto pc = GetGeometryCreater(mi_plane_grid);

            PlaneGridCreateInfo pgci;
            pgci.grid_size.Set(500, 500);
            pgci.sub_count.Set(5, 5);
            pgci.lum = 128;
            pgci.sub_lum = 192;

            geom_plane_grid = CreatePlaneGrid2D(pc, &pgci);
            if(!geom_plane_grid)
                return false;

            Add(geom_plane_grid);
            prim_plane_grid = CreatePrimitive(geom_plane_grid, mi_plane_grid, pipeline_plane_grid);
            if(!prim_plane_grid)
                return false;

            std::cout << "[BillboardECS] PlaneGrid geometry: " << (void*)geom_plane_grid
                      << ", primitive: " << (void*)prim_plane_grid << std::endl;
        }

        {
            auto pc = GetGeometryCreater(mi_billboard);

            pc->Init("Billboard", 1);

            if(!pc->WriteVAB(VAN::Position, VF_V3F, position_data))
                return false;

            prim_billboard = CreatePrimitive(pc, mi_billboard, pipeline_billboard);
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
        auto grid_transform = grid_entity->AddComponent<TransformComponent>();
        auto grid_primitive = grid_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        grid_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        grid_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        grid_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        grid_transform->SetMovable(false);

        grid_primitive->SetPrimitive(prim_plane_grid);
        grid_primitive->SetVisible(true);

        billboard_entity = ecs_world->CreateEntity<Entity>("Billboard");
        auto billboard_transform = billboard_entity->AddComponent<TransformComponent>();
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

    using WorkObject::WorkObject;

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

