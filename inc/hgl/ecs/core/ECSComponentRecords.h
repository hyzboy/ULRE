#pragma once

#include <any>
#include <string>

namespace hgl::ecs
{
    // Opaque component serialization record
    // Used by component serializers to pass data to the context serializer
    struct ComponentRecord
    {
        std::string type;
        std::any payload;
    };
}
