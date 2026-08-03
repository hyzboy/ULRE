#pragma once

#include <hgl/CoreType.h>
#include <hgl/common/ShaderStageDef.h>
#include <hgl/type/String.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstdint>

namespace hgl::graph::mtl
{
    constexpr uint32 ShaderStageKeySchemaVersion = 1;

    struct ShaderStageKey
    {
        uint32 schema_version = ShaderStageKeySchemaVersion;
        ShaderStage stage = ShaderStage::Vertex;
        uint64 definition_hash = 0;
        uint64 interface_hash = 0;
        uint64 resource_hash = 0;
        uint64 compiler_hash = 0;

        uint64 GetDigest() const noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, schema_version);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, stage);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, definition_hash);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, interface_hash);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, resource_hash);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, compiler_hash);
            return hash;
        }

        AnsiString ToString() const
        {
            return AnsiString("stage-")
                 + AnsiString::numberOf(static_cast<uint32>(stage))
                 + AnsiString("-")
                 + AnsiString::numberOf(GetDigest());
        }

        bool operator==(const ShaderStageKey &rhs) const noexcept
        {
            return schema_version == rhs.schema_version
                && stage == rhs.stage
                && definition_hash == rhs.definition_hash
                && interface_hash == rhs.interface_hash
                && resource_hash == rhs.resource_hash
                && compiler_hash == rhs.compiler_hash;
        }

        bool operator<(const ShaderStageKey &rhs) const noexcept
        {
            if (schema_version != rhs.schema_version)
                return schema_version < rhs.schema_version;
            if (stage != rhs.stage)
                return static_cast<uint32>(stage) < static_cast<uint32>(rhs.stage);
            if (definition_hash != rhs.definition_hash)
                return definition_hash < rhs.definition_hash;
            if (interface_hash != rhs.interface_hash)
                return interface_hash < rhs.interface_hash;
            if (resource_hash != rhs.resource_hash)
                return resource_hash < rhs.resource_hash;
            return compiler_hash < rhs.compiler_hash;
        }
    };
}
