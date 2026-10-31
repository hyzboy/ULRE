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
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/mtl/MaterialResolver.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/SSBOBufferRegistry.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/color/Color.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
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
    // 材质数据行的唯一来源：一个 URI + 一个行号，材质数值全部在块文件里
    constexpr char   MATERIAL_DATA_URI[] =
        "mtldata:file:ShaderLibrary/asset/material_data/gizmo_usage.material_data.toml";
    constexpr uint32 MATERIAL_DATA_ROW = 0;
}

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

class GizmoUsageApp : public WorkObject
{
private:
    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;
    hgl::ecs::Entity *plane_entity = nullptr;
    hgl::ecs::Entity *cube_entity = nullptr;

    std::shared_ptr<TransformGizmoSystem> gizmo_system;

    graph::mtl::MaterialRecipe grid_recipe{};
    PrimitiveAsset             grid_asset{};
    graph::GlobalSSBOBinding grid_mtl_data_ssbo_accessor{};
    Geometry *grid_geometry = nullptr;

    graph::mtl::MaterialRecipe cube_recipe{};
    PrimitiveAsset             cube_asset{};
    graph::GlobalSSBOBinding cube_mtl_data_ssbo_accessor{};
    Geometry *cube_geometry = nullptr;

    std::string debug_cache;

    bool InitSceneResources()
    {
        auto *device = GetDevice();
        if (!device)
            return false;

        {
            inline_geometry::PlaneGridCreateInfo pgci;
            pgci.grid_size.Set(32, 32);
            pgci.sub_count.Set(8, 8);
            pgci.lum = 180;
            pgci.sub_lum = 255;

            const GeometryVertexFormat geometry_vertex_format = CreateVertexLuminance2DGeometryVertexFormat();
            grid_geometry = GetGeometry("geo:builtin:plane_grid_lod", &geometry_vertex_format);
            if(!grid_geometry)
                return false;

            if (!GetMaterialDataBinding(MATERIAL_DATA_URI, 0, grid_mtl_data_ssbo_accessor))
                return false;

            const mtl::MaterialRecipe *loaded_grid_recipe =
                GetRecipe("GizmoUsage.VertexLuminance");
            if (!loaded_grid_recipe)
                return false;
            grid_recipe = *loaded_grid_recipe;
            if (!(grid_recipe.material_ssbo_binding = grid_mtl_data_ssbo_accessor).IsValid())
                return false;
            grid_asset = PrimitiveAsset(grid_geometry, &grid_recipe, PrimitiveType::Lines);
        }

        {
            const GeometryVertexFormat geometry_vertex_format2 = CreateGizmo3DGeometryVertexFormat();
            inline_geometry::CubeCreateInfo cci;
            cci.segments_x = 2;
            cci.segments_y = 2;
            cci.segments_z = 2;

            cube_geometry = GetGeometry("geo:builtin:cube_2x2x2", &geometry_vertex_format2);
            if(!cube_geometry)
                return false;

            if (!GetMaterialDataBinding(MATERIAL_DATA_URI, 1, cube_mtl_data_ssbo_accessor))
                return false;

            const mtl::MaterialRecipe *loaded_cube_recipe =
                GetRecipe("GizmoUsage.DebugNormalColor");
            if (!loaded_cube_recipe)
                return false;
            cube_recipe = *loaded_cube_recipe;
            if (!(cube_recipe.material_ssbo_binding = cube_mtl_data_ssbo_accessor).IsValid())
                return false;
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

        auto plane_transform = ecs_context->GetTransform(ecs_context->CreateTransform(plane_entity->GetEntityID(), hgl::ecs::Mobility::Static));
        plane_transform.SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));

        auto plane_primitive_comp = plane_entity->GetContext()->GetOrCreateGeometryData(plane_entity);
        hgl::ecs::MaterialData *material_data_comp = plane_entity->GetContext()->GetOrCreateMaterialData(plane_entity);
        plane_primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(plane_primitive_comp->GetOwnerID())->SetPrimitiveAsset(&grid_asset);
        hgl::ecs::MaterialData::MaterialDataAuthoringResource plane_struct{};
        plane_struct = grid_mtl_data_ssbo_accessor;
        material_data_comp->SetDataResource(plane_struct);
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

        cube_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("Cube");
        if(!cube_entity)
            return false;

        auto cube_transform = ecs_context->GetTransform(ecs_context->CreateTransform(cube_entity->GetEntityID(), hgl::ecs::Mobility::Movable));
        cube_transform.SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(3.0f));

        auto cube_primitive_comp = cube_entity->GetContext()->GetOrCreateGeometryData(cube_entity);
        hgl::ecs::MaterialData *cube_material_data_comp = cube_entity->GetContext()->GetOrCreateMaterialData(cube_entity);
        cube_primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(cube_primitive_comp->GetOwnerID())->SetPrimitiveAsset(&cube_asset);
        hgl::ecs::MaterialData::MaterialDataAuthoringResource cube_struct{};
        cube_struct = cube_mtl_data_ssbo_accessor;
        cube_material_data_comp->SetDataResource(cube_struct);
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

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
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = hgl::ecs::CameraControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 48.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->viewport_info = camera_system->GetViewportInfo();

        return true;
    }

    bool InitGizmos()
    {
        if(!ecs_context)
            return false;

        gizmo_system = ecs_context->GetSystem<TransformGizmoSystem>();
        if(!gizmo_system)
            gizmo_system = ecs_context->RegisterSystem<TransformGizmoSystem>();

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
    ~GizmoUsageApp()
    {
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
    return RunFramework<GizmoUsageApp>(OS_TEXT("Gizmo Usage"), argc, argv, 1280, 720);
}
