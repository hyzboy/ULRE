#include<hgl/ecs/support/VisibilityDataStorage.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>

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
            auto transform = entity->GetComponent<TransformComponent>();
            if (!transform)
                break;

            EntityID parent_id = transform->GetParentID();
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

