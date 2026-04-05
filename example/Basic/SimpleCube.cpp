// 该范例主要演示使用ECS架构绘制一个立方体，并通过ECS CameraSystem使用ViewModel模式
// This example demonstrates rendering a cube with ECS and driving the camera via ViewModel mode
//
// 本范例展示了：
// 1. 使用ECS架构创建立方体实体
// 2. 使用TransformComponent管理空间变换
// 3. 使用PrimitiveComponent管理渲染图元
// 4. CameraSystem配置为ViewModel控制模式

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>

#include<hgl/color/Color.h>

// 引入ECS相关头文件
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

class TestApp:public WorkObject
{
private:

    ECSContext *  ecs_context      =nullptr;
    Entity *      cube_entity    =nullptr;
    Entity *      camera_entity  =nullptr;

    Material *          material        = nullptr;
    MaterialInstance *  mi              = nullptr;

    Geometry *          geometry        = nullptr;
    Primitive *         primitive       = nullptr;

private:

    bool InitMaterial()
    {
        static const mtl::MaterialAssetRecord kCubeCfg {
            .id       = "cube_main",
            .preset   = mtl::MaterialPreset::Gizmo3D,
            .pipeline = GraphicsPipelinePreset::Solid3D,
        };

        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        MaterialAssetRegistry registry(material_manager, nullptr, nullptr);
        Color4f color = GetColor4f(COLOR::BlenderAxisBlue, 1.0f);

        mi = registry.AcquireMI(kCubeCfg, &color, sizeof(color));

        if(!mi)
            return false;

        material = mi->GetMaterial();

        return mi != nullptr;
    }

    bool CreateCubeGeometry()
    {
        using namespace inline_geometry;

        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* geometry_manager = graphics_context->GetGeometryManager();
        if (!geometry_manager)
            return false;

        auto* device = graphics_context->GetDevice();
        if (!device)
            return false;

        auto pc = std::make_unique<GeometryCreater>(device, material->GetDefaultVIL());

        CubeCreateInfo cci;
        cci.segments_x = 2;
        cci.segments_y = 3;
        cci.segments_z = 4;

        geometry = CreateCube(pc.get(), &cci);

        if(!geometry)
            return false;

        geometry_manager->Add(geometry);
        return true;
    }

    bool InitPrimitive()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!primitive_manager)
            return false;

        primitive = primitive_manager->CreatePrimitive(geometry, mi);
        return primitive != nullptr;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if(!ecs_context)
            return false;

        cube_entity = ecs_context->CreateEntity<Entity>("CubeEntity");

        auto transform = cube_entity->AddComponent<TransformComponent>(Mobility::Static);
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        auto primitive_comp = cube_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        primitive_comp->SetPrimitive(primitive);
        primitive_comp->SetVisible(true);

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 6.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
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
        SAFE_CLEAR(primitive)
        SAFE_CLEAR(geometry)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if(!InitMaterial())
            return false;

        if(!CreateCubeGeometry())
            return false;

        if(!InitPrimitive())
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
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Simple Cube (ECS)"), argc, argv, 1280, 720);
}

