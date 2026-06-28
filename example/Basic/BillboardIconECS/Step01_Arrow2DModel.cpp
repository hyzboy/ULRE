// Step01: 2D arrow model display (debug baseline)
//
// Goal:
// - Validate pure 2D model rendering path first.
// - Use a simple arrow made of line segments.

#include <hgl/framework/WorkManager.h>
#include <hgl/graph/module/MaterialRecipeRegistry.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/PrimitiveManager.h>
#include <hgl/mtl/MaterialLibrary.h>
#include <hgl/vk/VKVertexInputConfig.h>
#include <hgl/color/Color.h>

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/ecs/components/PrimitiveComponent.h>
#include <hgl/ecs/components/CameraComponent.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    static const float kArrow2DPos[] =
    {
        -0.8f,  0.0f,
         0.6f,  0.0f,
         0.2f,  0.3f,
         0.2f, -0.3f,
    };

    static const uint8_t kArrowLum[] =
    {
        180, 255, 220, 220
    };

    static const uint16_t kArrowLines[] =
    {
        0, 1,
        1, 2,
        1, 3,
    };

    static Color4f kWhite(1, 1, 1, 1);
}

class Step01Arrow2DModelApp : public WorkObject
{
private:

    ECSContext *ecs_context = nullptr;
    Entity *camera_entity = nullptr;

    Geometry *arrow_geometry = nullptr;

    inline static const mtl::MaterialRecipe kArrow2DCfg {
        .id       = "billboard_step01_arrow2d",
        .preset   = mtl::MaterialPreset::VertexLuminance2D,
        .prim     = PrimitiveType::Lines,
        .pipeline = GraphicsPipelinePreset::Solid3D,
    };

private:

    bool CreateArrowGeometry()
    {
        arrow_geometry = WorkObject::CreateGeometry("Step01Arrow2D",
                                                    4,
                                                    6,
                                                    IndexType::U16,
                                                    {
                                                        {VAN::Position,  VF_V2F,    kArrow2DPos},
                                                        {VAN::Luminance, VF_V1UN8,  kArrowLum}
                                                    },
                                                    kArrowLines);

        return arrow_geometry != nullptr;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        Entity *arrow_entity = ecs_context->CreateEntity<Entity>("Arrow2D");
        if (!arrow_entity)
            return false;

        auto transform = arrow_entity->AddComponent<TransformComponent>(Mobility::Static);
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(10.0f, 10.0f, 1.0f));
        transform->SetMovable(false);

        auto primitive = arrow_entity->AddComponent<PrimitiveComponent>();
        primitive->SetUnresolvedGeometry(arrow_geometry);
        primitive->SetMaterialRecipe(RegisterMaterialRecipe(kArrow2DCfg), &kWhite, sizeof(kWhite));
        primitive->SetVisible(true);

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
        camera->distance = 10.0f;
        camera->yaw = 0.0f;
        camera->pitch = -89.0f;
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
        SetClearColor(Color4f(0.12f, 0.12f, 0.12f, 1.0f));

        if (!CreateArrowGeometry())
            return false;

        if (!InitECS())
            return false;

        if (!InitCamera())
            return false;

        return true;
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<Step01Arrow2DModelApp>(OS_TEXT("Billboard Step01 - Arrow2D Model"), argc, argv, 1280, 720);
}
