#pragma once

#include <any>
#include <string>
#include <cstdint>

namespace hgl::ecs
{
    // Opaque component serialization record
    // Used by component serializers to pass data to the context serializer
    struct ComponentRecord
    {
        std::string type;
        std::any payload;
    };

    struct EntityIDRecord
    {
        uint32_t index = UINT32_MAX;
        uint16_t generation = 0;

        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(index, generation);
        }
    };

    struct SubWorldComponentRecord
    {
        uint8_t mode = 0;
        bool render_shared = true;
        bool logic_isolated = false;
        uint64_t subscene_id = 0;
        EntityIDRecord root_entity_id{};
        bool paused = false;
        bool tick_enabled = true;
        bool render_enabled = true;
        std::string asset_path;
        bool asset_binary = false;

        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(mode,
               render_shared,
               logic_isolated,
               subscene_id,
               root_entity_id,
               paused,
               tick_enabled,
               render_enabled,
               asset_path,
               asset_binary);
        }
    };

    struct SubSceneMembershipRecord
    {
        uint64_t subscene_id = 0;

        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(subscene_id);
        }
    };
}
