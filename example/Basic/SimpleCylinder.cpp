// SimpleCylinder.cpp - 基于 SimpleCube.cpp 改为创建 Cylinder 的示例

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>

#include<hgl/color/Color.h>

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
    Entity *      cylinder_entity    =nullptr;
    Entity *      camera_entity  =nullptr;

    Material *          material        = nullptr;
    MaterialInstance *  mi              = nullptr;

    Geometry *          geometry        = nullptr;
    Primitive *         primitive       = nullptr;

private:

    bool InitMaterial()
    {
        static const mtl::MaterialAssetRecord kCylinderCfg {
            .id       = "cylinder_main",
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
        auto handle = registry.Acquire(kCylinderCfg);
        if (!handle.IsValid())
            return false;

        material = handle.material;

        Color4f color = GetColor4f(COLOR::BlenderAxisBlue, 1.0f);

        mi = registry.CreateMI(handle, kCylinderCfg, &color, sizeof(color));

        if(!mi)
            return false;

        return mi != nullptr;
    }

    bool CreateCylinderGeometry()
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

        CylinderCreateInfo cci;
        cci.halfExtend   = 0.5f;   // cylinder height = 1.0
        cci.radius       = 1.0f;
        cci.numberSlices = 6;      // very low tessellation (hexagonal prism look)

        geometry = CreateCylinder(pc.get(), &cci);

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

        cylinder_entity = ecs_context->CreateEntity<Entity>("CylinderEntity");

        auto transform = cylinder_entity->AddComponent<TransformComponent>(Mobility::Static);
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        auto primitive_comp = cylinder_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
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

        if(!CreateCylinderGeometry())
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
    return RunFramework<TestApp>(OS_TEXT("Simple Cylinder (ECS)"), argc, argv, 1280, 720);
}
