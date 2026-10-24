// PipelineConfigEmitter.h — 「管线配置头」的唯一真源与唯一发射器。
//
// ────────────────────────────────────────────────────────────────────────────
// 这一层要解决什么
// ────────────────────────────────────────────────────────────────────────────
// 超薄架构的承诺是：**主着色器源码完全静态**，一个 Pass 与另一个 Pass 的差别
// 只剩下「包含哪几个算法模块」。于是这组 `#include` 的**内容、顺序、缺失处理**
// 必须收敛到一处，而不是散落在各个 composer 里各自遍历 template.slots。
//
// 本文件定义：
//   · `PipelineConfig`          —— 类型化结果（槽位 → 模块名 → include 路径）
//   · `EmitPipelineConfig`      —— 由已解析模板发射该结果（唯一判定点）
//   · `EmitPipelineConfigText`  —— 唯一的排版出口（纯 `#include` 行）
//   · `ComputePipelineConfigHash` —— 供缓存键 / 门禁使用的稳定摘要
//
// 因此 `PipelineConfig` 就是用户语境里的 `pipeline_config.h`：它的发射结果被
// 填进固定模板的 `{{module_includes}}` 槽。与 `ShaderConfig`（宏那一半）合起来
// 就是「C++ 为这个 Pass 生成的那一个头」的全部内容。
//
// ⚠ 为什么发射成文本而不是落盘 `.h`
//   与 `ShaderConfig` 同因：GLSL 的 `#include` 解析依赖 GLSLCompiler 的搜索路径，
//   落盘只会引入「生成目录生命周期 / 并发写 / 陈旧文件」三类问题。
//
// ⚠ 顺序是行为的一部分
//   发射顺序 == `template.slots` 数组顺序。`SurfaceProvider` 恒排末位，因为
//   `material_surface.glsl` 调用的 `EvalMaterialSource`/`EvalMaterialAlpha`/`GetNTB`
//   必须已被更早的行声明。改槽位表顺序 = 改生成 GLSL = 可能改变编译结果。
//
// ⚠ 缺失处理是「必需 = 硬失败 / 可选 = 静默跳过」而不是「全部跳过」
//   把必需槽静默跳过会生成一个缺少 `EvalLighting` 等符号的 shader，错误从
//   「槽位配置错了」漂移成「GLSL 编译失败」，定位成本高一个数量级。

#ifndef HGL_MTL_PIPELINE_CONFIG_EMITTER_INCLUDE
#define HGL_MTL_PIPELINE_CONFIG_EMITTER_INCLUDE

#include <hgl/mtl/RenderTemplate.h>
#include <hgl/type/DataType.h>
#include <hgl/type/String.h>
#include <string>

namespace hgl::graph::mtl
{
    struct ResolvedRenderTemplate;

    /// 一条「本 Pass 选了这个模块」的配置记录。
    /// 用定长缓冲而非 `AnsiString`：整个结构要能放进固定数组并被整体比较/摘要，
    /// 避免为每次 Pass 组合做堆分配。超长路径由 `EmitPipelineConfig` 报错拒绝，
    /// 不做静默截断（截断会变成一个指向别处的 `#include`）。
    struct PipelineConfigEntry
    {
        static constexpr int MaxIncludePathBytes = 192;
        static constexpr int MaxModuleNameBytes = 64;

        ShaderModuleSlotRole role = ShaderModuleSlotRole::Unknown;
        char module_name[MaxModuleNameBytes] = {};
        char include_path[MaxIncludePathBytes] = {};

        bool operator==(const PipelineConfigEntry &other) const;
        bool operator!=(const PipelineConfigEntry &other) const
        {
            return !(*this == other);
        }
    };

    struct PipelineConfig
    {
        static constexpr int MaxEntries = static_cast<int>(MaxRenderTemplateModuleRoots);

        PipelineConfigEntry entries[MaxEntries] = {};
        uint32 entry_count = 0;
        /// 规范化文本的 FNV1a 摘要。0 表示未发射/发射失败。
        uint64 hash = 0;
    };

    /// 由已解析模板发射管线配置。
    ///
    /// 判定规则（唯一执行点）：
    ///   · 模板声明了某槽 → 必须能在 resolved 里找到该槽的模块根；
    ///     缺失且 `required` → 失败；缺失且非必需 → 跳过（不发射该行）。
    ///   · resolved 里存在但模板未声明的槽 → 静默忽略（不发射）。
    ///     这是「profile 给了多余模块」的既有容忍行为，保持不破坏。
    ///   · 路径/模块名超长 → 失败。
    ///
    /// 失败时 `out_config` 保持 `hash == 0`，不保证其它字段有意义。
    bool EmitPipelineConfig(
        const ResolvedRenderTemplate &resolved,
        PipelineConfig &out_config) noexcept;

    /// 取某槽在配置里的下标；不存在返回 -1。
    /// 供消费方按**角色**取模块，而不是按槽位表里的硬编码下标
    /// （下标会随槽位表演进漂移）。
    int FindPipelineConfigEntry(
        const PipelineConfig &config,
        const ShaderModuleSlotRole role) noexcept;

    /// 规范化排版：每个 entry 一行 `#include "<path>"`，行尾 `\n`。
    /// 这是本文件唯一的文本格式出口；不要在别处手写 `"#include \""`。
    void EmitPipelineConfigText(const PipelineConfig &config, std::string &out);

    /// 配置摘要（基于规范化文本）。顺序不同 = 摘要不同。
    uint64 ComputePipelineConfigHash(const PipelineConfig &config);
}// namespace hgl::graph::mtl

#endif//HGL_MTL_PIPELINE_CONFIG_EMITTER_INCLUDE
