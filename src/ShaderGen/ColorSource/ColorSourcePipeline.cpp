/// ColorSourcePipeline.cpp
///
/// 实现 FinalizeColorSources：G1 → AddRequirements → Allocate (G2)

#include <hgl/shadergen/ColorSourcePipeline.h>
#include <hgl/shadergen/ColorSourceValidator.h>
#include <hgl/shadergen/IColorSourceCodegen.h>
#include "BindingAllocator.h"
#include "CodegenRegistry.h"
#include <hgl/mtl/SamplerSlot.h>
#include <cstdio>

namespace hgl::graph
{

ColorSourcePipelineResult FinalizeColorSources(
    const std::vector<ColorSource> &sources,
    const char                     *debug_context,
    bool                            supports_descriptor_indexing)
{
    const char *ctx = debug_context ? debug_context : "FinalizeColorSources";

    ColorSourcePipelineResult result;

    // ── Bindless fallback ────────────────────────────────────────────────────
    // Rewrite BuiltinBindlessSamplerArray → BuiltinSampler2DArray when the device
    // does not support VK_EXT_descriptor_indexing.  We operate on a local copy so
    // callers never see the rewritten sources.
    std::vector<ColorSource> effective_sources = sources;
    if (!supports_descriptor_indexing)
    {
        for (auto &cs : effective_sources)
        {
            if (cs.kind == ColorSourceKind::BuiltinBindlessSamplerArray)
            {
                const std::string msg = std::string("[Bindless][") + ctx
                    + "] device does not support VK_EXT_descriptor_indexing; "
                      "falling back BuiltinBindlessSamplerArray → BuiltinSampler2DArray for slot "
                    + mtl::SamplerSlotNameList[uint8_t(cs.slot)];
                std::fprintf(stderr, "%s\n", msg.c_str());
                result.diags.push_back({ ColorSourcePipelineResult::Diag::Level::Warning, msg });
                cs.kind = ColorSourceKind::BuiltinSampler2DArray;
            }
        }
    }

    // ── G1: 结构合法性校验 ──────────────────────────────────────────────────
    const ColorSourceValidationResult g1 = ValidateColorSources(effective_sources);
    if (!g1.ok)
    {
        result.ok = false;
        for (const auto &err : g1.errors)
        {
            const std::string msg = std::string("[G1][") + ctx + "] idx="
                + std::to_string(err.source_index) + ": " + err.message;
            std::fprintf(stderr, "%s\n", msg.c_str());
            result.diags.push_back({ ColorSourcePipelineResult::Diag::Level::Error, msg });
        }
        return result;  // G1 失败时不继续 G2
    }

    // ── G2: DescriptorRequirement 收集 + BindingAllocator 分配 ──────────────
    const ColorSourceCodegenRegistry &reg = ColorSourceCodegenRegistry::Global();

    BindingAllocator allocator;

    for (const auto &cs : effective_sources)
    {
        const IColorSourceCodegen *codegen = reg.Find(cs.kind);
        if (!codegen)
        {
            // UserPCG / 未注册 kind：允许无 binding，跳过 DeclareRequirements
            continue;
        }

        std::vector<DescriptorRequirement>   bindings;
        std::vector<PushConstantRequirement> push;
        codegen->DeclareRequirements(cs, bindings, push);
        if (!bindings.empty())
            allocator.AddRequirements(bindings);
    }

    const BindingAllocResult alloc = allocator.Allocate();

    // 转换 G2 诊断
    for (const auto &d : alloc.diags)
    {
        const auto level = (d.level == BindingAllocDiag::Level::Error)
            ? ColorSourcePipelineResult::Diag::Level::Error
            : ColorSourcePipelineResult::Diag::Level::Warning;

        const std::string msg = std::string("[G2][") + ctx + "] " + d.message;
        std::fprintf(stderr, "[G2][%s] %s\n", ctx, d.message.c_str());
        result.diags.push_back({ level, msg });
    }

    if (!alloc.ok)
    {
        result.ok = false;
        return result;
    }

    // ── 输出 binding map（G2 observable）───────────────────────────────────
    if (!alloc.bindings.empty())
    {
        std::fprintf(stderr, "[G2][%s] binding map:", ctx);
        for (const auto &rb : alloc.bindings)
            std::fprintf(stderr, " %s→(set=%u,b=%u)", rb.debug_name.c_str(), rb.set, rb.binding);
        std::fprintf(stderr, "\n");
    }

    result.bindings = alloc.bindings;
    return result;
}

} // namespace hgl::graph
