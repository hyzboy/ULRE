#include<hgl/ecs/VisibilityDataStorage.h>

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

    bool VisibilityDataStorage::IsInvisible(EntityID entity_id) const
    {
        if (!entity_id.IsValid())
            return false;

        std::lock_guard<std::mutex> lock(mutex);
        return invisible_entities.count(entity_id) > 0;
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
