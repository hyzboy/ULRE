#include<hgl/ecs/components/AssetNodeMotionComponent.h>
#include<hgl/ecs/core/ComponentRecords.h>
#include<hgl/ecs/core/Entity.h>

namespace hgl::ecs
{
    const char* AssetNodeMotionComponent::GetSerializationType()
    {
        return "AssetNodeMotion";
    }

    bool AssetNodeMotionComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                                     const hgl::UnorderedMap<EntityID, int32_t>&,
                                                     ComponentRecord& out_record)
    {
        auto motion = std::dynamic_pointer_cast<AssetNodeMotionComponent>(component);
        if (!motion)
            return false;

        AssetNodeMotionComponentRecord data{};
        data.instance_id = motion->GetInstanceID();
        data.override_table_ref = motion->GetOverrideTableRef();
        data.table_revision = motion->GetTableRevision();

        out_record.type = GetSerializationType();
        out_record.payload = data;
        return true;
    }

    void AssetNodeMotionComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                         Entity* entity,
                                                         std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&)
    {
        if (!entity)
            return;

        const auto& data = std::any_cast<const AssetNodeMotionComponentRecord&>(record.payload);

        auto component = std::make_shared<AssetNodeMotionComponent>();
        component->SetInstanceID(data.instance_id);
        component->SetOverrideTableRef(data.override_table_ref);
        component->SetTableRevision(data.table_revision);
        component->ClearAllChanges();

        entity->AddComponentInstance(component);
    }
}
