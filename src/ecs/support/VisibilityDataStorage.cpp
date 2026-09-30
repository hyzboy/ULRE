#include<hgl/ecs/support/VisibilityDataStorage.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>

namespace hgl::ecs
{
    void VisibilityDataStorage::SetInvisible(EntityID entity_id)
    {
        if (!entity_id.IsValid())
            return;

        std::lock_guard<std::mutex> lock(mutex);
        invisible_entities.insert(entity_id);
    }

    void VisibilityDataStorage::SetVisible(EntityID entity_id)
    {
        if (!entity_id.IsValid())
            return;

        std::lock_guard<std::mutex> lock(mutex);
        invisible_entities.erase(entity_id);
    }

    bool VisibilityDataStorage::IsDirectlyInvisible(EntityID entity_id) const
    {
        if (!entity_id.IsValid())
            return false;

        std::lock_guard<std::mutex> lock(mutex);
        return invisible_entities.count(entity_id) > 0;
    }

    bool VisibilityDataStorage::IsInvisible(EntityID entity_id) const
    {
        if (!entity_id.IsValid())
            return false;

        // Check if directly invisible
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (invisible_entities.count(entity_id) > 0)
                return true;
        }

        // Check ancestor chain for hierarchical visibility
        if (!context)
            return false;

        Entity* entity = context->GetEntity(entity_id);
        while (entity)
        {
            const TransformAccessor transform = context->GetTransformByEntity(entity->GetEntityID());
            if (!transform.IsValid())
                break;

            const TransformID parent_transform = transform.GetParent();
            if (!IsValidTransformID(parent_transform))
                break;

            const EntityID parent_id = context->GetTransformStorage()->GetOwner(parent_transform);
            if (!parent_id.IsValid())
                break;

            // Check if parent is invisible
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (invisible_entities.count(parent_id) > 0)
                    return true;
            }

            entity = context->GetEntity(parent_id);
        }

        return false;
    }

    size_t VisibilityDataStorage::GetInvisibleCount() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return invisible_entities.size();
    }

    void VisibilityDataStorage::Clear()
    {
        std::lock_guard<std::mutex> lock(mutex);
        invisible_entities.clear();
    }
}//namespace hgl::ecs

