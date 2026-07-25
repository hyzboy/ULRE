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
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/DescriptorBindingSet.h>
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
        GeometryVertexFormat gvf;
        gvf.Add(VertexSemantic::Position, VF_V2F, 2, sizeof(float) * 2);
        gvf.Add(VertexSemantic::Luminance, VF_V1UN8, 1, sizeof(uint8));
        return gvf;
    }

    GeometryVertexFormat CreateGizmo3DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf;
        gvf.Add(VertexSemantic::Position, VF_V3F, 3, sizeof(float) * 3);
        gvf.Add(VertexSemantic::Normal, VF_V3F, 3, sizeof(float) * 3);
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

    Material *grid_material = nullptr;
    DescriptorBindingSet *grid_dbs = nullptr;
    graph::DeviceBuffer *grid_mi_ssbo = nullptr;
    Geometry *grid_geometry = nullptr;
    Primitive *grid_primitive = nullptr;

    Material *cube_material = nullptr;
    DescriptorBindingSet *cube_dbs = nullptr;
    graph::DeviceBuffer *cube_mi_ssbo = nullptr;
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

        {
            mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);
            cfg.local_to_world = true;
            const GeometryVertexFormat grid_gvf = CreateVertexLuminance2DGeometryVertexFormat();
            grid_material = material_manager->CreateMaterial(mtl::MaterialPreset::VertexLuminance3D, &cfg, grid_gvf);
            if(!grid_material)
                return false;

            // Grid uses per-instance color; create SSBO + DBS if material has MI data
            auto *buffer_manager = graphics_context->GetBufferManager();
            auto *domain_manager = graphics_context->GetResourceDomainManager();
            if (!buffer_manager || !domain_manager)
                return false;

            const Color4f white = GetColor4f(COLOR::White, 1.0f);
            const uint32_t stride = grid_material->GetMIDataBytes();
            if (stride > 0)
            {
                grid_mi_ssbo = buffer_manager->CreateSSBO("GizmoUsage:GridMIData", stride, nullptr, SharingMode::Exclusive);
                if (!grid_mi_ssbo)
                    return false;

                if (auto *gpu = grid_mi_ssbo->GetGPUBuffer())
                    gpu->Write(&white, 0, hgl_min(stride, static_cast<uint32_t>(sizeof(white))));

                for (const auto &req : grid_material->GetBindingContract().requirements)
                {
                    if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                        continue;
                    const graph::mtl::SSBOAddress addr{req.ssbo_type, req.ssbo_id, 0};
                    domain_manager->RegisterBuffer(addr, grid_mi_ssbo, 1);
                    grid_dbs = new DescriptorBindingSet(grid_material);
                    grid_dbs->SetSSBOBinding(req.ssbo_type, req.ssbo_id, 0);
                }
            }
            if (!grid_dbs)
                grid_dbs = new DescriptorBindingSet(grid_material);

            auto pc = std::make_unique<GeometryCreater>(
                device,
                CreateVertexLuminance2DGeometryVertexFormat());

            inline_geometry::PlaneGridCreateInfo pgci;
            pgci.grid_size.Set(64, 64);
            pgci.sub_count.Set(8, 8);
            pgci.lum = 80;
            pgci.sub_lum = 128;

            grid_geometry = inline_geometry::CreatePlaneGrid2D(pc.get(), &pgci);
            if(!grid_geometry)
                return false;

            geometry_manager->Add(grid_geometry);

            grid_primitive = primitive_manager->CreatePrimitive(grid_geometry, grid_material, grid_dbs, nullptr);
            if(!grid_primitive)
                return false;
        }

        {
            mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);

            cube_material = material_manager->CreateMaterial(mtl::MaterialPreset::Gizmo3D, &cfg);
            if(!cube_material)
                return false;

            auto *buffer_manager = graphics_context->GetBufferManager();
            auto *domain_manager = graphics_context->GetResourceDomainManager();
            if (!buffer_manager || !domain_manager)
                return false;

            const Color4f blue = GetColor4f(COLOR::BlenderAxisBlue, 1.0f);
            const uint32_t stride = cube_material->GetMIDataBytes();
            if (stride > 0)
            {
                cube_mi_ssbo = buffer_manager->CreateSSBO("GizmoUsage:CubeMIData", stride, nullptr, SharingMode::Exclusive);
                if (!cube_mi_ssbo)
                    return false;

                if (auto *gpu = cube_mi_ssbo->GetGPUBuffer())
                    gpu->Write(&blue, 0, hgl_min(stride, static_cast<uint32_t>(sizeof(blue))));

                for (const auto &req : cube_material->GetBindingContract().requirements)
                {
                    if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                        continue;
                    const graph::mtl::SSBOAddress addr{req.ssbo_type, req.ssbo_id, 0};
                    domain_manager->RegisterBuffer(addr, cube_mi_ssbo, 1);
                    cube_dbs = new DescriptorBindingSet(cube_material);
                    cube_dbs->SetSSBOBinding(req.ssbo_type, req.ssbo_id, 0);
                }
            }
            if (!cube_dbs)
                cube_dbs = new DescriptorBindingSet(cube_material);

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

            cube_primitive = primitive_manager->CreatePrimitive(cube_geometry, cube_material, cube_dbs, nullptr);
            if(!cube_primitive)
                return false;
        }

        return true;
    }

    bool InitSceneEntities()
    {
        if(!ecs_context || !grid_primitive || !cube_primitive || !gizmo_system)
            return false;

        plane_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("PlaneGrid");
        if(!plane_entity)
            return false;

        auto plane_transform = plane_entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Static);
        plane_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));

        auto plane_primitive_comp = plane_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        plane_primitive_comp->SetPrimitive(grid_primitive);
        plane_primitive_comp->RequestPipeline(InlinePipeline::Solid3D);
        plane_primitive_comp->SetVisible(true);

        cube_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("Cube");
        if(!cube_entity)
            return false;

        auto cube_transform = cube_entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        cube_transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(3.0f));

        auto cube_primitive_comp = cube_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        cube_primitive_comp->SetPrimitive(cube_primitive);
        cube_primitive_comp->RequestPipeline(InlinePipeline::Solid3D);
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
        delete grid_dbs;   grid_dbs = nullptr;
        delete cube_dbs;   cube_dbs = nullptr;
        SAFE_CLEAR(grid_mi_ssbo)
        SAFE_CLEAR(cube_mi_ssbo)
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
