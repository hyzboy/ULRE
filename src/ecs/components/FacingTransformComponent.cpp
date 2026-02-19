#include<hgl/ecs/components/FacingTransformComponent.h>

namespace hgl::ecs
{
    void FacingTransformComponent::OnAttach()
    {
        // Called when component is attached to entity
        Component::OnAttach();
    }

    void FacingTransformComponent::OnUpdate(float deltaTime)
    {
        // Rotation calculation is handled by FacingTransformSystem
        Component::OnUpdate(deltaTime);
    }

    void FacingTransformComponent::OnDetach()
    {
        // Called when component is detached from entity
        Component::OnDetach();
    }

    const char* FacingTransformComponent::GetSerializationType()
    {
        return "FacingTransform";
    }

    bool FacingTransformComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                                     const hgl::UnorderedMap<EntityID, int32_t>& entity_index,
                                                     ComponentRecord& out_record)
    {
        // TODO: Implement serialization
        return true;
    }

    void FacingTransformComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                         Entity* entity,
                                                         std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents)
    {
        // TODO: Implement deserialization
    }
}//namespace hgl::ecs
