#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/ecs/core/AssetTypes.h>
#include<hgl/type/UnorderedMap.h>
#include<cstdint>
#include<vector>
#include<utility>

namespace hgl::ecs
{
    struct ComponentRecord;
    class TransformComponent;

    struct AssetOverrideRef
    {
        uint64_t payload_ref = 0;
        uint32_t revision = 0;
    };

    class AssetInstanceComponent final : public Component
    {
    private:
        AssetWorldId asset_world_id = 0;
        InstanceId instance_id = 0;
        AssetVersion expected_version = 0;
        uint64_t visibility_mask = ~0ull;
        uint32_t flags = 0;
        AssetOverrideRef override_ref{};

    public:
        explicit AssetInstanceComponent(const std::string& name = "AssetInstance")
            : Component(name)
        {
        }

        AssetWorldId GetAssetWorldID() const { return asset_world_id; }
        void SetAssetWorldID(AssetWorldId id)
        {
            if (asset_world_id == id)
                return;

            asset_world_id = id;
            TouchChange(0x1u);
        }

        InstanceId GetInstanceID() const { return instance_id; }
        void SetInstanceID(InstanceId id)
        {
            if (instance_id == id)
                return;

            instance_id = id;
            TouchChange(0x2u);
        }

        AssetVersion GetExpectedVersion() const { return expected_version; }
        void SetExpectedVersion(AssetVersion version)
        {
            if (expected_version == version)
                return;

            expected_version = version;
            TouchChange(0x4u);
        }

        uint64_t GetVisibilityMask() const { return visibility_mask; }
        void SetVisibilityMask(uint64_t mask)
        {
            if (visibility_mask == mask)
                return;

            visibility_mask = mask;
            TouchChange(0x8u);
        }

        uint32_t GetFlags() const { return flags; }
        void SetFlags(uint32_t value)
        {
            if (flags == value)
                return;

            flags = value;
            TouchChange(0x10u);
        }

        const AssetOverrideRef& GetOverrideRef() const { return override_ref; }
        void SetOverrideRef(const AssetOverrideRef& value)
        {
            if (override_ref.payload_ref == value.payload_ref &&
                override_ref.revision == value.revision)
                return;

            override_ref = value;
            TouchChange(0x20u);
        }

        static const char* GetSerializationType();
        static bool SerializeToRecord(const std::shared_ptr<Component>& component,
                                      const hgl::UnorderedMap<EntityID, int32_t>& entity_index,
                                      ComponentRecord& out_record);
        static void DeserializeFromRecord(const ComponentRecord& record,
                                          Entity* entity,
                                          std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents);
    };
}
