#include<hgl/ecs/CameraComponent.h>
#include<hgl/ecs/Entity.h>

namespace hgl
{
    namespace ecs
    {
        CameraComponent::CameraComponent(const std::string& name)
            : Component(name)
        {
        }

        void CameraComponent::SetController(std::unique_ptr<CameraController> ctrl)
        {
            if (controller)
            {
                controller->Shutdown();
            }

            controller = std::move(ctrl);

            if (controller)
            {
                controller->SetCameraComponent(this);
                controller->Initialize();
            }
        }

        void CameraComponent::RemoveController()
        {
            if (controller)
            {
                controller->Shutdown();
                controller.reset();
            }
        }

        void CameraComponent::OnAttach()
        {
            if (controller)
            {
                controller->Initialize();
            }
        }

        void CameraComponent::OnUpdate(float deltaTime)
        {
            if (controller)
            {
                controller->Update(deltaTime);
            }
        }

        void CameraComponent::OnDetach()
        {
            RemoveController();
        }
    }//namespace ecs
}//namespace hgl
