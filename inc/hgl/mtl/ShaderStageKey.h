#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/common/ShaderStageDef.h>
#include <hgl/type/String.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstdint>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    struct ShaderStageKey
    {
        ShaderStage stage = ShaderStage::Mesh;
        uint64 definition_hash = 0;
        uint64 glsl_module_graph_hash = 0;
        uint64 interface_hash = 0;
        uint64 resource_hash = 0;
        uint64 compiler_hash = 0;

        uint64 GetDigest() const noexcept
        {
            hgl::hash::FNV1aHasher64 h;

            h << stage
              << definition_hash
              << glsl_module_graph_hash
              << interface_hash
              << resource_hash
              << compiler_hash;

            return h;
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
            return stage == rhs.stage
                && definition_hash == rhs.definition_hash
                && glsl_module_graph_hash == rhs.glsl_module_graph_hash
                && interface_hash == rhs.interface_hash
                && resource_hash == rhs.resource_hash
                && compiler_hash == rhs.compiler_hash;
        }

        bool operator<(const ShaderStageKey &rhs) const noexcept
        {
            if (stage != rhs.stage)
                return static_cast<uint32>(stage) < static_cast<uint32>(rhs.stage);
            if (definition_hash != rhs.definition_hash)
                return definition_hash < rhs.definition_hash;
            if (glsl_module_graph_hash != rhs.glsl_module_graph_hash)
                return glsl_module_graph_hash < rhs.glsl_module_graph_hash;
            if (interface_hash != rhs.interface_hash)
                return interface_hash < rhs.interface_hash;
            if (resource_hash != rhs.resource_hash)
                return resource_hash < rhs.resource_hash;
            return compiler_hash < rhs.compiler_hash;
        }
    };
}
