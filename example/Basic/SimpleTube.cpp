// SimpleTube.cpp - 基于示例创建一个低精度 Tube (用于 RenderDoc 导出测试)
// Demonstrates creating a Tube primitive via the inline geometry helper

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
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
    Entity *      tube_entity      =nullptr;
    Entity *      camera_entity    =nullptr;

    MaterialTemplate *          material        = nullptr;
    SemanticMaterialId  semantic_material_id = 0;

    Primitive *         primitive       = nullptr;
    bool                mi_color_initialized = false;

private:

    bool InitMaterial()
    {
        static const mtl::MaterialAssetRecord kTubeCfg {
            .id       = "tube_main",
            .preset   = mtl::MaterialPreset::Gizmo3D,
            .pipeline = GraphicsPipelinePreset::Solid3D,
        };

        semantic_material_id = RegisterSemanticMaterial(kTubeCfg);
        return semantic_material_id != 0;
    }

    bool CreateTubePrimitive()
    {
        using namespace inline_geometry;

        TubeCreateInfo tci;
        tci.length = 1.0f;
        tci.outer_radius = 1.0f;
        tci.inner_radius = 0.5f;
        tci.segments = 32;
        tci.generate_caps = true;

        return (primitive = CreateComplexSemanticPrimitive(
            semantic_material_id,
            "Tube",
            graph::vfmt::kLitSurface,
            [&tci](graph::GeometryCreater* pc) { return CreateTube(pc, &tci); }
        )) != nullptr;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if(!ecs_context)
            return false;

        tube_entity = ecs_context->CreateEntity<Entity>("TubeEntity");

        auto transform = tube_entity->AddComponent<TransformComponent>(Mobility::Static);
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        auto primitive_comp = tube_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        primitive_comp->SetPrimitive(primitive);
        primitive_comp->SetSemanticMaterial(semantic_material_id);
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

        if(!InitMaterial())
            return false;

        if(!CreateTubePrimitive())
            return false;

        if(!InitECS())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

    void Tick(double delta_time) override
    {
        if (!mi_color_initialized && primitive && primitive->GetMIData())
        {
            const Color4f tube_color = GetColor4f(COLOR::BlenderAxisGreen, 1.0f);
            primitive->WriteMIData(tube_color);
            mi_color_initialized = true;
        }

        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Simple Tube (ECS)"), argc, argv, 1280, 720);
}

