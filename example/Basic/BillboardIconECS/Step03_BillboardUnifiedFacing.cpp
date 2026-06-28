// Step03: unified integrated billboard (always facing camera)
//
// Goal:
// - Keep the split architecture (Quad + Facing) through BillboardComponent.
// - Validate the integrated always-front-facing result with minimal setup.

#include <hgl/framework/WorkManager.h>
#include <hgl/graph/module/MaterialRecipeRegistry.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/PrimitiveManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/mtl/MaterialLibrary.h>
#include <hgl/vk/VKVertexInputConfig.h>
#include <hgl/color/Color.h>

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/ecs/components/BillboardComponent.h>
#include <hgl/ecs/components/CameraComponent.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include <hgl/ecs/systems/render/QuadMaterialBindingSystem.h>
#include <hgl/ecs/systems/transform/FacingTransformSystem.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

class Step03BillboardUnifiedFacingApp : public WorkObject
{
private:

    ECSContext *ecs_context = nullptr;
    Entity *camera_entity = nullptr;

private:

    bool EnsureRenderSystems()
    {
        if (!ecs_context)
            return false;

        auto quad_prepare = ecs_context->GetSystem<QuadResourcePrepareSystem>();
        if (!quad_prepare)
        {
            quad_prepare = ecs_context->RegisterRenderSystem<QuadResourcePrepareSystem>();
            quad_prepare->SetWorld(ecs_context);
            if (ecs_context->IsActive())
            {
                quad_prepare->OnDependenciesReady();
                quad_prepare->Initialize();
            }
        }

        auto quad_binding = ecs_context->GetSystem<QuadMaterialBindingSystem>();
        if (!quad_binding)
        {
            quad_binding = ecs_context->RegisterRenderSystem<QuadMaterialBindingSystem>();
            quad_binding->SetWorld(ecs_context);
            if (ecs_context->IsActive())
            {
                quad_binding->OnDependenciesReady();
                quad_binding->Initialize();
            }
        }

        auto facing_system = ecs_context->GetSystem<FacingTransformSystem>();
        if (!facing_system)
        {
            facing_system = ecs_context->RegisterTickSystem<FacingTransformSystem>();
            facing_system->SetWorld(ecs_context);
            facing_system->SetCameraInfo(GetCameraInfo());
            if (ecs_context->IsActive())
            {
                facing_system->OnDependenciesReady();
                facing_system->Initialize();
            }
        }

        return quad_prepare && quad_binding && facing_system;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        QuadResourcePrepareSystem::SetPresetForWorld(ecs_context, GraphicsPipelinePreset::Alpha3D);
        QuadResourcePrepareSystem::SetFixedSizeForWorld(ecs_context, true);

        if (!EnsureRenderSystems())
            return false;

        Entity *billboard_entity = ecs_context->CreateEntity<Entity>("UnifiedBillboard");
        if (!billboard_entity)
            return false;

        auto transform = billboard_entity->AddComponent<TransformComponent>(Mobility::Static);
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        auto billboard = billboard_entity->AddComponent<BillboardComponent>();
        billboard->SetVisible(true);
        billboard->SetFixedPixelSize(true);
        billboard->SetPixelSize(256, 256);
        billboard->SetFrontFace(VK_FRONT_FACE_CLOCKWISE);
        billboard->SetTexture(OS_TEXT("res/image/lena.Tex2D"));
        billboard->SetDomainTag("billboard_step03_unified");

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
        camera->distance = 24.0f;
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
        SetClearColor(Color4f(0.14f, 0.14f, 0.14f, 1.0f));

        if (!InitECS())
            return false;

        if (!InitCamera())
            return false;

        return true;
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<Step03BillboardUnifiedFacingApp>(OS_TEXT("Billboard Step03 - Unified Facing"), argc, argv, 1280, 720);
}
