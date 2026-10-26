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
    //        HGL_MATERIAL_HAS_VERTEX_NORMAL   ← material/checkerboard_3d_source.glsl 等
    //                                          （`#ifdef` 选顶点法线，与上一行互斥）
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
    /// 这是运行时可用的数据契约，不反向依赖程序构建模块。
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

        // ── 顶点法线来源 ────────────────────────────────────────────────────
        /// 顶点格式没有 Normal 但材质要法线光照 ⇒ main() 用 dFdx/dFdy 叉积求面法线。
        /// 这是**构建请求**的事实（几何格式），不是材质数据。
        bool face_normal_fallback = false;

        /// 顶点格式**有** Normal 且材质要法线光照 ⇒ 走顶点法线，并让
        /// `material/checkerboard_3d_source.glsl` 等模块的
        /// `#ifdef HGL_MATERIAL_HAS_VERTEX_NORMAL` 分支成立。
        /// 与 `face_normal_fallback` 严格互补：同一个判据（几何格式 × 材质法线需求）
        /// 的两个互斥结果，两者绝不同时为真。
        bool vertex_normal_present = false;

        // ── 材质定义自带的 compile_defines ──────────────────────────────────
        /// 指向 `MaterialDefinition::compile_defines` 的元素数组，逐条发射成
        /// `#define <name> 1`。旧路径由 `BuildCompileDefineDocument` 在收尾阶段
        /// 注入，现在同样是 options 的唯一产地（例：Text2D 的 TEXT_SDF_ENABLED
        /// 决定 `text_source_gpu.glsl` 走 SDF 还是位图分支）。
        ///
        /// 不走容器类型：本层只读这块连续内存（`definition_define_count` 个元素）。
        const char *const *definition_defines = nullptr;
        uint32 definition_define_count = 0;

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

    // ═══════════════════════════════════════════════════════════════════════════
    // mesh_options.glsl —— 网格阶段主着色器唯一的可变输入
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // 与片段侧**完全同构**：每个网格形状是一份物理固定的 GLSL 编译单元
    // （`ShaderLibrary/mesh/master/<模式>__<变体签名>.mesh`），它能接受的唯一外部
    // 输入就是主着色器里那一行 `#include "generated/mesh_options.glsl"`。
    //
    // 为什么非要切这一刀：阶段六的过渡态把整个网格源按顶点格式哈希落盘
    // （`ShaderLibrary/mesh/fixed/<shape_key>.mesh`，22 份）。顶点格式每多一种组合
    // （半精度 UV、八面体法线、2D NDC 变换……）就多一个"找不到固定源"的运行时失败，
    // 因为**网格源里唯一随格式变化的部分**被固化了。切出来之后：
    //
    //   顶点格式变化 → 只改 options 文本（同样落盘为 `generated/mesh_options.glsl`）
    //   领域/变体变化 → 换一份主着色器（这本来就该是一份文件）
    //
    // 三条硬约束（与 `EmitDomainOptions` 同一套口径）：
    //
    //   1. **只发射真正被消费的 `#include`**，顺序固定为
    //      Stage 1（属性解码） → Stage 2（位置映射） → Stage 3（投影/视口）。
    //      顺序不是风格问题：`s1_position_*` 的 `LoadVertexData()` 按固定顺序调用
    //      各流模块的空对象实现（空对象模式见 `s1_stream_defaults.glsl`），所以
    //      其它 s1 模块必须先于它被 include；而 `s2_*` 的 `GetLocalPos()` 依赖
    //      Stage 1 声明的 `Position`，`s3_*` 的 `GetClipPos()` 依赖 Stage 2。
    //   2. **`s2` 与 `s3` 必须成对出现**。Stage 2 给出 `GetLocalPos()`、Stage 3 给出
    //      `GetClipPos()`/`GetWorldPos()`，主着色器的 `GetClipPos(GetLocalPos())`
    //      少了任一半都编不过。两者皆空是合法的唯一特例：CharQuad 一类自持全部
    //      SSBO 的路径（`s1_text_char_quad.glsl` 自带整条链路）。
    //      Stage 1 则无条件必需——`s2_*` 消费的正是 Stage 1 解出的 `Position`。
    //   3. **文本同时是 program 身份的一部分** ⇒ 无注释、无空行、无 CR、
    //      以 `'\n'` 收尾，且每次都是整体覆盖（不留半截文本）。

    struct MeshOptionsInput
    {
        /// Stage 1：顶点属性解码 include 块（1~5 行，每行一条
        /// `#include "vertex/s1_*.glsl"`）。
        ///
        /// 这段文本由 `BuildResolvedVertexABI` 按「几何 VkFormat × 材质顶点语义需求」
        /// 选出——那份「格式 ↔ 解码模块」查表是唯一真源（新增格式必须同时新增解码
        /// 模块并在那里登记）。本层只做**透传**，绝不在这里再枚举一遍格式：两处
        /// 各写一份迟早会分叉，而分叉的症状是"顶点全体错位"这种最难查的静默缺陷。
        const char *vertex_decode_includes = nullptr;

        /// Stage 2：位置映射模块（`vertex/s2_*.glsl`），由
        /// `VertexNodeConfigResolver::GetMappingModulePath` 产出。CharQuad 自持路径为 nullptr。
        const char *position_mapping_module = nullptr;

        /// Stage 3：投影/视口变换模块（`vertex/s3_*.glsl`），由
        /// `VertexNodeConfigResolver::GetStage3ModulePath` 产出。CharQuad 自持路径为 nullptr。
        const char *stage3_module = nullptr;
    };

    /// 发射 mesh_options.glsl 文本。
    /// 成功时**整体覆盖** `out_mesh_options_glsl`；返回 false 时保持调用前的原值。
    /// 返回 false = 输入不自洽（全空 / s2 与 s3 只给了一半 / 路径非法 /
    /// 解码块里有非 `#include` 行）。
    bool EmitMeshOptions(
        const MeshOptionsInput &input,
        AnsiString &out_mesh_options_glsl);

    // ═══════════════════════════════════════════════════════════════════════════
    // mesh options 摘要（stage 缓存键互锁）
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // 同形状的网格主着色器文本**逐字节相同**，两种完全不同的顶点格式会算出同一个
    // stage source digest，磁盘上旧 SPV 被永久命中复用（症状：换了顶点格式，几何
    // 还是老的）。options 文件的 include 闭包不在 ShaderLibrary 下，闭包哈希抓不到
    // 它，所以必须把 options 文本的摘要送进 stage 键——落点是主着色器尾部的一行注释。
    //
    // 三处消费方必须**调同一个函数**，否则会各自漂移且都不报错：
    //   ① 生产侧写注释（`MaterialProgramAssembler`，让 stage 键覆盖 options 内容）；
    //   ② `DomainShaderBuildBackend` 给 options 目录名取网格哈希（两文件成对）；
    //   ③ 门禁校验注释真的存在、只有一条、且等于落盘文本的摘要。
    //
    // 注释形态固定为 `"\n// mesh-options-digest <十进制 uint64>\n"`，其中前导换行属于
    // 标记本身：主着色器不保证以换行收尾，标记自带一个才能保证注释独立成行。

    /// 注释标记本体（**含前导换行**）。声明成数组而非 `const char *`，
    /// 才能让扫描侧拿到编译期长度，不必再手数一遍字符。
    inline constexpr char kMeshOptionsDigestMarker[] = "\n// mesh-options-digest ";
    inline constexpr size_t kMeshOptionsDigestMarkerLength =
        sizeof(kMeshOptionsDigestMarker) - 1;

    /// options 文本 → 摘要。空指针/空长度返回 0（0 是"无摘要"的保留值）。
    uint64 ComputeMeshOptionsDigest(const char *text, const size_t length);
    uint64 ComputeMeshOptionsDigest(const AnsiString &text);

    /// 把摘要追加到网格主着色器文本尾部（上述固定形态）。
    /// 模板而非固定类型：生产侧的目标缓冲历史上是 `std::string`（`plan.ms`），
    /// 而本层其余接口都用 `AnsiString`——为了一个拼接动作去改上游缓冲类型是反向成本。
    template <typename TextType_>
    void AppendMeshOptionsDigest(TextType_ &inout_glsl, const uint64 digest)
    {
        const AnsiString digits = AnsiString::numberOf(digest);
        inout_glsl += kMeshOptionsDigestMarker;
        inout_glsl += digits.c_str();
        inout_glsl += "\n";
    }

    /// 扫描主着色器文本里的溯源注释。
    /// 三种异常形态必须能与"不存在"区分开：多写一条、值不是十进制数字，
    /// 都说明生产侧被改坏了，而它们在索引层面都会被当成合法注释静默放过。
    enum class MeshOptionsDigestScan
    {
        Absent,     ///< 没有标记（CharQuad 一类不写 options 的路径，或生产侧漏写）
        Present,    ///< 恰好一条，且值解析成功
        Malformed,  ///< 标记在，但后面不是"至少一位十进制数字 + 行尾"
        Duplicated, ///< 标记出现多于一次（谁才是被 stage 键覆盖的那一条不可判定）
    };

    /// `out_offset` 返回标记的字节偏移（用于剥离尾部注释，`Absent` 时不写）。
    MeshOptionsDigestScan ScanMeshOptionsDigest(
        const char *glsl, const size_t length,
        uint64 &out_digest, size_t *out_offset = nullptr);
}// namespace hgl::graph::mtl
