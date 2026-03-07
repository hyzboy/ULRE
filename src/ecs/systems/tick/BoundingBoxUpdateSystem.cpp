#include<hgl/ecs/systems/tick/BoundingBoxUpdateSystem.h>
#include<hgl/ecs/components/BoundingBoxComponent.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/math/geometry/AABB.h>
#include<glm/glm.hpp>

namespace hgl::ecs
{
    BoundingBoxUpdateSystem::BoundingBoxUpdateSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::BoundingBox);
        SetExecutionOrder(ExecutionPhase::TickTransform);
        AddDependency<TransformSystem>();
    }

    void BoundingBoxUpdateSystem::Update(float /*deltaTime*/)
    {
        if (!world || !update_enabled)
            return;

        std::vector<std::shared_ptr<BoundingBoxComponent>> bboxes;
        world->GetComponents<BoundingBoxComponent>(bboxes);

        const uint32_t bbox_update_mask = static_cast<uint32_t>(BoundingBoxComponent::BoundingBoxChange::MinMax) |
                          static_cast<uint32_t>(BoundingBoxComponent::BoundingBoxChange::CenterExtents) |
                          static_cast<uint32_t>(BoundingBoxComponent::BoundingBoxChange::WorldAABB);

        const uint32_t transform_update_mask = TransformComponent::ToChangeMask(TransformComponent::TransformChange::LocalTRS) |
                               TransformComponent::ToChangeMask(TransformComponent::TransformChange::Parent) |
                               TransformComponent::ToChangeMask(TransformComponent::TransformChange::WorldMatrix);

        for (const auto& bbox : bboxes)
        {
            if (!bbox)
                continue;

            Entity* owner = bbox->GetOwner();
            if (!owner)
            {
                MarkSeen(bbox, nullptr);
                continue;
            }

            if (!world->IsEntityTickEnabled(owner))
                continue;

            auto primitive = owner->GetComponent<PrimitiveComponent>();
            if (!primitive)
            {
                MarkSeen(bbox, nullptr);
                continue;
            }

            auto transform = owner->GetComponent<TransformComponent>();
            if (!ShouldProcess(bbox, transform, bbox_update_mask, transform_update_mask))
            {
                MarkSeen(bbox, transform);
                continue;
            }

            hgl::math::AABB local_aabb;
            if (!primitive->GetLocalAABB(local_aabb))
            {
                MarkSeen(bbox, transform);
                continue;
            }

            const auto current = bbox->GetAABB();
            if (current.GetMin() != local_aabb.GetMin() || current.GetMax() != local_aabb.GetMax())
            {
                bbox->SetAABB(local_aabb);
            }

            const glm::mat4 world_matrix = transform ? transform->GetWorldMatrix() : glm::mat4(1.0f);
            const auto world_aabb = local_aabb.Transformed(world_matrix);
            bbox->SetWorldAABB(world_aabb);

            MarkSeen(bbox, transform);
        }
    }

    bool BoundingBoxUpdateSystem::ShouldProcess(const std::shared_ptr<BoundingBoxComponent>& bbox,
                                                const std::shared_ptr<TransformComponent>& transform,
                                                uint32_t bbox_update_mask,
                                                uint32_t transform_update_mask)
    {
        if (!bbox)
            return false;

        const uint64_t bbox_version = bbox->GetVersion();
        bool bbox_new = false;
        if (bbox_version == 0)
        {
            bbox_new = true;
        }
        else
        {
            auto last_version = last_seen_version.GetValuePointer(bbox.get());
            bbox_new = (!last_version || *last_version != bbox_version);
        }

        const bool bbox_mask_match = (bbox->GetChangeMask() & bbox_update_mask) != 0;
        const bool bbox_changed = bbox_new && (bbox_mask_match || bbox_version == 0);

        bool transform_changed = false;
        if (transform)
        {
            const uint64_t transform_version = transform->GetVersion();
            if ((transform->GetChangeMask() & transform_update_mask) != 0)
            {
                const auto handle = transform->GetStorageHandle();
                auto last_version = last_seen_transform_version.GetValuePointer(handle);
                transform_changed = (!last_version || *last_version != transform_version);
            }
        }

        return bbox_changed || transform_changed;
    }

    void BoundingBoxUpdateSystem::MarkSeen(const std::shared_ptr<BoundingBoxComponent>& bbox,
                                           const std::shared_ptr<TransformComponent>& transform)
    {
        if (!bbox)
            return;

        last_seen_version[bbox.get()] = bbox->GetVersion();

        if (!transform)
            return;

        const auto handle = transform->GetStorageHandle();
        if (handle == TransformDataStorage::INVALID_HANDLE)
            return;

        last_seen_transform_version[handle] = transform->GetVersion();
    }
}//namespace hgl::ecs

