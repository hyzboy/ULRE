# ShaderGen 拆分复审计划（2026-03-04）

## 1. 复审结论（基于当前代码）

本轮复审确认：

- `MaterialManager` 继续依赖 `VulkanDevice` 是合理的（渲染资源创建职责）。
- `ShaderGen` 不应依赖 `VulkanDevice`，仅应依赖“物理设备能力快照”。
- “能力快照”应统一为 contract DTO，并支持双来源：
  - 运行时采样（当前机器设备）
  - 外部 JSON（离线/远端/预设设备）

这条边界是“彻底解耦”的主线，不再混用“渲染对象依赖”和“编译能力依赖”。

---

## 2. 当前状态快照（As-Is）

已具备：

- ShaderGen contract 已落地（request/result + validator + mirror diff）。
- `MaterialManager` 主路径已 mirror-first + strict gate policy。
- 物理设备采集器已可输出 JSON（纯 JSON stdout，支持 shell 重定向）。
- `GLSLCompiler` 调用端已恢复以 `GetLimit/SetLimit` 为标准入口（不强绑扩展接口）。
- `SetShaderCompilerVersion(pd)` 已恢复“按 Vulkan 版本/扩展选择 SPV 版本”。

仍缺：

- `ShaderGenRequest` 中 physical profile 的“统一写入与消费链路”尚未完全贯通到编译策略层。
- `collector JSON -> profile DTO -> GLSLCompiler limits` 仍缺一条端到端示例路径。
- ABI 面仍有“可选扩展接口”残留，需明确长期策略（保留兼容或移除）。

---

## 3. 目标架构（To-Be）

### 3.1 边界

- Renderer 层：
  - 持有 `VulkanDevice`。
  - 负责资源创建与运行时生命周期。

- ShaderGen 层：
  - 不持有 `VulkanDevice`。
  - 仅接收 `PhysicalDeviceProfileLite`（contract DTO）。
  - 基于 DTO 生成 `TBuiltInResource` + 生成 SPV。

### 3.2 输入统一

统一输入模型：

- `ShaderGenRequest`
  - `material/permutation/pipeline`
  - `required_resources/vertex_requirements`
  - `physical_device_profile`（可选但推荐）

统一两种来源：

- Runtime Adapter: `VulkanPhyDevice -> PhysicalDeviceProfileLite`
- JSON Adapter: `collector JSON -> PhysicalDeviceProfileLite`

---

## 4. 重排后的执行阶段

## Phase A（1~2 天）：边界冻结 + 接口口径统一

- 固定原则：`GetLimit/SetLimit` 为编译器限制设置标准入口。
- 将 `PhysicalDeviceProfileLite` 作为 ShaderGen 输入契约主字段。
- 明确“扩展接口策略”：
  - 若保留扩展函数，仅作为内部兼容，不作为上层依赖。

验收：

- 文档明确“单一标准入口 + 双来源输入”。
- `ULRE.ShaderGen` 构建通过。

## Phase B（2~3 天）：双来源数据链路打通

- Runtime 路径：
  - `VulkanPhyDevice` -> profile DTO -> `ShaderGenRequest` -> `GetLimit/SetLimit`。
- JSON 路径：
  - collector JSON -> profile DTO -> `ShaderGenRequest` -> `GetLimit/SetLimit`。
- 增加最小示例调用（不要求 UI，仅代码路径）。

验收：

- 两条路径使用同一 DTO 与同一 limits 应用函数。
- 同一 profile 输入下，编译目标版本与关键 limits 一致。

## Phase C（2~4 天）：策略层消费 profile（真正影响生成策略）

- 将 profile 的关键字段纳入 ShaderGen 策略分支：
  - descriptor/UBO/SSBO 上限约束
  - feature 开关（geometry/tessellation/indexing）
  - stage capability 降级策略
- 输出统一 diagnostics（机器可读）。

验收：

- 低配 profile 会触发可解释的降级路径。
- mirror/strict gate 在 profile 变化下行为可预期。

## Phase D（1~2 天）：验证与收口

- 回归：
  - `test_ShaderGenPathMode`
  - strict gate 相关测试
  - collector + JSON 导入路径 smoke test
- 记录基线：不同 profile 下的编译结果摘要（stage/layout/diagnostics）。

验收：

- 关键链路稳定，无新增 ABI 破坏。
- 文档与代码行为一致。

---

## 5. 本周优先级（执行顺序）

P0：

1. 完成 JSON profile 直接喂给 ShaderGen 编译器 limits 的端到端示例。
2. 固化 `GetLimit/SetLimit` 入口为官方路径（扩展接口降级为兼容）。

P1：

3. 将 profile 字段正式写入 `ShaderGenRequest` 的使用路径（不只存储）。
4. 增补 profile 驱动下的 strict gate 与 diagnostics 回归样例。

P2：

5. 输出 profile 驱动策略矩阵文档（字段 -> 影响点 -> 降级行为）。

---

## 6. 风险与约束

- 风险 1：`TBuiltInResource` 字段映射不精确会导致“可编译但策略偏差”。
  - 对策：先聚焦已验证关键字段（attrib/uniform/storage/descriptor_set + feature flags）。

- 风险 2：插件 ABI 变更影响旧调用。
  - 对策：上层仅依赖 `GetLimit/SetLimit`，新增接口仅内部保底。

- 风险 3：运行时路径与 JSON 路径行为漂移。
  - 对策：统一 DTO + 统一 apply 函数 + 同输入对比测试。

---

## 7. 完成定义（DoD）

满足以下条件视为“ShaderGen 拆分完成到可独立演进阶段”：

- ShaderGen 不再依赖 `VulkanDevice`。
- ShaderGen 编译限制仅通过 profile DTO 驱动。
- runtime/json 双来源可互换，行为一致。
- `MaterialManager` 仅保留渲染职责，不承载 ShaderGen 设备细节逻辑。
