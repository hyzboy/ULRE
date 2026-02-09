#include<hgl/ecs/BoundingBoxUpdateSystem.h>
#include<hgl/ecs/BoundingBoxComponent.h>
#include<hgl/ecs/Context.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/TransformSystem.h>
#include<hgl/math/geometry/AABB.h>

namespace hgl::ecs
{
    BoundingBoxUpdateSystem::BoundingBoxUpdateSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::BoundingBox);
        SetExecutionOrder(15);
        AddDependency<TransformSystem>();
    }

    void BoundingBoxUpdateSystem::Update(float /*deltaTime*/)
    {
        if (!world || !update_enabled)
            return;

        std::vector<std::shared_ptr<BoundingBoxComponent>> bboxes;
        world->GetComponents<BoundingBoxComponent>(bboxes);

        const uint32_t update_mask = static_cast<uint32_t>(BoundingBoxComponent::BoundingBoxChange::MinMax) |
                                     static_cast<uint32_t>(BoundingBoxComponent::BoundingBoxChange::CenterExtents) |
                                     static_cast<uint32_t>(BoundingBoxComponent::BoundingBoxChange::WorldAABB);

        for (const auto& bbox : bboxes)
        {
            if (!bbox)
                continue;

            if (!ShouldProcess(bbox, update_mask))
            {
                MarkSeen(bbox);
                continue;
            }

            Entity* owner = bbox->GetOwner();
            if (!owner)
            {
                MarkSeen(bbox);
                continue;
            }

            auto primitive = owner->GetComponent<PrimitiveComponent>();
            if (!primitive)
            {
                MarkSeen(bbox);
                continue;
            }

            hgl::math::AABB local_aabb;
            if (!primitive->GetLocalAABB(local_aabb))
            {
                MarkSeen(bbox);
                continue;
            }

            const auto current = bbox->GetAABB();
            if (current.GetMin() != local_aabb.GetMin() || current.GetMax() != local_aabb.GetMax())
            {
                bbox->SetAABB(local_aabb);
            }

            MarkSeen(bbox);
        }
    }

    bool BoundingBoxUpdateSystem::ShouldProcess(const std::shared_ptr<BoundingBoxComponent>& bbox, uint32_t update_mask)
    {
        if (!bbox)
            return false;

        const uint64_t version = bbox->GetVersion();
        if (version == 0)
            return true;

        if ((bbox->GetChangeMask() & update_mask) == 0)
            return false;

        auto it = last_seen_version.find(bbox.get());
        if (it != last_seen_version.end() && it->second == version)
            return false;

        return true;
    }

    void BoundingBoxUpdateSystem::MarkSeen(const std::shared_ptr<BoundingBoxComponent>& bbox)
    {
        if (!bbox)
            return;

        last_seen_version[bbox.get()] = bbox->GetVersion();
    }
}//namespace hgl::ecs
