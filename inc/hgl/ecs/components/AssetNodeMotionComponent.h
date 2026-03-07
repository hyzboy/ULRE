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
}

namespace hgl::ecs
{
    class AssetNodeMotionComponent final : public Component
    {
    private:
        InstanceId instance_id = 0;
        uint64_t override_table_ref = 0;
        uint32_t table_revision = 0;

    public:
        explicit AssetNodeMotionComponent(const std::string& name = "AssetNodeMotion")
            : Component(name)
        {
        }

        InstanceId GetInstanceID() const { return instance_id; }
        void SetInstanceID(InstanceId id)
        {
            if (instance_id == id)
                return;

            instance_id = id;
            TouchChange(0x1u);
        }

        uint64_t GetOverrideTableRef() const { return override_table_ref; }
        void SetOverrideTableRef(uint64_t ref)
        {
            if (override_table_ref == ref)
                return;

            override_table_ref = ref;
            TouchChange(0x2u);
        }

        uint32_t GetTableRevision() const { return table_revision; }
        void SetTableRevision(uint32_t revision)
        {
            if (table_revision == revision)
                return;

            table_revision = revision;
            TouchChange(0x4u);
        }

        void BumpTableRevision()
        {
            ++table_revision;
            TouchChange(0x4u);
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
