#pragma once

namespace hgl::graph::mtl {}

#include <hgl/shadergen/ShaderStageKey.h>
#include <hgl/util/hash/FNV1a.h>

#include <string_view>

namespace hgl::graph::shadergen
{
    using namespace hgl::graph::mtl;
    inline uint64 HashFinalShaderSource(
        const char *source,
        const size_t length) noexcept
    {
        if (!source || length == 0)
            return 0;

        hgl::hash::FNV1aHasher64 h;
        h << std::string_view(source, length);
        return h;
    }

    inline ShaderStageKey BuildFinalShaderStageKey(
        const ShaderStage stage,
        const char *final_glsl,
        const size_t final_glsl_length,
        const uint64 module_graph_hash,
        const uint64 interface_hash,
        const uint64 resource_hash,
        const uint64 compiler_hash) noexcept
    {
        ShaderStageKey key{};
        key.stage = stage;
        key.definition_hash = HashFinalShaderSource(
            final_glsl, final_glsl_length);
        key.glsl_module_graph_hash = module_graph_hash;
        key.interface_hash = interface_hash;
        key.resource_hash = resource_hash;
        key.compiler_hash = compiler_hash;
        return key;
    }
}
