#include<hgl/ecs/support/cull/VisibilityDataStorage.h>
#include<hgl/ecs/core/ECSCoreContext.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/support/transform/TransformDataStorage.h>

namespace hgl::ecs
{
    void VisibilityDataStorage::SetInvisible(EntityID entity_id)
    {
        if (!entity_id.IsValid())
            return;

        hgl::MutexLock lock(&mutex);
        invisible_entities.insert(entity_id);
    }

    void VisibilityDataStorage::SetVisible(EntityID entity_id)
    {
        if (!entity_id.IsValid())
            return;

        hgl::MutexLock lock(&mutex);
        invisible_entities.erase(entity_id);
    }

    bool VisibilityDataStorage::IsDirectlyInvisible(EntityID entity_id) const
    {
        if (!entity_id.IsValid())
            return false;

        hgl::MutexLock lock(&mutex);
        return invisible_entities.count(entity_id) > 0;
    }

    bool VisibilityDataStorage::IsInvisible(EntityID entity_id) const
    {
        if (!entity_id.IsValid())
            return false;

        // Check if directly invisible
        {
            hgl::MutexLock lock(&mutex);
            if (invisible_entities.count(entity_id) > 0)
                return true;
        }

        // Check ancestor chain for hierarchical visibility
        if (!core_context || !transform_storage)
            return false;

        Entity* entity = core_context->GetEntity(entity_id);
        while (entity)
        {
            const TransformID transform_id = transform_storage->FindByOwner(entity->GetEntityID());
            const TransformAccessor transform(transform_storage, transform_id);
            if (!transform.IsValid())
                break;

            const TransformID parent_transform = transform.GetParent();
            if (!IsValidTransformID(parent_transform))
                break;

            const EntityID parent_id = transform_storage->GetOwner(parent_transform);
            if (!parent_id.IsValid())
                break;

            // Check if parent is invisible
            {
                hgl::MutexLock lock(&mutex);
                if (invisible_entities.count(parent_id) > 0)
                    return true;
            }

            entity = core_context->GetEntity(parent_id);
        }

        return false;
    }

    size_t VisibilityDataStorage::GetInvisibleCount() const
    {
        hgl::MutexLock lock(&mutex);
        return invisible_entities.size();
    }

    void VisibilityDataStorage::Clear()
    {
        hgl::MutexLock lock(&mutex);
        invisible_entities.clear();
    }
}//namespace hgl::ecs

