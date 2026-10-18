#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/ShaderStageKey.h>
#include <hgl/util/hash/FNV1a.h>

#include <string_view>

namespace hgl::graph::mtl
{
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

    /// 由**已算好的**定义哈希构造 stage key。
    ///
    /// 与 BuildFinalShaderStageKey 的区别只在 definition_hash 的来源：那条路
    /// 现算最终 GLSL 文本的哈希，这条路吃调用方在 emit 之前算出的声明键。
    /// 运行时因此可以只跑声明阶段就得到与 ShaderGen 完全一致的 stage key。
    inline ShaderStageKey BuildDeclarationShaderStageKey(
        const ShaderStage stage,
        const uint64 definition_hash,
        const uint64 module_graph_hash,
        const uint64 interface_hash,
        const uint64 resource_hash,
        const uint64 compiler_hash) noexcept
    {
        ShaderStageKey key{};
        key.stage = stage;
        key.definition_hash = definition_hash;
        key.glsl_module_graph_hash = module_graph_hash;
        key.interface_hash = interface_hash;
        key.resource_hash = resource_hash;
        key.compiler_hash = compiler_hash;
        return key;
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
