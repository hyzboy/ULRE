#pragma once

/// ColorSourcePipeline.h
///
/// 统一的 ColorSource → ResolvedBindings 管道。
///
/// FinalizeColorSources() 依次执行：
///   G1 — ValidateColorSources（结构合法性）
///   AddRequirements — 收集每个 ColorSource 的 DescriptorRequirement
///   Allocate        — BindingAllocator 分配 (G2: 冲突 → ok=false)
///
/// 所有进入 codegen 的 color_sources 都应通过此函数而非直接操作 BindingAllocator，
/// 保证 G1/G2 校验逻辑只在一处实现。

#include <hgl/shadergen/ColorSource.h>
#include <hgl/shadergen/IColorSourceCodegen.h>
#include <hgl/shadergen/DescriptorRequirement.h>
#include <string>
#include <vector>

namespace hgl::graph
{

/// FinalizeColorSources 的输出结果
struct ColorSourcePipelineResult
{
    bool ok = true;

    /// G2 分配结果（binding 映射，与 color_sources 顺序对应）
    ResolvedBindings bindings;

    /// 诊断列表（G1 错误 + G2 冲突）
    /// level == Error 时 ok = false
    struct Diag
    {
        enum class Level { Info, Warning, Error };
        Level       level   = Level::Error;
        std::string message;
    };
    std::vector<Diag> diags;

    explicit operator bool() const noexcept { return ok; }
};

/// 统一 G1+G2 管道：
///   1. G1 ValidateColorSources
///   2. （可选）Bindless fallback: supports_descriptor_indexing=false 时
///      将 ColorSourceKind::BuiltinBindlessSamplerArray 重写为 BuiltinSampler2DArray，并 warn
///   3. 通过 CodegenRegistry 调用每个 ColorSource 的 DeclareRequirements
///   4. BindingAllocator::Allocate (G2)
///
/// @param sources                       待验证/分配的 ColorSource 列表
/// @param debug_context                 诊断日志上下文名称（如 row.name）
/// @param supports_descriptor_indexing  设备是否支持 VK_EXT_descriptor_indexing；
///                                      false 时 Bindless 种类自动 fallback 到 SamperArray
ColorSourcePipelineResult FinalizeColorSources(
    const std::vector<ColorSource> &sources,
    const char                     *debug_context                 = nullptr,
    bool                            supports_descriptor_indexing  = false);

} // namespace hgl::graph
