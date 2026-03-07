#include<hgl/ecs/components/AssetInstanceComponent.h>
#include<hgl/ecs/core/ComponentRecords.h>
#include<hgl/ecs/core/Entity.h>

namespace hgl::ecs
{
    const char* AssetInstanceComponent::GetSerializationType()
    {
        return "AssetInstance";
    }

    bool AssetInstanceComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                                   const hgl::UnorderedMap<EntityID, int32_t>&,
                                                   ComponentRecord& out_record)
    {
        auto asset_instance = std::dynamic_pointer_cast<AssetInstanceComponent>(component);
        if (!asset_instance)
            return false;

        AssetInstanceComponentRecord data{};
        data.asset_world_id = asset_instance->GetAssetWorldID();
        data.instance_id = asset_instance->GetInstanceID();
        data.expected_version = asset_instance->GetExpectedVersion();
        data.visibility_mask = asset_instance->GetVisibilityMask();
        data.flags = asset_instance->GetFlags();
        data.override_ref.payload_ref = asset_instance->GetOverrideRef().payload_ref;
        data.override_ref.revision = asset_instance->GetOverrideRef().revision;

        out_record.type = GetSerializationType();
        out_record.payload = data;
        return true;
    }

    void AssetInstanceComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                       Entity* entity,
                                                       std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&)
    {
        if (!entity)
            return;

        const auto& data = std::any_cast<const AssetInstanceComponentRecord&>(record.payload);

        auto component = std::make_shared<AssetInstanceComponent>();
        component->SetAssetWorldID(data.asset_world_id);
        component->SetInstanceID(data.instance_id);
        component->SetExpectedVersion(data.expected_version);
        component->SetVisibilityMask(data.visibility_mask);
        component->SetFlags(data.flags);

        AssetOverrideRef override_ref{};
        override_ref.payload_ref = data.override_ref.payload_ref;
        override_ref.revision = data.override_ref.revision;
        component->SetOverrideRef(override_ref);

        component->ClearAllChanges();
        entity->AddComponentInstance(component);
    }
}
