// SimpleCube_AutoTransparency.cpp
//
// 3D semantic-material demo with runtime auto transparency decision in collect phase.

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
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    // User-facing switches for runtime auto transparency behavior.
    constexpr bool kEnableAutoTransparency = true;
    constexpr bool kUseRealAlpha3D = true;   // false => use Dither3D path
    constexpr float kAutoTransparencyNearDistance = 3.0f;
}

class TestApp:public WorkObject
{
private:

    ECSContext *  ecs_context = nullptr;
    Entity *      cube_entity = nullptr;
    Entity *      camera_entity = nullptr;

    Primitive *         primitive = nullptr;
    SemanticMaterialId  semantic_material_id = 0;

private:

    bool InitMaterial()
    {
        static const mtl::MaterialAssetRecord kCubeCfg {
            .id       = "cube_auto_transparency",
            .preset   = mtl::MaterialPreset::Gizmo3D,
            .pipeline = GraphicsPipelinePreset::Solid3D,
        };

        semantic_material_id = RegisterSemanticMaterial(kCubeCfg);
        return semantic_material_id != 0;
    }

    bool CreateCubePrimitive()
    {
        using namespace inline_geometry;

        CubeCreateInfo cci;
        cci.segments_x = 2;
        cci.segments_y = 3;
        cci.segments_z = 4;

        return (primitive = CreateComplexSemanticPrimitive(
            semantic_material_id,
            "Cube",
            graph::vfmt::kLitSurface,
            [&cci](graph::GeometryCreater* pc) { return CreateCube(pc, &cci); }
        )) != nullptr;
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

    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if(!InitMaterial())
            return false;

        if(!CreateCubePrimitive())
            return false;

        if(!InitECS())
            return false;

        if(!InitCamera())
            return false;

        if (auto render_collect = ecs_context->GetSystem<RenderPrimitiveCollectSystem>())
        {
            render_collect->SetSemanticRuntimeResolveEnabled(true);
            render_collect->SetAutoTransparencyEnabled(kEnableAutoTransparency);
            render_collect->SetUseRealAlpha3DEnabled(kUseRealAlpha3D);
            render_collect->SetAutoTransparencyNearDistance(kAutoTransparencyNearDistance);
        }

        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Simple Cube Auto Transparency (ECS)"), argc, argv, 1280, 720);
}
