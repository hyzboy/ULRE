#pragma once

#include <hgl/mtl/MaterialDomain.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    // ═══════════════════════════════════════════════════════════════════════════
    // 领域主着色器模板表
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // 一个 `MaterialDomain` × pass 形状 = 一份**物理固定**的 GLSL 完整编译单元
    // （`ShaderLibrary/domain/<领域>/<stage>.frag`）。它不含任何 #if 分支选择
    // 「用什么算法」，算法在文件里写死；唯一的可变输入是 `generated/options.glsl`
    // （见 <hgl/mtl/DomainOptions.h>）。
    //
    // 为什么这里只有表、没有拼装：废除 ShaderGen 之后，主着色器**不再是生成物**，
    // 运行时只需要知道「这个领域 + 这个 pass 该编哪个文件」。把这张表放在
    // 数据层（而不是生成器里），发行形态才能照常解析它。
    //
    // 未列入本表的领域 = 尚无固定主着色器 ⇒ 解析失败（不是「回退到 Custom」）。
    // 静默回退会让「忘了写主着色器」变成一个能跑但算法不对的 shader——那比直接
    // 失败更难发现。

    struct DomainShaderTemplate
    {
        MaterialDomain domain = MaterialDomain::Custom;

        /// 相对 `ShaderLibrary` 的路径，例：
        /// `domain/standard_opaque/forward_lit.frag`
        AnsiString relative_path;
    };

    /// 解析 (领域, pass 形状) → 固定的主着色器文件。
    ///
    /// `masked_coverage` 只在深度/阴影 pass 有意义：需要逐像素覆盖判定
    /// （alpha_test / alpha_dither）时走 `shadow_caster_masked.frag`，否则走
    /// 空实现体的 `shadow_caster.frag`。前向 pass 忽略该参数——是否输出 alpha
    /// 由 options 里的 `HGL_ALPHA_OUTPUT` 表达，不需要两份文件。
    ///
    /// 返回 false = 该领域尚无固定主着色器，或该领域不支持此 pass
    /// （例如 Sky 不会出现在深度 pass 里）。调用方必须当成构建失败处理。
    ///
    /// 前向 pass 的文件名**逐领域不同**（StandardOpaque → `forward_lit.frag`，
    /// Sky → `sky.frag`），所以不要在调用方按领域名拼路径，一律走本函数。
    bool ResolveDomainShaderTemplate(
        MaterialDomain domain,
        ShaderProgramPurpose purpose,
        bool masked_coverage,
        DomainShaderTemplate &out_template) noexcept;

    /// 已实现固定主着色器的领域个数（供门禁穷举核对「表里每项文件都存在」）。
    int GetDomainShaderTemplateCount() noexcept;

    /// 按序号取已实现领域（0..GetDomainShaderTemplateCount()-1）。
    MaterialDomain GetDomainShaderTemplateDomainAt(int index) noexcept;

    /// 深度 pass 判定（purpose != ForwardColor）。领域模板解析与 options profile
    /// 路由必须用同一个判定，否则会解析出文件却在里面写错宏。
    inline constexpr bool IsDomainDepthPurpose(
        const ShaderProgramPurpose purpose) noexcept
    {
        return purpose != ShaderProgramPurpose::ForwardColor;
    }
}//namespace hgl::graph::mtl
