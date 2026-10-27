#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/ShaderStageKey.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    /// 一段着色器文本（主着色器、options 文件）的逐字节摘要。空文本返回 0。
    inline uint64 HashFinalShaderSource(
        const char *source,
        const size_t length) noexcept
    {
        if (!source || length == 0)
            return 0;

        hgl::hash::FNV1aHasher64 h;
        h.AppendBytes(source, length);
        return h;
    }

    /// 由**已算好的**四个哈希构造 stage key。
    ///
    /// 调用方（`MaterialProgramAssembler::AssembleProgramLinkFromSource`）传入的是
    /// 最终编译文本的摘要、文本之外全部编译输入的摘要（`#include` 闭包 + 注入项 +
    /// options 文本 + 资源契约）、接口形状与编译器 profile —— 键覆盖「真正喂给编译器
    /// 的全部字节」，不接受任何「声明里应该有但实际没发」的假设。
    inline ShaderStageKey BuildShaderStageKey(
        const ShaderStage stage,
        const uint64 definition_hash,
        const uint64 compile_inputs_hash,
        const uint64 interface_hash,
        const uint64 compiler_hash) noexcept
    {
        ShaderStageKey key{};
        key.stage = stage;
        key.definition_hash = definition_hash;
        key.compile_inputs_hash = compile_inputs_hash;
        key.interface_hash = interface_hash;
        key.compiler_hash = compiler_hash;
        return key;
    }
}