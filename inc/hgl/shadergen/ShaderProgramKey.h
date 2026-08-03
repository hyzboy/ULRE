#pragma once

#include <hgl/CoreType.h>
#include <hgl/type/String.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstdint>

namespace hgl::graph::mtl
{
    constexpr uint32 ShaderProgramKeySchemaVersion = 1;

    struct ShaderProgramKey
    {
        uint32 schema_version = ShaderProgramKeySchemaVersion;
        uint64 vertex_stage_digest = 0;
        uint64 fragment_stage_digest = 0;
        uint64 resource_layout_hash = 0;
        uint64 vertex_input_hash = 0;
        uint64 pipeline_state_hash = 0;
        uint64 render_target_hash = 0;
        uint64 compiler_hash = 0;

        uint64 GetDigest() const noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, schema_version);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, vertex_stage_digest);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, fragment_stage_digest);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, resource_layout_hash);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, vertex_input_hash);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, pipeline_state_hash);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, render_target_hash);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, compiler_hash);
            return hash;
        }

        AnsiString ToString() const
        {
            return AnsiString("program-") + AnsiString::numberOf(GetDigest());
        }

        bool operator==(const ShaderProgramKey &rhs) const noexcept
        {
            return schema_version == rhs.schema_version
                && vertex_stage_digest == rhs.vertex_stage_digest
                && fragment_stage_digest == rhs.fragment_stage_digest
                && resource_layout_hash == rhs.resource_layout_hash
                && vertex_input_hash == rhs.vertex_input_hash
                && pipeline_state_hash == rhs.pipeline_state_hash
                && render_target_hash == rhs.render_target_hash
                && compiler_hash == rhs.compiler_hash;
        }

        bool operator<(const ShaderProgramKey &rhs) const noexcept
        {
            if (schema_version != rhs.schema_version)
                return schema_version < rhs.schema_version;
            if (vertex_stage_digest != rhs.vertex_stage_digest)
                return vertex_stage_digest < rhs.vertex_stage_digest;
            if (fragment_stage_digest != rhs.fragment_stage_digest)
                return fragment_stage_digest < rhs.fragment_stage_digest;
            if (resource_layout_hash != rhs.resource_layout_hash)
                return resource_layout_hash < rhs.resource_layout_hash;
            if (vertex_input_hash != rhs.vertex_input_hash)
                return vertex_input_hash < rhs.vertex_input_hash;
            if (pipeline_state_hash != rhs.pipeline_state_hash)
                return pipeline_state_hash < rhs.pipeline_state_hash;
            if (render_target_hash != rhs.render_target_hash)
                return render_target_hash < rhs.render_target_hash;
            return compiler_hash < rhs.compiler_hash;
        }
    };
}
