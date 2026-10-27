#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/common/ShaderStageDef.h>
#include <hgl/type/String.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstdint>

namespace hgl::graph::mtl
{
    /// 一个 stage 的缓存身份 = 它**实际吃进去的全部输入** + 编译器 profile。
    ///
    /// 四个哈希按「失效时能一眼看出坏在哪」分工，互不掩盖：
    ///   · definition_hash     —— 主着色器文本本身，逐字节
    ///   · compile_inputs_hash —— 文本之外的**全部**输入：`#include` 闭包、
    ///                            注入的 define/sampler、options 文本
    ///                            （`generated/frag_options.glsl` 与
    ///                            `generated/mesh_options.glsl`）、资源/模板契约
    ///   · interface_hash      —— 接口形状（varying / 输出附件 / 领域+purpose+覆盖判定）
    ///   · compiler_hash       —— 编译器 profile（目标 Vulkan/SPIR-V + 代码生成代数）
    ///
    /// 历史包袱说明：options 摘要一度被编码成一行 `// domain-options-key <digest>`
    /// 注释拼在主着色器尾部，靠 definition_hash 顺带覆盖它。那是把「输入」伪装成
    /// 「文本」——读着色器的人会以为那行注释有语义，编译器只当垃圾，而任何按文本
    /// 比对的断言都得先剥掉它。现在 options 直接进 compile_inputs_hash，落盘的主
    /// 着色器就是纯粹的可编译 GLSL。
    struct ShaderStageKey
    {
        ShaderStage stage = ShaderStage::Mesh;
        uint64 definition_hash = 0;
        uint64 compile_inputs_hash = 0;
        uint64 interface_hash = 0;
        uint64 compiler_hash = 0;

        uint64 GetDigest() const noexcept
        {
            hgl::hash::FNV1aHasher64 h;

            h << stage
              << definition_hash
              << compile_inputs_hash
              << interface_hash
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
                && compile_inputs_hash == rhs.compile_inputs_hash
                && interface_hash == rhs.interface_hash
                && compiler_hash == rhs.compiler_hash;
        }

        bool operator<(const ShaderStageKey &rhs) const noexcept
        {
            if (stage != rhs.stage)
                return static_cast<uint32>(stage) < static_cast<uint32>(rhs.stage);
            if (definition_hash != rhs.definition_hash)
                return definition_hash < rhs.definition_hash;
            if (compile_inputs_hash != rhs.compile_inputs_hash)
                return compile_inputs_hash < rhs.compile_inputs_hash;
            if (interface_hash != rhs.interface_hash)
                return interface_hash < rhs.interface_hash;
            return compiler_hash < rhs.compiler_hash;
        }
    };
}
