#include<hgl/framework/WorkManager.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include"../common/SubWorldAnimatedGeometryModule.h"

using namespace hgl;
using namespace hgl::ecs;

class SubWorldBuiltinGeometryApp : public WorkObject
{
private:
    ECSContext* root_context = nullptr;
    Entity* camera_entity = nullptr;
    std::unique_ptr<example::modules::ISubWorldModule> geometry_module;

private:
    bool InitRootCamera()
    {
        if (!root_context || !root_context->EnsureCameraSystem())
            return false;

        camera_entity = root_context->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();
        if (!camera)
            return false;

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 8.5f;
        camera->yaw = 35.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo*>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();
        return true;
    }

public:
    bool Init() override
    {
        SetClearColor(Color4f(0.12f, 0.12f, 0.16f, 1.0f));

        root_context = GetECSContext();
        if (!root_context)
            return false;

        if (!InitRootCamera())
            return false;

        geometry_module = example::modules::CreateSubWorldAnimatedGeometryModule();
        if (!geometry_module)
            return false;

        if (!geometry_module->Mount(GetRenderContext(), root_context, "SubWorldRoot", SubWorldMode::IsolatedContext))
            return false;

        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<SubWorldBuiltinGeometryApp>(OS_TEXT("SubWorld Builtin Geometry (ECS)"), argc, argv, 1280, 720);
}
