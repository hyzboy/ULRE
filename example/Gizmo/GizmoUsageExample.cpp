/*
 TransformGizmoSystem 使用示例

 展示如何使用 TransformGizmoSystem 控制物体变换
 支持 5 种模式：MoveWorld, MoveLocal, RotateWorld, RotateLocal, ScaleLocal
 按键 1/2/3/4/5 切换对应模式
*/

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/gizmo/TransformGizmoSystem.h>
#include<hgl/math/VectorTypes.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
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

namespace
{
    GeometryVertexFormat CreateVertexLuminance2DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position,  VF_V2F},
            {VertexSemantic::Luminance, VF_V1UN8},
        };
        return gvf;
    }

    GeometryVertexFormat CreateGizmo3DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }
}

const math::Vector3f GizmoPosition(0, 0, 0);

class GizmoExampleApp : public WorkObject
{
private:
    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;
    hgl::ecs::Entity *plane_entity = nullptr;
    hgl::ecs::Entity *cube_entity = nullptr;

    std::shared_ptr<TransformGizmoSystem> gizmo_system;

    graph::mtl::MaterialRecipe grid_recipe{};
    PrimitiveAsset             grid_asset{};
    graph::SSBOArrayAccessor<Color4f>* grid_mtl_data_ssbo_accessor = nullptr;
    Geometry *grid_geometry = nullptr;

    graph::mtl::MaterialRecipe cube_recipe{};
    PrimitiveAsset             cube_asset{};
    graph::SSBOArrayAccessor<Color4f>* cube_mtl_data_ssbo_accessor = nullptr;
    Geometry *cube_geometry = nullptr;

    std::string debug_cache;

    bool InitSceneResources()
    {
        auto *geometry_manager = GetManager<GeometryManager>();
        auto *device = GetDevice();
        if(!geometry_manager || !device)
            return false;

        {
            inline_geometry::PlaneGridCreateInfo pgci;
            pgci.grid_size.Set(32, 32);
            pgci.sub_count.Set(8, 8);
            pgci.lum = 180;
            pgci.sub_lum = 255;

            auto pc = std::make_unique<GeometryCreater>(
                device,
                CreateVertexLuminance2DGeometryVertexFormat());

            grid_geometry = inline_geometry::CreatePlaneGrid2D(pc.get(), &pgci);
            if(!grid_geometry)
                return false;

            geometry_manager->Add(grid_geometry);

            auto *domain_manager = GetManager<ResourceDomainManager>();
            if (!domain_manager)
                return false;

            grid_mtl_data_ssbo_accessor = domain_manager->AllocateArrayAccessor<Color4f>(
                graph::mtl::SSBOType::EmissiveSurface,
                "GizmoUsage:GridMaterialData",
                1);
            if (!grid_mtl_data_ssbo_accessor)
                return false;

            (*grid_mtl_data_ssbo_accessor)[0] = GetColor4f(COLOR::White, 1.0f);
            grid_mtl_data_ssbo_accessor->Commit();

            grid_recipe.recipe_name = "GizmoUsageExample.VertexLuminance";
            grid_recipe.mtl_def_id = "VertexLuminance";
            grid_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
            grid_recipe.domain = "GizmoUsageExample";
            grid_recipe.vertex_node_config.input = graph::mtl::VertexInputMode::Vec2Position;
            grid_recipe.vertex_node_config.position_mapping = graph::mtl::PositionMappingMode::LiftXY_XY0;
            grid_recipe.vertex_node_config.orientation = graph::mtl::OrientationMode::World;
            grid_recipe.vertex_node_config.scale = graph::mtl::ScaleMode::World;
            grid_recipe.vertex_node_config.projection = graph::mtl::ProjectionMode::WorldCameraVP;
            grid_recipe.vertex_node_config.transport = graph::mtl::VertexTransportMode::SSBO;   // 材质 TOML transport=ssbo——recipe 显式覆盖需同步
            graph::mtl::UpsertRecipeSSBOAssetBinding(grid_recipe,
                                                     graph::mtl::DefaultMaterialDataSlotName,
                                                     grid_mtl_data_ssbo_accessor->GetSSBOBinding());
            grid_asset = PrimitiveAsset(grid_geometry, &grid_recipe, PrimitiveType::Lines);
        }

        {
            auto *domain_manager = GetManager<ResourceDomainManager>();
            if (!domain_manager)
                return false;

            auto pc = std::make_unique<GeometryCreater>(
                device,
                CreateGizmo3DGeometryVertexFormat());

            inline_geometry::CubeCreateInfo cci;
            cci.segments_x = 2;
            cci.segments_y = 2;
            cci.segments_z = 2;

            cube_geometry = inline_geometry::CreateCube(pc.get(), &cci);
            if(!cube_geometry)
                return false;

            geometry_manager->Add(cube_geometry);

            cube_mtl_data_ssbo_accessor = domain_manager->AllocateArrayAccessor<Color4f>(
                graph::mtl::SSBOType::EmissiveSurface,
                "GizmoUsage:CubeMaterialData",
                1);
            if (!cube_mtl_data_ssbo_accessor)
                return false;

            (*cube_mtl_data_ssbo_accessor)[0] = GetColor4f(COLOR::BlenderAxisBlue, 1.0f);
            cube_mtl_data_ssbo_accessor->Commit();

            cube_recipe.recipe_name = "GizmoUsageExample.DebugNormalColor";
            cube_recipe.mtl_def_id = "DebugNormalColor";
            cube_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
            cube_recipe.domain = "GizmoUsageExample";
            graph::mtl::UpsertRecipeSSBOAssetBinding(cube_recipe,
                                                     graph::mtl::DefaultMaterialDataSlotName,
                                                     cube_mtl_data_ssbo_accessor->GetSSBOBinding());
            cube_asset = PrimitiveAsset(cube_geometry, &cube_recipe, PrimitiveType::Triangles);
        }

        return true;
    }

    bool InitSceneEntities()
    {
        if(!ecs_context || !gizmo_system)
            return false;

        plane_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("PlaneGrid");
        if(!plane_entity)
            return false;

        auto plane_transform = plane_entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Static);
        plane_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));

        auto plane_primitive_comp = plane_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        plane_primitive_comp->SetPrimitiveAsset(&grid_asset);
        hgl::ecs::PrimitiveComponent::MaterialDataSlotAuthoringResource plane_struct{};
        plane_struct.data_slot_name = graph::mtl::DefaultMaterialDataSlotName;
        plane_struct.ssbo_id = grid_mtl_data_ssbo_accessor->GetSSBOId();
        plane_struct.data_index = 0;
        plane_struct.use_data_index = true;
        plane_struct.shared_across_instances = true;
        plane_primitive_comp->SetMaterialDataSlotResource(plane_struct);
        plane_primitive_comp->SetVisible(true);

        cube_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("Cube");
        if(!cube_entity)
            return false;

        auto cube_transform = cube_entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        cube_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(3.0f));

        auto cube_primitive_comp = cube_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        cube_primitive_comp->SetPrimitiveAsset(&cube_asset);
        hgl::ecs::PrimitiveComponent::MaterialDataSlotAuthoringResource cube_struct{};
        cube_struct.data_slot_name = graph::mtl::DefaultMaterialDataSlotName;
        cube_struct.ssbo_id = cube_mtl_data_ssbo_accessor->GetSSBOId();
        cube_struct.data_index = 0;
        cube_struct.use_data_index = true;
        cube_struct.shared_across_instances = true;
        cube_primitive_comp->SetMaterialDataSlotResource(cube_struct);
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
        if(!InitECS())
            return false;

        std::cout << "Gizmo Example Started." << std::endl;
        std::cout << "Press 1 MoveWorld, 2 MoveLocal, 3 RotateWorld, 4 RotateLocal, 5 ScaleLocal" << std::endl;

        return true;
    }
    ~GizmoExampleApp()
    {
        gizmo_system.reset();
        // graph modules own GPU resources and release them during graphics shutdown.
        // Keep raw pointers non-owning here to avoid teardown order double-free.
        SAFE_CLEAR(grid_mtl_data_ssbo_accessor)
        SAFE_CLEAR(cube_mtl_data_ssbo_accessor)
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
