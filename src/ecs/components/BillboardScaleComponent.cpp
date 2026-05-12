#include<hgl/ecs/components/BillboardScaleComponent.h>

namespace hgl::ecs
{
    void BillboardScaleComponent::OnAttach()
    {
        Component::OnAttach();
    }

    void BillboardScaleComponent::OnUpdate(float deltaTime)
    {
        Component::OnUpdate(deltaTime);
    }

    void BillboardScaleComponent::OnDetach()
    {
        Component::OnDetach();
    }

    const char* BillboardScaleComponent::GetSerializationType()
    {
        return "BillboardScale";
    }

    bool BillboardScaleComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                                    const hgl::UnorderedMap<EntityID, int32_t>& entity_index,
                                                    ComponentRecord& out_record)
    {
        return true;
    }

    void BillboardScaleComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                        Entity* entity,
                                                        std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents)
    {
    }
}//namespace hgl::ecs
