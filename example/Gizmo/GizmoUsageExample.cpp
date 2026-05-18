/*
 TransformGizmoSystem 使用示例

 展示如何使用 TransformGizmoSystem 控制物体变换
 支持 5 种模式：MoveWorld, MoveLocal, RotateWorld, RotateLocal, ScaleLocal
 按键 1/2/3/4/5 切换对应模式
*/

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/gizmo/TransformGizmoSystem.h>
#include<hgl/math/VectorTypes.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/graph/module/GeometryManager.h>
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
    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;
    hgl::ecs::Entity *plane_entity = nullptr;
    hgl::ecs::Entity *cube_entity = nullptr;

    std::shared_ptr<TransformGizmoSystem> gizmo_system;

    Geometry *grid_geometry = nullptr;
    Geometry *cube_geometry = nullptr;

    inline static const mtl::MaterialRecipe kGridCfg {
        .id            = "gizmo_grid",
        .preset        = mtl::MaterialPreset::VertexLuminance,
        .prim          = PrimitiveType::Lines,
        .vertex_input  = mtl::VertexInputProfile::PositionLuminance2D,
        .vertex_policy = mtl::VertexTransformPolicy::Quad2D,
        .shading_model = mtl::SurfaceShadingModel::VertexLuminance,
        .schema        = mtl::ShaderDataSchema::Color4f,
        .has_explicit_schema = true,
        .pipeline      = GraphicsPipelinePreset::Solid3D,
    };

    inline static const mtl::MaterialRecipe kCubeCfg {
        .id            = "gizmo_cube",
        .preset        = mtl::MaterialPreset::Gizmo3D,
        .vertex_input  = mtl::VertexInputProfile::PositionNormal3D,
        .vertex_policy = mtl::VertexTransformPolicy::Mesh3D,
        .shading_model = mtl::SurfaceShadingModel::Gizmo,
        .schema        = mtl::ShaderDataSchema::Color4f,
        .has_explicit_schema = true,
        .pipeline      = GraphicsPipelinePreset::Solid3D,
    };

    Color4f grid_color;
    Color4f cube_color;

    std::string debug_cache;

    bool InitSceneResources()
    {

        auto *geometry_manager = GetGeometryManager();
        auto *device = GetDevice();
        if(!geometry_manager || !device)
            return false;
        {
            GeometryVertexFormat gvf;
            gvf.Set(VAN::Position, VF_V2F);
            gvf.Set(VAN::Luminance, VF_V1UN8);

            auto pc = std::make_unique<GeometryCreater>(device, gvf);

            inline_geometry::PlaneGridCreateInfo pgci;
            pgci.grid_size.Set(64, 64);
            pgci.sub_count.Set(8, 8);
            pgci.lum = 80;
            pgci.sub_lum = 128;

            grid_geometry = inline_geometry::CreatePlaneGrid2D(pc.get(), &pgci);
            if(!grid_geometry)
                return false;

            geometry_manager->Add(grid_geometry);
        }

        {
            GeometryVertexFormat gvf;
            gvf.Set(VAN::Position, VF_V3F);
            gvf.Set(VAN::Normal, VF_V3F);

            auto pc = std::make_unique<GeometryCreater>(device, gvf);

            inline_geometry::CubeCreateInfo cci;
            cci.segments_x = 2;
            cci.segments_y = 2;
            cci.segments_z = 2;

            cube_geometry = inline_geometry::CreateCube(pc.get(), &cci);
            if(!cube_geometry)
                return false;

            geometry_manager->Add(cube_geometry);
        }

        return true;
    }

    bool InitSceneEntities()
    {
        if(!ecs_context || !grid_geometry || !cube_geometry || !gizmo_system)
            return false;

        plane_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("PlaneGrid");
        if(!plane_entity)
            return false;

        auto plane_transform = plane_entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Static);
        plane_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));

        auto plane_primitive_comp = plane_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        plane_primitive_comp->SetUnresolvedGeometry(grid_geometry);
        plane_primitive_comp->SetMaterialRecipe(RegisterMaterialRecipe(kGridCfg), &grid_color, sizeof(grid_color));
        plane_primitive_comp->SetVisible(true);

        cube_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("Cube");
        if(!cube_entity)
            return false;

        auto cube_transform = cube_entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        cube_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(3.0f));

        auto cube_primitive_comp = cube_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        cube_primitive_comp->SetUnresolvedGeometry(cube_geometry);
        cube_primitive_comp->SetMaterialRecipe(RegisterMaterialRecipe(kCubeCfg), &cube_color, sizeof(cube_color));
        cube_primitive_comp->SetVisible(true);

        return true;
    }

    hgl::ecs::CameraSystem *GetCameraSystem() const
    {
        if(!ecs_context)
            return nullptr;

        return ecs_context->GetSystem<hgl::ecs::CameraSystem>().get();
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        auto *camera_system = GetCameraSystem();
        if(!camera_system)
            return false;

        camera_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("MainCamera");
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
        if(!ecs_context)
            return false;

        gizmo_system = ecs_context->GetSystem<TransformGizmoSystem>();
        if(!gizmo_system)
            gizmo_system = ecs_context->RegisterTickSystem<TransformGizmoSystem>();

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
        ecs_context = GetECSContext();
        if(!ecs_context)
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
        grid_color = GetColor4f(COLOR::White, 1.0f);
        cube_color = GetColor4f(COLOR::BlenderAxisBlue, 1.0f);

        if(!InitECS())
            return false;

        std::cout << "Gizmo Example Started." << std::endl;
        std::cout << "Press 1 MoveWorld, 2 MoveLocal, 3 RotateWorld, 4 RotateLocal, 5 ScaleLocal" << std::endl;

        return true;
    }
    ~GizmoExampleApp()
    {
        // Resources are owned by ECS/Graphics managers; avoid double-free on shutdown.
        gizmo_system.reset();
    }

    void Tick(double delta) override
    {
        if(!ecs_context)
            return;

        auto input_system = ecs_context->GetSystem<hgl::ecs::InputSystem>();
        if(!input_system)
            return;

        UpdateDebug(input_system.get());
        WorkObject::Tick(delta);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<GizmoExampleApp>(OS_TEXT("Gizmo Usage Example"), argc, argv, 1280, 720);
}


