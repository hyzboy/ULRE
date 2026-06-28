// Step02: 2D model -> 3D mapping debug
//
// Goal:
// - Keep one 2D arrow shape.
// - Render it in 3D with three mapping modes:
//   XY -> XY0, XY -> YX0, XY -> 0YX.

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
        -1.0f,  0.0f,
         0.8f,  0.0f,
         0.3f,  0.35f,
         0.3f, -0.35f,
    };

    static const uint8_t kArrowLum[] =
    {
        200, 255, 220, 220
    };

    static const uint16_t kArrowLines[] =
    {
        0, 1,
        1, 2,
        1, 3,
    };

    static Color4f kAxisColorX(1.0f, 0.2f, 0.2f, 1.0f);
    static Color4f kAxisColorY(0.2f, 1.0f, 0.2f, 1.0f);
    static Color4f kAxisColorZ(0.2f, 0.5f, 1.0f, 1.0f);
}

class Step02Arrow2DTo3DAxesApp : public WorkObject
{
private:

    ECSContext *ecs_context = nullptr;
    Entity *camera_entity = nullptr;

    Geometry *arrow_xy_to_xy0 = nullptr;
    Geometry *arrow_xy_to_yx0 = nullptr;
    Geometry *arrow_xy_to_0yx = nullptr;

    inline static const mtl::MaterialRecipe kArrow3DCfg {
        .id       = "billboard_step02_arrow3d",
        .preset   = mtl::MaterialPreset::VertexLuminance3D,
        .prim     = PrimitiveType::Lines,
        .pipeline = GraphicsPipelinePreset::Solid3D,
    };

private:

    Geometry *CreateMappedArrowGeometry(const char *name,
                                        const int mapping_mode)
    {
        float pos3d[12] = {};

        for (int i = 0; i < 4; ++i)
        {
            const float x = kArrow2DPos[i * 2 + 0];
            const float y = kArrow2DPos[i * 2 + 1];

            float ox = 0.0f;
            float oy = 0.0f;
            float oz = 0.0f;

            switch (mapping_mode)
            {
            case 0: // XY -> XY0
                ox = x; oy = y; oz = 0.0f;
                break;
            case 1: // XY -> YX0  (shaft axis: +Y)
                ox = y; oy = x; oz = 0.0f;
                break;
            default: // XY -> 0YX  (shaft axis: +Z)
                ox = 0.0f; oy = y; oz = x;
                break;
            }

            pos3d[i * 3 + 0] = ox;
            pos3d[i * 3 + 1] = oy;
            pos3d[i * 3 + 2] = oz;
        }

        return WorkObject::CreateGeometry(name,
                                          4,
                                          6,
                                          IndexType::U16,
                                          {
                                              {VAN::Position,  VF_V3F,    pos3d},
                                              {VAN::Luminance, VF_V1UN8,  kArrowLum}
                                          },
                                          kArrowLines);
    }

    bool CreateGeometries()
    {
        arrow_xy_to_xy0 = CreateMappedArrowGeometry("Arrow_XY_to_XY0", 0);
        arrow_xy_to_yx0 = CreateMappedArrowGeometry("Arrow_XY_to_YX0", 1);
        arrow_xy_to_0yx = CreateMappedArrowGeometry("Arrow_XY_to_0YX", 2);

        return arrow_xy_to_xy0 && arrow_xy_to_yx0 && arrow_xy_to_0yx;
    }

    bool CreateArrowEntity(const char *name,
                           Geometry *geometry,
                           const Color4f &color)
    {
        Entity *entity = ecs_context->CreateEntity<Entity>(name);
        if (!entity)
            return false;

        auto transform = entity->AddComponent<TransformComponent>(Mobility::Static);
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(5.0f, 5.0f, 5.0f));
        transform->SetMovable(false);

        auto primitive = entity->AddComponent<PrimitiveComponent>();
        primitive->SetUnresolvedGeometry(geometry);
        primitive->SetMaterialRecipe(RegisterMaterialRecipe(kArrow3DCfg), &color, sizeof(Color4f));
        primitive->SetVisible(true);

        return true;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        if (!CreateArrowEntity("Arrow_XY_to_XY0", arrow_xy_to_xy0, kAxisColorX))
            return false;

        if (!CreateArrowEntity("Arrow_XY_to_YX0", arrow_xy_to_yx0, kAxisColorY))
            return false;

        if (!CreateArrowEntity("Arrow_XY_to_0YX", arrow_xy_to_0yx, kAxisColorZ))
            return false;

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
        camera->distance = 16.0f;
        camera->yaw = 45.0f;
        camera->pitch = -25.0f;
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

        if (!CreateGeometries())
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
    return RunFramework<Step02Arrow2DTo3DAxesApp>(OS_TEXT("Billboard Step02 - Arrow2D To 3D Axes"), argc, argv, 1280, 720);
}
