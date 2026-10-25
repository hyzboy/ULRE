#pragma once

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    // ═══════════════════════════════════════════════════════════════════════════
    // options.glsl —— 主着色器唯一的可变输入
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // 废除 ShaderGen 之后，每个 `MaterialDomain` × pass 组合是一份**物理固定**的
    // GLSL 完整编译单元（`ShaderLibrary/domain/<domain>/*.frag`），由 glslang 直接
    // 编译。它能接受的唯一外部输入，就是主着色器里那一行
    // `#include "generated/options.glsl"`——本文件就是该 options 文本的唯一产地。
    //
    // 三条硬约束（违反其一，等价性立刻失效）：
    //
    //   1. **只发射「被 GLSL 真正消费」的宏**。
    //      词表来自对 ShaderLibrary 的实际扫描，不是从旧 ShaderGen 抄名单：
    //        HGL_COVERAGE_ONLY           ← surface/material_surface.glsl: #ifndef
    //        MTL_TEX_<SLOT>_CHANNELS     ← ntb/ntb_tangent_vbo_normalmap.glsl
    //        HGL_SHADOW_PCF_POISSON_TAPS ← shadow/pcf_shadow.glsl: #ifndef 兜底 + #if >0
    //        HGL_SHADOW_NORMAL_OFFSET    ← 同上
    //        HGL_MATERIAL_FACE_NORMAL_FALLBACK ← 各主着色器的 main()
    //        HGL_ALPHA_TEST / HGL_ALPHA_CUTOFF / HGL_ALPHA_DITHER ← alpha_compositor.glsl
    //        HGL_ALPHA_OUTPUT            ← 各主着色器的 main()
    //      旧路径还会发射 `HGL_USE_MATERIAL_SOURCE_PROVIDER` / `HGL_USE_SCENE_LIGHTING`，
    //      这两个宏在现有 ShaderLibrary 里**零消费者**（读它们的槽位 #if 已随条件编译
    //      一起删除），属于纯噪声，本发射器不再发射。
    //
    //   2. **数字格式逐字节对齐历史实现**。
    //      旧路径用的是 `std::to_string(float)`，隐式提升成 double 后按 `%f` 输出，
    //      即**六位小数**（`0.500000`）。program 身份哈希入口吃的是本函数的文本，
    //      格式漂移会让「同一材质」分裂出第二个 program。uint 走十进制无符号。
    //
    //   3. **`channels == 0` 的纹理槽整条跳过**，绝不发射成 `0`。
    //      下游用 `#if defined(MTL_TEX_X_CHANNELS)` 表达「该槽存在且非零」，发射 0
    //      会让「存在」判定为真而取到错误分支（双通道法线重建被误触发）。
    //
    // 环境光 provider 为什么必须在 options 里：它是**环境层**的选择
    // （`EnvironmentSystem::SetAmbientProvider` → `Profile::ambient_module_name`），
    // 同一个 PBR 表面因此可能配 `indirect_sky_ambient` 或 `indirect_ibl`。主着色器的
    // 固定 include 表里故意**不含**任何 ambient 模块（否则与 options 里的那份重复
    // 定义同一个 `EvalAmbientLight`），⇒ ForwardLit 必须给出字面 include 路径。
    // 顺带记一条 glslang 硬约束：`#include` 不接受宏展开（`#define X "a.glsl"` +
    // `#include X` 会报 "must be followed by a header name"），所以只能是字面路径。
    //
    // 输出形态：只含 `#define` 行与至多一行 `#include`，无注释、无空行——
    // 因为这段文本同时充当 program 身份的一部分。

    /// 主着色器的宏族。语义与旧的 `ShaderConfigProfile` 相同，但**刻意不复用**：
    /// `src/ShaderGen` 将在阶段五被物理删除，本层不能反向依赖它。
    enum class DomainOptionsProfile : unsigned char
    {
        None = 0,       ///< Sky 等无 define 块的主着色器：整段不发射（不是遗漏）
        ForwardLit,     ///< 前向着色（有场景光照 + 环境光 provider）
        ForwardUnlit,   ///< 前向无光照（flat_lighting）
        ShadowCaster,   ///< 深度/阴影 pass：位移写深度，可选 alpha 覆盖判定
    };

    struct DomainOptionsInput
    {
        DomainOptionsProfile profile = DomainOptionsProfile::None;

        /// 主着色器固定 include 表里**不含** ambient 模块，故 ForwardLit 必填。
        /// 形如 `"lighting/indirect_sky_ambient.glsl"`；ForwardUnlit / ShadowCaster
        /// 忽略此字段。ForwardLit 下给 nullptr 或空串 ⇒ `EmitDomainOptions` 返回
        /// false（宁可失败，也不要生成一份编译时才炸的 options）。
        const char *ambient_include_path = nullptr;

        // ── 阴影 PCF ────────────────────────────────────────────────────────
        /// 主着色器是否真的 include 了 `shadow/pcf_shadow.glsl`。深度/阴影 pass
        /// 自己就在写深度，没有 shadow provider 槽 ⇒ 恒为 false。
        bool emit_shadow_pcf = false;
        /// 0 = 走旧版 3x3 网格盒式采样；>0 = Poisson 磁盘采样（上限 32）。
        uint32 shadow_pcf_poisson_taps = 16;
        /// 0 = 不编译法线偏移代码，只靠深度 bias。
        uint32 shadow_normal_offset = 1;

        // ── 顶点法线降级 ────────────────────────────────────────────────────
        /// 顶点格式没有 Normal 但材质要法线光照 ⇒ main() 用 dFdx/dFdy 叉积求面法线。
        /// 这是**构建请求**的事实（几何格式），不是材质数据。
        bool face_normal_fallback = false;

        // ── 纹理通道覆盖 ────────────────────────────────────────────────────
        /// 指向 `MaterialDefinition::texture_declarations` 的元素数组。
        /// 不走容器类型：本层只读这块连续内存（`declaration_count` 个元素）。
        const MaterialTextureDeclaration *texture_declarations = nullptr;
        uint32 texture_declaration_count = 0;

        // ── 覆盖 / alpha 输出（由 ResolvedMaterialRenderState 解出）──────────
        bool alpha_test = false;
        float alpha_cutoff = 0.5f;
        bool alpha_dither = false;
        /// 曲面 alpha 写进输出颜色：`alpha_blend` 的 SRC_ALPHA 取输出 alpha。
        bool alpha_output = false;
    };

    /// 把渲染状态里「会影响 GLSL 文本」的那几个字段搬进 input。
    /// 口径来源：`HashMaterialRecipeShaderVariant` 的注释——只有
    /// alpha_test / alpha_cutoff / dither / pipeline_config.alpha_blend 会改变 SPV。
    inline void ApplyRenderStateToDomainOptions(
        DomainOptionsInput &input,
        const ResolvedMaterialRenderState &state) noexcept
    {
        input.alpha_test = state.alpha_test;
        input.alpha_cutoff = state.alpha_cutoff;
        input.alpha_dither = state.dither;
        input.alpha_output = state.pipeline_config.alpha_blend;
    }

    /// 由「是否深度 pass + 材质源模块名 + NTB provider 模块名」判定 profile。
    /// 与 `SelectSceneRenderTemplateRequest` 的模板路由同口径：
    ///   深度 → ShadowCaster；有 NTB → ForwardLit；有材质源 → ForwardUnlit；否则 Sky。
    DomainOptionsProfile ResolveDomainOptionsProfile(
        bool depth_purpose,
        const char *material_source_module_name,
        const char *ntb_provider_module_name) noexcept;

    /// 发射 options.glsl 文本。
/// 成功时**整体覆盖** `out_options_glsl`（profile==None ⇒ 空串）；
/// 返回 false 时 `out_options_glsl` 保持调用前的原值。
    /// 返回 false = 输入不自洽（例如 ForwardLit 缺 ambient 路径、槽名字符非法）。
    bool EmitDomainOptions(
        const DomainOptionsInput &input,
        AnsiString &out_options_glsl);
}// namespace hgl::graph::mtl
