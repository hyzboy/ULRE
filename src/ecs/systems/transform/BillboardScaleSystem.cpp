#include<hgl/ecs/systems/transform/BillboardScaleSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/BillboardScaleComponent.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/camera/ViewportInfo.h>
#include<cmath>

namespace hgl::ecs
{
    BillboardScaleSystem::BillboardScaleSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::Transform);
        SetExecutionOrder(ExecutionPhase::TickPostCamera);
        AddDependency<TransformSystem>();
        AddDependency<CameraSystem>();
    }

    void BillboardScaleSystem::Update(float deltaTime)
    {
        if (!world)
            return;

        std::vector<Entity*> entities;
        world->GetAllEntities(entities);

        for (Entity* entity : entities)
        {
            if (!entity)
                continue;

            auto scale = entity->GetComponent<BillboardScaleComponent>();
            if (!scale || !scale->IsEnabled())
                continue;

            auto transform = entity->GetComponent<TransformComponent>();
            if (!transform)
                continue;

            UpdateBillboardScale(scale.get(), transform.get());
        }
    }

    bool BillboardScaleSystem::UpdateBillboardScale(BillboardScaleComponent* scale,
                                                    TransformComponent* transform)
    {
        if (!scale || !transform)
            return false;

        if (scale->GetScaleMode() == BillboardScaleMode::WorldSize)
        {
            const glm::vec2 world_size = scale->GetWorldSize();
            glm::vec3 desired_scale(world_size.x, world_size.y, 1.0f);
            const glm::vec3 current_scale = transform->GetLocalScale();

            if (std::fabs(current_scale.x - desired_scale.x) > 1e-5f
             || std::fabs(current_scale.y - desired_scale.y) > 1e-5f
             || std::fabs(current_scale.z - desired_scale.z) > 1e-5f)
            {
                transform->SetLocalScale(desired_scale);
            }

            scale->ClearScaleDirty();
            return true;
        }

        if (!camera_info || !viewport_info)
            return false;

        const auto pixel_size = scale->GetPixelSize();
        const float pixel_diameter = static_cast<float>((pixel_size.x > pixel_size.y) ? pixel_size.x : pixel_size.y);
        if (pixel_diameter <= 0.0f)
            return false;

        const bool updated = transform->ApplyFixedPixelUniformScale(camera_info,
                                                                    viewport_info,
                                                                    pixel_diameter,
                                                                    scale->GetReferenceWorldDiameter(),
                                                                    scale->GetMinUniformScale());
        if (updated)
            scale->ClearScaleDirty();

        return updated;
    }
}//namespace hgl::ecs
