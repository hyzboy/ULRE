/*
 Gizmo ECS 使用示例

 展示如何同时使用 Move, Rotate, Scale 三种 Gizmo
 按键 W/E/R 切换模式
*/

#include<hgl/framework/WorkManager.h>
#include"Gizmo.h"
#include"GizmoResource.h"
#include<hgl/math/VectorTypes.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/color/Color.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>

#include<glm/glm.hpp>
#include<iostream>
#include<memory>
#include<string>

using namespace hgl;
using namespace hgl::graph;

const math::Vector3f GizmoPosition(0, 0, 0);

class GizmoExampleApp : public WorkObject
{
private:
    hgl::ecs::ECSContext *ecs_world = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;
    hgl::ecs::Entity *plane_entity = nullptr;
    hgl::ecs::Entity *cube_entity = nullptr;

    std::shared_ptr<TransformGizmoSystem> gizmo_system;

    Material *grid_material = nullptr;
    MaterialInstance *grid_mi = nullptr;
    Pipeline *grid_pipeline = nullptr;
    Geometry *grid_geometry = nullptr;
    Primitive *grid_primitive = nullptr;

    Material *cube_material = nullptr;
    MaterialInstance *cube_mi = nullptr;
    Pipeline *cube_pipeline = nullptr;
    Geometry *cube_geometry = nullptr;
    Primitive *cube_primitive = nullptr;

    std::string debug_cache;

    bool InitSceneResources()
    {
        auto *render_context = GetRenderContext();
        if(!render_context)
            return false;

        auto *graphics_context = render_context->GetGraphicsContext();
        if(!graphics_context)
            return false;

        auto *material_manager = graphics_context->GetMaterialManager();
        auto *geometry_manager = graphics_context->GetGeometryManager();
        auto *primitive_manager = graphics_context->GetPrimitiveManager();
        auto *device = graphics_context->GetDevice();
        if(!material_manager || !geometry_manager || !primitive_manager || !device)
            return false;

        auto *render_target = render_context->GetCurrentRenderTarget();
        auto *render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        if(!render_pass)
            return false;

        {
            mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);
            cfg.local_to_world = true;
            cfg.position_format = VAT_VEC2;

            grid_material = material_manager->LoadMaterial("Std3D/VertexLum3D", &cfg);
            if(!grid_material)
                return false;

            VILConfig vil_config;
            vil_config.Add(VAN::Luminance, VF_V1UN8);

            const Color4f white = GetColor4f(COLOR::White, 1.0f);
            grid_mi = material_manager->CreateMaterialInstance(grid_material, &vil_config, &white);
            if(!grid_mi)
                return false;

            grid_pipeline = render_pass->CreatePipeline(grid_mi, InlinePipeline::Solid3D);
            if(!grid_pipeline)
                return false;

            auto pc = std::make_unique<GeometryCreater>(device, grid_mi->GetVIL());

            inline_geometry::PlaneGridCreateInfo pgci;
            pgci.grid_size.Set(64, 64);
            pgci.sub_count.Set(8, 8);
            pgci.lum = 80;
            pgci.sub_lum = 128;

            grid_geometry = inline_geometry::CreatePlaneGrid2D(pc.get(), &pgci);
            if(!grid_geometry)
                return false;

            geometry_manager->Add(grid_geometry);

            grid_primitive = primitive_manager->CreatePrimitive(grid_geometry, grid_mi, grid_pipeline);
            if(!grid_primitive)
                return false;
        }

        {
            mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);

            mtl::MaterialCreateInfo *mci = mtl::CreateGizmo3D(device->GetDevAttr(), &cfg);
            if(!mci)
                return false;

            cube_material = material_manager->CreateMaterial("GizmoUsageCube", mci);
            if(!cube_material)
                return false;

            const Color4f blue = GetColor4f(COLOR::BlenderAxisBlue, 1.0f);
            cube_mi = material_manager->CreateMaterialInstance(cube_material, (VIL *)nullptr, &blue);
            if(!cube_mi)
                return false;

            cube_pipeline = render_pass->CreatePipeline(cube_material, InlinePipeline::Solid3D);
            if(!cube_pipeline)
                return false;

            auto pc = std::make_unique<GeometryCreater>(device, cube_material->GetDefaultVIL());

            inline_geometry::CubeCreateInfo cci;
            cci.segments_x = 2;
            cci.segments_y = 2;
            cci.segments_z = 2;

            cube_geometry = inline_geometry::CreateCube(pc.get(), &cci);
            if(!cube_geometry)
                return false;

            geometry_manager->Add(cube_geometry);

            cube_primitive = primitive_manager->CreatePrimitive(cube_geometry, cube_mi, cube_pipeline);
            if(!cube_primitive)
                return false;
        }

        return true;
    }

    bool InitSceneEntities()
    {
        if(!ecs_world || !grid_primitive || !cube_primitive || !gizmo_system)
            return false;

        plane_entity = ecs_world->CreateEntity<hgl::ecs::Entity>("PlaneGrid");
        if(!plane_entity)
            return false;

        auto plane_transform = plane_entity->AddComponent<hgl::ecs::TransformComponent>();
        plane_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
        plane_transform->SetMovable(false);

        auto plane_primitive_comp = plane_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        plane_primitive_comp->SetPrimitive(grid_primitive);
        plane_primitive_comp->SetVisible(true);

        cube_entity = ecs_world->CreateEntity<hgl::ecs::Entity>("Cube");
        if(!cube_entity)
            return false;

        auto cube_transform = cube_entity->AddComponent<hgl::ecs::TransformComponent>();
        cube_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(3.0f));
        cube_transform->SetMovable(true);

        auto cube_primitive_comp = cube_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        cube_primitive_comp->SetPrimitive(cube_primitive);
        cube_primitive_comp->SetVisible(true);

        return true;
    }

    bool EnsureCameraSystem()
    {
        if(!ecs_world)
            return false;

        auto camera_system = ecs_world->GetSystem<hgl::ecs::CameraSystem>();
        if(!camera_system)
        {
            camera_system = ecs_world->RegisterTickSystem<hgl::ecs::CameraSystem>(ecs_world);
            if(ecs_world->IsActive())
            {
                camera_system->OnDependenciesReady();
                camera_system->Initialize();
            }
        }

        return camera_system != nullptr;
    }

    hgl::ecs::CameraSystem *GetCameraSystem() const
    {
        if(!ecs_world)
            return nullptr;

        return ecs_world->GetSystem<hgl::ecs::CameraSystem>().get();
    }

    bool InitCamera()
    {
        if(!EnsureCameraSystem())
            return false;

        auto *camera_system = GetCameraSystem();
        if(!camera_system)
            return false;

        camera_entity = ecs_world->CreateEntity<hgl::ecs::Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<hgl::ecs::CameraComponent>();

        camera->control_mode = hgl::ecs::CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 48.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = camera_system->GetCamera();
        camera->camera_info = const_cast<hgl::graph::CameraInfo *>(camera_system->GetCameraInfo());
        camera->viewport_info = camera_system->GetViewportInfo();

        return true;
    }

    bool InitGizmos()
    {
        if(!ecs_world)
            return false;

        gizmo_system = ecs_world->GetSystem<TransformGizmoSystem>();
        if(!gizmo_system)
            gizmo_system = ecs_world->RegisterTickSystem<TransformGizmoSystem>();

        if(!gizmo_system)
            return false;

        gizmo_system->SetDefaultMode(GizmoMode::MoveWorld);
        gizmo_system->SetModeSwitchEnabled(true);
        gizmo_system->SetInitialPosition(GizmoPosition);

        return true;
    }

    void UpdateDebug(hgl::ecs::InputSystem *input_system)
    {
        if(!input_system || !gizmo_system)
            return;

        GizmoMode mode = gizmo_system->GetCurrentMode();
        std::string text = "mode=";
        if(mode == GizmoMode::MoveWorld)
            text += "MoveWorld(1)";
        else if(mode == GizmoMode::MoveLocal)
            text += "MoveLocal(2)";
        else if(mode == GizmoMode::RotateWorld)
            text += "RotateWorld(3)";
        else if(mode == GizmoMode::RotateLocal)
            text += "RotateLocal(4)";
        else if(mode == GizmoMode::ScaleLocal)
            text += "ScaleLocal(5)";

        text += " left=";
        text += input_system->IsMouseButtonDown(hgl::io::MouseButton::Left) ? "1" : "0";

        if(text != debug_cache)
        {
            debug_cache = text;
            std::cout << text << std::endl;
        }
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        if(!InitSceneResources())
            return false;

        if(!InitGizmos())
            return false;

        if(!InitSceneEntities())
            return false;

        if(!gizmo_system || !gizmo_system->SetTargetEntity(cube_entity))
            return false;

        gizmo_system->SetChangedCallback([](const GizmoTransformChange &change)
                                         {
                                             std::cout << "gizmo changed mode=" << static_cast<int>(change.mode)
                                                       << " pos=(" << change.current_position.x << ", "
                                                       << change.current_position.y << ", "
                                                       << change.current_position.z << ")"
                                                       << std::endl;
                                         });

        if(!InitCamera())
            return false;

        return true;
    }

public:
    bool Init() override
    {
        if(!InitECS())
            return false;

        std::cout << "Gizmo Example Started." << std::endl;
        std::cout << "Press 1 MoveWorld, 2 MoveLocal, 3 RotateWorld, 4 RotateLocal, 5 ScaleLocal" << std::endl;

        return true;
    }
    ~GizmoExampleApp()
    {
        gizmo_system.reset();

        SAFE_CLEAR(cube_primitive)
        SAFE_CLEAR(cube_geometry)
        SAFE_CLEAR(grid_primitive)
        SAFE_CLEAR(grid_geometry)
    }

    void Tick(double delta) override
    {
        if(!ecs_world)
            return;

        auto input_system = ecs_world->GetSystem<hgl::ecs::InputSystem>();
        if(!input_system)
            return;

        UpdateDebug(input_system.get());
        WorkObject::Tick(delta);
    }
};

int os_main(int, os_char **)
{
    return RunFramework<GizmoExampleApp>(OS_TEXT("Gizmo Usage Example"), 1280, 720);
}

