# Graphics Pipeline Libraries 可执行重构方案

- 项目: ULRE
- 日期: 2026-04-03
- 目标: 按“VkDevice 总管理 + 4类分片去重 + Link 统一出口 + 批处理阶段决策”的架构完成可回退重构，并让 GPL/非GPL 共用同一控制流

## 0. 当前完成进度（截至 2026-04-03）

### 已完成

1. `VulkanDevice` 统一入口 `AcquireGraphicsPipeline(req)` 已接入请求校验、`RenderStateProfile` 计算、`LinkedPipelineKey` 计算、缓存查询与后端分派骨架。
2. `VKGplRequest` 与 key 构建函数已落地（含稳定哈希来源、debug 字段隔离）。
3. `RenderStateProfile` 已接入请求链并参与 link key。
4. `LinkedPipelineCache` 占位实现已落地（互斥保护、命中/未命中/插入统计、节流日志）。
5. `RenderFormat::CreatePipeline(...)` 已接入统一入口转发，统一入口失败时保留 legacy 回退。
6. 构建系统已清理 `ULRE_GPL_ENABLE` 与 `ULRE_PIPELINE_UNIFIED_ACQUIRE`，策略改为“设备支持 GPL 即启用”。
7. 已落地 `PipelineLibraryCache`（VI/PR/FS/FO 四维虚拟库），并在 `AcquireGraphicsPipeline` 中接入命中统计与节流观测日志。
8. `MonolithicLinkBackend::Build(...)` 已实现真实创建路径，统一入口可通过单体后端产出 `Pipeline*`。
9. `GplLinkBackend::Build(...)` 已实现最小可运行路径（当前临时回退到 monolithic create，避免统一入口在 GPL 设备上退回 legacy）。
10. `PrimitiveBatchPipeline::BuildMaterialBatches(...)` 已开始前移 pipeline 决策：批处理阶段按 `RenderFormat + Material + VIL + PipelineData` 统一获取 pipeline 并回写到 `Primitive`。
11. 批处理前移链路已补齐失败统计、节流告警与保底策略（获取失败保留原 pipeline，不中断渲染）。
12. `TextRenderPipeline::ProcessInputs(...)` 已接入构建阶段 pipeline 预解析（首次创建 + 后续按模板刷新），并在已有 primitive 上同步更新 pipeline。
13. `RenderFormat::GetVkCreateCount()` 全局 `vkCreateGraphicsPipelines` 调用计数器已落地（`VKRenderFormat.h/cpp`），支持帧内创建数量区间统计。
14. `PrimitiveBatchPipeline::BuildMaterialBatches()` 已接入帧间 pipeline 创建越界检测：批处理结束时记录快照，下一帧批处理开始时对比，若检测到热路径创建则 `LogWarning`（阶段4验收标准 7.1/7.2 可观测化）。
15. `VulkanDevice::AcquireGraphicsPipeline(...)` 已补齐后端失败降级策略：GPL backend 失败时自动重试 monolithic backend，并禁止将空 `Pipeline*` 写入 `LinkedPipelineCache`。
16. `LineRenderPipeline` 已接入 pipeline 解析统计与渲染热路径创建断言：初始化创建记录 attempts/success/failure，`Render(...)` 中若检测到 `vkCreateGraphicsPipelines` 增量则按 pow2 节流 `LogWarning`。
17. `QuadResourcePrepareSystem` 已接入 pipeline 解析统计（attempts/success/failure）与创建增量观测，`CreateConfiguredPipeline(...)` 统一输出阶段4可观测日志。
18. `TerrainRenderPipeline::Render(...)` 已接入热路径 pipeline 创建断言：若渲染期间出现 `vkCreateGraphicsPipelines` 增量则按 pow2 节流 `LogWarning`。
19. 已新增共享观测工具 `PipelineResolveMetrics.h`，并统一接入 Text/Line/Quad/Terrain/PrimitiveBatch 五条路径：`attempts/successes/failures`、pow2 节流、summary、hot-path 违例、outside-batch 检测全部复用同一套 helper。

### 进行中

1. `GplLinkBackend::Build(...)` 真实 GPL library/link 构建逻辑未完成（当前为可运行过渡实现）。
2. `RenderFormat` 旧去重缓存和旧直连创建路径尚未彻底移除（仅过渡桥接完成）。
3. 阶段4剩余工作转入回归验证：Primitive/Text/Line/Terrain/Quad 多场景压测与日志基线（确认 steady-state 下热路径创建告警为 0）。

### 未开始

1. 场景级 pipeline 预热接口与性能面板补齐。
2. 全量回归、性能压测与稳定性压测。

## 1. 最终架构目标

1. 在 `VulkanDevice` 内新增统一管理器 `GplPipelineManager`。
2. 新增 4 个分片去重缓存类：
   - `VertexInputLibraryCache`
   - `PreRasterLibraryCache`
   - `FragmentShaderLibraryCache`
   - `FragmentOutputLibraryCache`
3. 新增 `LinkedPipelineCache`，负责最终可绑定 pipeline 的去重。
4. `RenderFormat` 只负责 RT 格式定义与 `FragmentOutputKey` 相关参数来源，不再直接创建完整 pipeline。
5. `VIL` 仅负责 VertexInput 相关 key 来源。
6. `Material` 负责 shader 相关 key 来源（拆分 pre-raster 与 fragment 两维）。
7. 渲染状态由独立结构 `RenderStateProfile` 保存并参与 key。
8. 最终 pipeline 的申请时机放在批处理构建阶段，不放在提交热点。
9. 非GPL模式不再走独立旧分支，而是在 Link 阶段用“完整创建（monolithic create）”作为后端实现，保证与 GPL 模式共享同一请求、同一 key、同一缓存决策链。

## 2. 关键设计决策

## 决策 A: 保持对上 API 稳定

- `RenderCmdBuffer::BindPipeline(Pipeline*)` 不变。
- `Primitive` / `MaterialBatch` 继续持有 `Pipeline*`。
- 上层业务与 example 无需感知 GPL。

## 决策 B: 创建时机前移

- `PipelineMaterialRenderer` 只绑定已就绪 pipeline。
- `PrimitiveBatchPipeline::BuildMaterialBatches` 阶段确定并申请 pipeline。
- 避免 draw 提交时临时编译/链接导致帧内卡顿。

## 决策 C: 双路径可回退

- 设备支持 GPL: Link 后端使用 GPL libraries。
- 否则: Link 后端使用 monolithic `vkCreateGraphicsPipelines`。

## 决策 D: 统一控制流（关键）

- 两种模式都必须经过同一入口 `AcquireGraphicsPipeline(req)`。
- 两种模式都必须构建同一套 `VertexInputKey/PreRasterKey/FragmentShaderKey/FragmentOutputKey/LinkedPipelineKey`。
- 两种模式都必须命中同一 `LinkedPipelineCache`。
- 仅最后一步后端不同：GPL 使用 `CreateLinkedFromLibraries(...)`；非GPL 使用 `CreateMonolithicFromRequest(...)`。
- 禁止存在“非GPL直接走旧 RenderFormat::CreatePipeline 的旁路”。

## 3. 新增数据结构与接口草案

## 3.1 RenderStateProfile

建议新增文件:

- `inc/hgl/vk/pipeline/VKRenderStateProfile.h`
- `src/Vulkan/pipeline/VKRenderStateProfile.cpp`

建议字段:

1. 拓扑与 primitive restart
2. rasterization 状态（polygon/cull/front/depth bias/line width）
3. depth-stencil 状态
4. color blend 状态（含 attachment 列表）
5. multisample 状态
6. 动态状态策略位集

要求:

1. 可序列化为稳定字节流
2. 提供 `Hash()` 与 `operator==`
3. 与 `PipelineData` 可互转（迁移期桥接）

## 3.2 GPL 请求对象

建议新增文件:

- `inc/hgl/vk/pipeline/VKGplRequest.h`

核心结构:

1. `GplPipelineRequest`
2. `VertexInputKey`
3. `PreRasterKey`
4. `FragmentShaderKey`
5. `FragmentOutputKey`
6. `LinkedPipelineKey`

建议请求字段来源:

1. Material: shader module / entry / specialization / pipeline layout
2. VIL: vertex binding + attribute 描述
3. RenderFormat: color/depth format 列表
4. RenderStateProfile: 固定功能状态

## 3.3 VkDevice 入口 API

在 `inc/hgl/vk/VKDevice.h` 增加:

1. `bool IsGplSupported() const`
2. `void SetGplEnabled(bool)`
3. `bool IsGplEnabled() const`
4. `Pipeline* AcquireGraphicsPipeline(const GplPipelineRequest& req)`

说明:

- `AcquireGraphicsPipeline` 内部固定执行“请求标准化 -> key 计算 -> cache 查询 -> Link后端创建”。
- GPL/非GPL 仅在 Link 后端分叉，不在上游分叉。

## 4. 模块职责拆分

## 4.1 RenderFormat

保留:

1. RT 格式持有与查询
2. 作为 `FragmentOutputKey` 的格式来源

移除/下沉:

1. 完整 graphics pipeline 直接创建逻辑
2. 原有 `(mtl|vil|cpd|restart)` 字符串去重缓存
3. 任何基于模式分叉的 pipeline 申请入口

## 4.2 VIL

职责:

1. 提供 `VertexInputKey` 构造数据
2. 保持 `DirectCreatePrimitive` 的兼容检查逻辑

## 4.3 Material

职责:

1. 提供 pre-raster 阶段 shader 描述
2. 提供 fragment shader 阶段 shader 描述
3. 提供 layout 信息

## 4.4 PrimitiveBatchPipeline

新增职责:

1. 在 `BuildMaterialBatches` 中构建 `GplPipelineRequest`
2. 调 `VulkanDevice::AcquireGraphicsPipeline` 拿到 `Pipeline*`
3. 将 `Pipeline*` 写入 `MaterialPipelineKey`

## 4.5 PipelineMaterialRenderer

职责收敛:

1. 仅消费 `MaterialBatch` 已准备好的 `Pipeline*`
2. 不创建、不回退、不等待编译

## 5. 分阶段执行计划

## 阶段 0: 预备改造（1-2 天，已完成）

任务:

1. 设备能力探测并记录日志（GPL 支持即启用，不再提供额外开关）
2. 统一 RenderFormat -> VulkanDevice 入口（失败可回退）
3. 把现有 monolithic 创建提炼成单独函数 `CreatePipelineMonolithic`
4. 定义统一 Link 后端接口 `ILinkBackend`

完成标准:

1. 现有行为零回归
2. GPL 支持设备默认走统一路径，不支持设备自动回落
3. 非GPL路径改为通过 `ILinkBackend(Monolithic)` 调用，而不是旧旁路

## 阶段 1: 数据模型与 key 上线（2-4 天，已完成）

任务:

1. 新增 `RenderStateProfile`
2. 新增 `GplPipelineRequest` 与 5 类 key
3. 为 key 提供稳定哈希与等价比较

完成标准:

1. 单元测试验证 key 等价性与哈希稳定性
2. 从现有 `Material + VIL + RenderFormat + PipelineData` 可生成请求对象

## 阶段 2: 4 类 library cache（4-6 天，进行中）

任务:

1. 实现 4 类 library 创建与缓存
2. 引入并发保护（mutex 或 RWLock）
3. 统一对象命名（便于 debug utils 追踪）
4. 提供“虚拟库统计视图”（非GPL模式下仍可观测四维 key 命中）

完成标准:

1. 重复请求命中率正确
2. 销毁流程无泄漏

## 阶段 3: link cache 与对外 Acquire（3-5 天，进行中）

任务:

1. 实现 `LinkedPipelineCache`
2. `VulkanDevice::AcquireGraphicsPipeline` 打通
3. 实现两个 Link 后端:
   - `GplLinkBackend`
   - `MonolithicLinkBackend`
4. 默认通过策略选择后端，但共享同一上游控制流

完成标准:

1. 在 GPL 开启时可输出可绑定 pipeline
2. 在非GPL模式下，输出与旧路径一致（画面/状态一致）
3. 两模式 `LinkedPipelineKey` 统计可对齐（命中行为可比）

## 阶段 4: 接入批处理阶段（3-4 天，进行中）

任务:

1. 在 `PrimitiveBatchPipeline` 前移 pipeline 决策
2. 调整 `MaterialPipelineKey` 生成路径（值不变、时机变化）
3. Renderer 热路径不再触发 pipeline 创建
4. 禁止 Renderer 内部根据模式分支创建 pipeline

完成标准:

1. 帧内提交路径无 pipeline 创建日志
2. 关键场景渲染正确

## 阶段 5: 清理与推广（2-3 天，未开始）

任务:

1. 清理 `RenderFormat` 中旧去重逻辑
2. 增加预热接口（场景加载后预链接）
3. 补齐性能统计面板
4. 删除遗留“非GPL直连旧逻辑”的代码入口

完成标准:

1. 代码路径单一、职责清晰
2. 性能数据可观测

## 6. 具体改文件清单

## 6.1 必改

1. `inc/hgl/vk/VKDevice.h`
2. `src/Vulkan/VKDevice.cpp`
3. `inc/hgl/vk/VKRenderFormat.h`
4. `src/Vulkan/VKRenderFormat.cpp`
5. `inc/hgl/vk/pipeline/VKPipeline.h`
6. `src/Vulkan/pipeline/VKPipeline.cpp`
7. `src/ecs/support/PrimitiveBatchPipeline.cpp`
8. `src/ecs/support/PipelineMaterialRenderer.cpp`（仅确保无创建分支）

## 6.2 新增

1. `inc/hgl/vk/pipeline/VKGplRequest.h`
2. `inc/hgl/vk/pipeline/VKRenderStateProfile.h`
3. `src/Vulkan/pipeline/VKRenderStateProfile.cpp`
4. `inc/hgl/vk/pipeline/VKGplManager.h`
5. `src/Vulkan/pipeline/VKGplManager.cpp`
6. `inc/hgl/vk/pipeline/VKPipelineLibraryCache.h`
7. `src/Vulkan/pipeline/VKPipelineLibraryCache.cpp`
8. `inc/hgl/vk/pipeline/VKLinkBackend.h`
9. `src/Vulkan/pipeline/VKLinkBackend.cpp`

## 7. 验收标准

## 7.1 功能验收

1. Primitive/Text/Line/Terrain/Quad 全路径可渲染
2. GPL 支持/不支持设备两路径画面结果一致
3. 设备不支持 GPL 时自动回退
4. GPL/非GPL 两模式输出的 RenderStateProfile 哈希一致（同请求）

## 7.2 性能验收

1. 帧提交流程中 pipeline 创建次数为 0（冷启动除外）
2. 首帧或材质切换时 P95 卡顿不劣于当前版本
3. library cache 与 link cache 有稳定命中
4. GPL/非GPL 的 Link 请求计数与 key 分布可对齐（允许命中率不同）

## 7.3 稳定性验收

1. 连续切场景压测无 crash
2. 设备销毁时 tracked object 无泄漏增长
3. 多线程压力下无重复创建风暴

## 8. 回滚策略

1. 保留统一入口，异常时自动降级到 `MonolithicLinkBackend`。
2. 保留旧创建路径至少两个里程碑版本。
3. 新路径任何异常时自动降级，不阻断渲染。
4. 降级后仍走统一 `AcquireGraphicsPipeline(req)`，只切换 `MonolithicLinkBackend`

## 9. 风险与应对

- 驱动兼容差异。应对: 严格 feature gate + fallback。
- key 设计不完整导致错误复用。应对: key 字段评审清单 + 回放测试。
- 生命周期复杂化。应对: manager 持有与引用计数统一，`Pipeline` 仅作为可绑定包装。
- 性能收益不稳定。应对: 先做观测再优化，预热策略按场景统计驱动。
- 控制流分叉回潮。应对: 代码评审强制规则：除 Link 后端外，不允许出现 GPL/非GPL 条件分支。

## 10. 本方案执行顺序建议

1. 先完成阶段 2 与阶段 3 的后端实装（不改变上层 API）。
2. 再接入阶段 4（批处理前移），保留统一入口内自动降级。
3. 最后执行阶段 5（删除旧旁路、补齐观测与预热）。

补充里程碑检查:

1. M1: 统一入口完成，非GPL仍可跑通。
2. M2: GPL后端接入完成，双后端通过相同回归用例。
3. M3: 删除旧旁路入口，只保留统一控制流。

该顺序可以保证每个 PR 都可独立回退，且风险可控。

## 11. 阶段0+阶段1代码骨架清单（已落地，供对照）

本节只给最小骨架，不切换现有渲染行为。

## 11.1 新增: Link后端抽象（阶段0）

建议新增 `VKLinkBackend` 基础接口，统一 GPL/非GPL 最终创建出口。

建议接口签名:

```cpp
enum class LinkBackendType
{
   Monolithic,
   Gpl
};

struct PipelineBuildContext;
struct GplPipelineRequest;

class ILinkBackend
{
public:
   virtual ~ILinkBackend() = default;
   virtual LinkBackendType GetType() const = 0;
   virtual Pipeline *Build(const PipelineBuildContext &, const GplPipelineRequest &) = 0;
};

class MonolithicLinkBackend final : public ILinkBackend
{
public:
   LinkBackendType GetType() const override { return LinkBackendType::Monolithic; }
   Pipeline *Build(const PipelineBuildContext &, const GplPipelineRequest &) override;
};

class GplLinkBackend final : public ILinkBackend
{
public:
   LinkBackendType GetType() const override { return LinkBackendType::Gpl; }
   Pipeline *Build(const PipelineBuildContext &, const GplPipelineRequest &) override;
};
```

骨架要求:

1. `MonolithicLinkBackend::Build(...)` 内部先调用旧逻辑包装函数，不改输出。
2. `GplLinkBackend::Build(...)` 先返回空实现并打日志（阶段2再填）。
3. 不允许在其他模块直接判断 GPL 开关后走旁路创建。

## 11.2 新增: 请求与Key结构（阶段1）

建议新增 `VKGplRequest.h`，先定义结构，不引入重逻辑。

建议结构签名:

```cpp
struct VertexInputKey
{
   uint64_t hash = 0;
   bool operator==(const VertexInputKey &) const = default;
};

struct PreRasterKey
{
   uint64_t hash = 0;
   bool operator==(const PreRasterKey &) const = default;
};

struct FragmentShaderKey
{
   uint64_t hash = 0;
   bool operator==(const FragmentShaderKey &) const = default;
};

struct FragmentOutputKey
{
   uint64_t hash = 0;
   bool operator==(const FragmentOutputKey &) const = default;
};

struct LinkedPipelineKey
{
   VertexInputKey vi;
   PreRasterKey pre;
   FragmentShaderKey fs;
   FragmentOutputKey fo;
   uint64_t state_hash = 0;
   uint64_t layout_hash = 0;
   bool operator==(const LinkedPipelineKey &) const = default;
};

struct GplPipelineRequest
{
   const Material *material = nullptr;
   const VIL *vil = nullptr;
   const RenderFormat *render_format = nullptr;
   const PipelineData *pipeline_data = nullptr;
   PrimitiveType primitive = PrimitiveType::Triangles;
   bool primitive_restart = false;
};
```

配套要求:

1. 为 5 类 key 提供 `std::hash`。
2. 哈希来源必须稳定，不使用裸指针地址作为最终哈希。
3. 保留 debug 字段（例如 `debug_name`）仅用于日志，不参与哈希。

## 11.3 新增: 渲染状态桥接结构（阶段1）

建议新增 `VKRenderStateProfile`。

建议接口签名:

```cpp
struct RenderStateProfile
{
   VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   VkBool32 primitive_restart = VK_FALSE;

   VkPipelineRasterizationStateCreateInfo raster{};
   VkPipelineDepthStencilStateCreateInfo depth_stencil{};
   VkPipelineMultisampleStateCreateInfo multisample{};

   ValueArray<VkPipelineColorBlendAttachmentState> blend_attachments;
   VkPipelineColorBlendStateCreateInfo blend{};

   ValueArray<VkDynamicState> dynamic_states;

   uint64_t Hash() const;
   bool Equals(const RenderStateProfile &) const;

   static RenderStateProfile FromPipelineData(const PipelineData &, PrimitiveType, bool prim_restart);
};
```

实现要求:

1. `FromPipelineData(...)` 用于兼容旧 `PipelineData` 输入。
2. `Hash()` 使用稳定序列化字节流。
3. 保证同请求在 GPL/非GPL 下 `RenderStateProfile` hash 完全一致。

## 11.4 修改: VkDevice 统一入口骨架（阶段0+1）

在 `VulkanDevice` 增加统一入口，但默认保持旧行为。

建议新增成员:

```cpp
class VulkanDevice
{
private:
   bool gpl_supported = false;
   bool gpl_enabled = false;
   std::unique_ptr<ILinkBackend> link_backend_mono;
   std::unique_ptr<ILinkBackend> link_backend_gpl;

public:
   bool IsGplSupported() const;
   void SetGplEnabled(bool);
   bool IsGplEnabled() const;

   Pipeline *AcquireGraphicsPipeline(const GplPipelineRequest &req);
};
```

`AcquireGraphicsPipeline(...)` 骨架流程:

1. 校验请求参数。
2. 标准化请求并构建 `RenderStateProfile`。
3. 计算 `LinkedPipelineKey`。
4. 查询 `LinkedPipelineCache`（阶段1可先占位）。
5. 选 `ILinkBackend` 调 `Build(...)`。
6. 返回 `Pipeline*`。

## 11.5 修改: RenderFormat 过渡桥接（阶段0）

过渡期保留接口，内部改为组装请求并转发到 `VulkanDevice::AcquireGraphicsPipeline(...)`。

过渡目标:

1. 对外 `RenderFormat::CreatePipeline(...)` 签名暂不变。
2. 内部不再直接做最终创建。
3. 原 `pipeline_by_name` 标注 deprecated，阶段5移除。

## 11.6 修改: PrimitiveBatchPipeline 接入点（阶段1预埋）

先预埋调用点，不改变 batch key 行为。

建议改动:

1. 增加辅助函数 `BuildPipelineRequest(item/material/render_target)`。
2. 在 `BuildMaterialBatches` 内接入统一入口（不再引入独立开关）。
3. 失败场景仅允许在统一入口内部降级，不允许新增旁路。

## 11.7 PR切分建议（直接按单子执行）

1. PR-A: `ILinkBackend` + `VulkanDevice` GPL探测与统一入口骨架（已完成）
2. PR-B: `GplPipelineRequest` + `RenderStateProfile` + key/hash 接入（已完成）
3. PR-C: `RenderFormat` 内部请求转发骨架 + fallback（已完成）
4. PR-D: `PrimitiveBatchPipeline` 前移新入口（未开始）

每个 PR 必须满足:

1. 可单独回滚。
2. 统一入口自动降级时输出与当前一致。
3. 新增日志不超过 Debug 级别。
