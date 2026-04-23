// SimpleCylinder.cpp - 基于 SimpleCube.cpp 改为创建 Cylinder 的示例

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/graph/module/GeometryManager.h>

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

    Geometry *          geometry        = nullptr;

    inline static const mtl::MaterialRecipe kCylinderCfg {
        .id       = "cylinder_main",
        .preset   = mtl::MaterialPreset::Gizmo3D,
        .pipeline = GraphicsPipelinePreset::Solid3D,
    };

private:

    bool CreateCylinderGeometry()
    {
        using namespace inline_geometry;

        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        CylinderCreateInfo cci;
        cci.halfExtend   = 0.5f;   // cylinder height = 1.0
        cci.radius       = 1.0f;
        cci.numberSlices = 6;      // very low tessellation (hexagonal prism look)

        GeometryVertexFormat gvf;
        gvf.Set(VAN::Position, VF_V3F);
        gvf.Set(VAN::Normal, VF_V3F);

        GraphicsGeometryFactory geometry_factory(graphics_context);
        auto pc = geometry_factory.CreateCreater(gvf);
        if (!pc) return false;

        geometry = CreateCylinder(pc.get(), &cci);
        if (!geometry) return false;

        return geometry_factory.RegisterGeometry(geometry) != nullptr;
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

        Color4f color = GetColor4f(COLOR::BlenderAxisBlue, 1.0f);

        auto primitive_comp = cylinder_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        primitive_comp->SetUnresolvedGeometry(geometry);
        primitive_comp->SetMaterialRecipe(RegisterMaterialRecipe(kCylinderCfg), &color, sizeof(color));
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
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if(!CreateCylinderGeometry())
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

