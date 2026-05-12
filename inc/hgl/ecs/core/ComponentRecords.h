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

    struct AssetOverrideRefRecord
    {
        uint64_t payload_ref = 0;
        uint32_t revision = 0;

        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(payload_ref, revision);
        }
    };

    struct AssetInstanceComponentRecord
    {
        uint64_t asset_world_id = 0;
        uint64_t instance_id = 0;
        uint32_t expected_version = 0;
        uint64_t visibility_mask = ~0ull;
        uint32_t flags = 0;
        AssetOverrideRefRecord override_ref{};

        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(asset_world_id,
               instance_id,
               expected_version,
               visibility_mask,
               flags,
               override_ref);
        }
    };

    struct AssetNodeMotionComponentRecord
    {
        uint64_t instance_id = 0;
        uint64_t override_table_ref = 0;
        uint32_t table_revision = 0;

        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(instance_id,
               override_table_ref,
               table_revision);
        }
    };
}
