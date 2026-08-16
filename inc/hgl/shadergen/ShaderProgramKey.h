#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/type/String.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstdint>

namespace hgl::graph::shadergen
{
    using namespace hgl::graph::mtl;
    struct ShaderProgramKey
    {
        uint64 vertex_stage_digest = 0;
        uint64 fragment_stage_digest = 0;
        uint64 resource_layout_hash = 0;
        uint64 vertex_input_hash = 0;
        uint64 pipeline_state_hash = 0;
        uint64 render_target_hash = 0;
        uint64 compiler_hash = 0;

        uint64 GetDigest() const noexcept
        {
            hgl::hash::FNV1aHasher64 h;

            h << vertex_stage_digest
              << fragment_stage_digest
              << resource_layout_hash
              << vertex_input_hash
              << pipeline_state_hash
              << render_target_hash
              << compiler_hash;

            return h;
        }

        AnsiString ToString() const
        {
            return AnsiString("program-") + AnsiString::numberOf(GetDigest());
        }

        bool operator==(const ShaderProgramKey &rhs) const noexcept
        {
            return vertex_stage_digest == rhs.vertex_stage_digest
                && fragment_stage_digest == rhs.fragment_stage_digest
                && resource_layout_hash == rhs.resource_layout_hash
                && vertex_input_hash == rhs.vertex_input_hash
                && pipeline_state_hash == rhs.pipeline_state_hash
                && render_target_hash == rhs.render_target_hash
                && compiler_hash == rhs.compiler_hash;
        }

        bool operator<(const ShaderProgramKey &rhs) const noexcept
        {
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
