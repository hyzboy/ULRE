#include<hgl/ecs/components/SubSceneMembershipComponent.h>
#include<hgl/ecs/core/ComponentRecords.h>
#include<hgl/ecs/core/Entity.h>

namespace hgl::ecs
{
    const char* SubSceneMembershipComponent::GetSerializationType()
    {
        return "SubSceneMembership";
    }

    bool SubSceneMembershipComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                                        const hgl::UnorderedMap<EntityID, int32_t>&,
                                                        ComponentRecord& out_record)
    {
        auto membership = std::dynamic_pointer_cast<SubSceneMembershipComponent>(component);
        if (!membership)
            return false;

        SubSceneMembershipRecord data{};
        data.subscene_id = membership->GetSubsceneID();

        out_record.type = GetSerializationType();
        out_record.payload = data;
        return true;
    }

    void SubSceneMembershipComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                            Entity* entity,
                                                            std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&)
    {
        if (!entity)
            return;

        const auto& data = std::any_cast<const SubSceneMembershipRecord&>(record.payload);
        auto membership = std::make_shared<SubSceneMembershipComponent>(data.subscene_id);
        entity->AddComponentInstance(membership);
    }
}
