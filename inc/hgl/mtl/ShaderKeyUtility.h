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

    /// 由**已算好的**三个哈希构造 stage key。
    ///
    /// 调用方（`GenericMaterialBuilder::AssembleProgramLinkFromSource`）传入的是
    /// Phase 4 发射出的文档文本摘要与其 `#include` 闭包指纹 —— 也就是说键覆盖
    /// 「发射出的全部字节」，不接受任何"声明里应该有但实际没发"的假设。
    inline ShaderStageKey BuildShaderStageKey(
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
}
