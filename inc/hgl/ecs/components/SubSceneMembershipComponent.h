#pragma once

#include<hgl/ecs/core/Component.h>
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
    class SubSceneMembershipComponent final : public Component
    {
    private:
        uint64_t subscene_id = 0;

    public:
        explicit SubSceneMembershipComponent(uint64_t id = 0, const std::string& name = "SubSceneMembership")
            : Component(name)
            , subscene_id(id)
        {
        }

        uint64_t GetSubsceneID() const { return subscene_id; }

        void SetSubsceneID(uint64_t id)
        {
            if (subscene_id == id)
                return;

            subscene_id = id;
            TouchChange(0x1u);
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
