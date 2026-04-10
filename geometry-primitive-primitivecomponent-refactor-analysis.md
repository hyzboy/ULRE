# Geometry / Primitive / PrimitiveComponent 职责重构分析（中文）

日期：2026-04-11（持续更新）

## 0. 阶段完成度看板

- Phase A（最小行为修复）：100%（已完成）
- Phase B（对齐与清理）：100%（已完成）
- Phase C（API 硬化）：100%（已完成）

最近状态补充：

- 已修复 MI 数据链路中的两类初始化问题（`AllocMaterialInstanceSlot` 路径与 `CreateMaterialInstance` 路径）。
- 已修复一个回归：无 MI 材质（如 `VertexColor2D`）错误地被要求分配 MI 槽，导致 `ResolveMI` 失败并无绘制指令。
- 已完成 `BindMaterialSlot` 调用来源标注（`collect` / `quad`），并对未标注调用加入运行期告警（软白名单观测）。

## 1. 背景

当前 ECS 渲染流在以下场景出现行为不匹配：

- 几何体变体 10 个
- 实体材质实例配置 100 个

观察结果：实际生效的 MI 条目约 10 个。

这不是单一调用点 bug，而是 Geometry、Primitive、PrimitiveComponent 与 ECS MI 上传路径之间的职责边界问题。

## 2. 现状模型（当前实现）

### 2.1 Primitive 混合了两类职责

Primitive 同时承载：

- 几何绘制载体状态（geometry、draw range、VIL）
- 可变的“每实例材质绑定状态”（material/domain/mi_id/MIT）

这会使“共享 Primitive”在语义上变成“共享可变材质实例容器”。

### 2.2 Render Collect 逐实体回写 Primitive

Collect 阶段先解析 material slot，再通过 `BindMaterialSlot` 回写到 Primitive。

当多个实体共享同一个 Primitive 时，后写会覆盖先写。

### 2.3 MI 上传按 Primitive 指针去重

`MaterialInstanceAssignmentBuffer` 以 `Primitive*` 作为去重键。

因此 100 个实体若只引用 10 个 Primitive 指针，MI 路径会自然塌缩为约 10 个有效条目。

### 2.4 PrimitiveComponent override 不是一等主键

`PrimitiveComponent::mi_id_override` 存在，但未成为 ECS 上传路径的一等去重主键。

结果：设置 override 也不能保证每实体 MI 唯一行为。

## 3. 根因总结

本质是“键不一致”：

- 语义期望键：每实体材质槽身份
- 运行时实际键：主要是 `Primitive*` 身份

因此“共享 Primitive + 每实体 MI”按当前设计天然不一致。

## 4. 目标职责模型

### 4.1 Geometry

只负责网格/拓扑/缓冲。

### 4.2 Primitive

表示可复用几何绘制原型与静态兼容信息，不再作为每实体 MI 槽的可变拥有者。

### 4.3 PrimitiveComponent / RenderItem

拥有每实体“已解析材质槽快照”：

- material_template
- domain
- mi_id
- MIT payload（或 MIT 引用）
- render/material preset

### 4.4 MI 上传路径

去重/上传键应为材质槽身份（domain + mi_id + material_template，可选 MIT hash），而非 `Primitive*`。

## 5. 推荐重构计划

## Phase A（最小行为修复，低 API 风险）

目标：

1. 在 RenderItem/PrimitiveRenderItem 增加每条目已解析槽快照。
2. Collect 阶段写入 RenderItem，不再依赖共享 Primitive 回写作为身份键。
3. MIAB 以材质槽键去重，而不是 Primitive 指针。

状态：已完成

- 已完成：resolved slot 快照进入 render item 主路径。
- 已完成：collect 流程消费 resolved slot，不再把共享 Primitive 回写当作主身份来源。
- 已完成：MI 分配/写入去重路径已对齐到 domain + mi_id。
- 已验证：`08_PBRSpheresECS` 与 `14_PBRColor3DSpheresECS` 日志稳定（`mode_seen=1`，`mi_direct=1`，`mi_fallback=0`）。

## Phase B（对齐与清理）

目标：

1. 降低 Primitive 可变 MI 状态职责。
2. 收敛 `BindMaterialSlot` 的使用边界（仅原型级或 deferred-setup）。
3. 将 override 路径显式化并可验证。

状态：已完成（100%）

已完成项：

- 已完成：override 消费与 resolved-slot-first 行为进入 ECS 主路径。
- 已完成：Collect 跳过 `BindMaterialSlot` 的条件已收窄为“真 domain-direct 实例槽”（`domain != nullptr && mi_id >= 0`）。
- 已完成：`PrimitiveBatchPipeline` 增加 fallback 归因统计（`fallback_no_snapshot/material/domain/mi`）。
- 已完成：清理切片 1：将 resolved-slot 可绘制性与实例索引可用性解耦。
- 已完成：清理切片 2：Descriptor 绑定实例扫描改为显式 eligibility，不再依赖 `resolved_slot_valid`。
- 已完成：清理切片 3：MIAB 在 domain-direct 下对 non-instance resolved 项不再强制 Primitive fallback。
- 已完成：清理切片 4：Quad 绑定在 domain-direct collect 开启时，不再对已有 primitive 原地 `BindMaterialSlot`。
- 已完成：清理切片 5：Collect 路径仅在“槽位状态发生变化”时才执行 `BindMaterialSlot`；对于已匹配槽位改为 no-op，减少共享 Primitive 的无意义可变写入。
- 已完成：清理切片 6：Collect 路径新增 `BindSlotSummary` 统计（attempt/success/noop/skip_domain_direct/failed），可量化剩余可变写入面并定位后续收敛目标。
- 已完成：清理切片 7：`BindMaterialSlot` 增加 `source_tag`，Collect/Quad 调用点显式标注（`collect` / `quad`），未标注调用触发告警，用于后续硬白名单收口。
- 已完成：清理切片 8：新增 CMake 开关 `ULRE_BIND_MATERIAL_SLOT_REQUIRE_TAG`（默认 OFF）。开启后未标注调用会被 `BindMaterialSlot` 直接阻断并报错，用于硬白名单门禁验证。
- 已完成：修复“无 MI 材质回归”——`CreateMaterialInstance` 仅在 `material->hasMI()` 时分配 MI 槽，恢复 `draw_triangle` 这类路径。

收口结论：

- 已收口：非过渡调用路径未发现新增共享 Primitive 可变副作用回归。
- 已收口：`BindMaterialSlot` 允许调用点具备软白名单（告警）与可选硬白名单（阻断）双门禁。
- 后续优化项：`BindSlotSummary` 的日志可见性增强转入 Phase C 工程化项，不再阻塞 Phase B 完成判定。

关键回归门禁：

- 非实例语义槽（`mi_id == -1`）在 domain-direct 过渡期仍必须可绘制（例如 `08` 天空球案例）。

## Phase C（API 硬化）

目标：

1. 引入显式类型：
   - `PrimitivePrototype`（几何中心）
   - `EntityMaterialBinding`（实例中心）
2. 废弃“共享对象可变写入隐式表达每实体状态”的歧义 API。

状态：已完成（100%）

最近推进（Phase C 批次 1）：

- 已完成：`RenderPrimitiveCollectSystem` 增加 `BindSlotSummaryLogMode`（`Off` / `Throttled` / `EveryFrame`）。
- 已完成：`BindSlotSummary` 配置入口收敛到 `ECSContext::SetBindSlotSummaryLogMode(...)`，并在系统安装/子世界同步时统一下发到 `RenderPrimitiveCollectSystem`。
- 已完成：默认行为保持 `Throttled`，不改变既有运行开销与日志噪声基线。

最近推进（Phase C 批次 2）：

- 已完成：在 `RenderItem` 层引入显式 `EntityMaterialBinding`，承载实体级 `material_template/domain/mi_id/vil/MIT` 快照。
- 已完成：`RenderItem::SetResolvedMaterialSlot()` 改为写入 `EntityMaterialBinding`，并同步维护旧的 `resolved_*` 字段作为兼容桥接。
- 已完成：`PrimitiveBatchPipeline`、`RenderDescriptorBindingSystem`、`MaterialInstanceAssignmentBuffer` 的核心读路径已切换到 `EntityMaterialBinding` 访问接口。
- 已完成：保持现有行为与回退语义不变，旧字段尚未删除，仅作为迁移过渡层。

最近推进（Phase C 批次 3）：

- 已完成：删除 `RenderItem` 中历史 `resolved_*` 兼容桥字段，收口为单一 `EntityMaterialBinding` 状态表达。
- 已完成：确认 ECS 代码中已无对 `resolved_slot_valid/resolved_material_template/resolved_domain/resolved_mi_id/resolved_vil/resolved_mit_*` 的直接读路径依赖。
- 已完成：关键目标 `ULRE.ECS`、`08`、`14`、`03`、`09` 编译通过，证明“删桥”批次未引入构建回归。

最近推进（Phase C 批次 4）：

- 已完成：`RenderItem` 新增显式命名 API：`SetEntityMaterialBinding(...)` / `ClearEntityMaterialBinding()`。
- 已完成：`RenderPrimitiveCollectSystem` 主调用点改用 `SetEntityMaterialBinding(...)`，减少 Phase B 术语在主路径中的残留。
- 已完成：删除 `SetResolvedMaterialSlot/ClearResolvedMaterialSlot` 兼容包装，完成命名收口。

最近推进（Phase C 批次 5）：

- 已完成：新增统一调试配置入口 `ECSContext::ApplyDebugConfig(const ECSDebugConfig&)` 与 `GetDebugConfig()`。
- 已完成：`BindSlotSummary` 门禁配置不再依赖环境变量约定；`ULRE_BIND_SLOT_SUMMARY` 读取路径已从 `ECSContext::Initialize()` 移除。
- 已完成：保留并强化 `ECSContext::SetBindSlotSummaryLogMode(...)` 的系统/子世界统一下发路径。

最近推进（Phase C 批次 6）：

- 已完成：新增更高层默认入口 `DefaultEcsDebugConfig`，并通过 `RegisterDefaultEcsSystems(..., debug_config)` 支持调试配置注入。
- 已完成：`DefaultSystems` 将高层配置统一转换并下发到 `ECSContext::ApplyDebugConfig(...)`，形成单一配置汇聚路径。
- 已完成：`AppFramework` 已接入默认 ECS 调试配置注入，完成从应用层到 ECS 层的端到端入口打通。
- 已完成：修复 `DefaultSystems.cpp` 中 `ECSDebugConfig` 类型限定错误后，关键目标复编通过（`ULRE.Work`、`ULRE.ECS`、`08_PBRSpheresECS`、`14_PBRColor3DSpheresECS`）。

最近推进（Phase C 批次 7）：

- 已完成：`AppFramework` 新增高层统一调试入口（`SetDefaultEcsDebugConfig/GetDefaultEcsDebugConfig`、`SetBindSlotSummaryLogMode/GetBindSlotSummaryLogMode`、`CycleBindSlotSummaryLogMode`），可直接被 Editor/UI 层调用。
- 已完成：默认系统注册改为消费 `AppFramework` 持有的 `DefaultEcsDebugConfig`，避免初始化时散落局部配置。
- 已完成：新增无面板场景的运行期切换回路（`F8` 循环 `Off -> Throttled -> EveryFrame -> Off`），用于快速验证统一入口有效性。
- 已完成：新增入口后关键目标复编通过（`ULRE.Work`、`ULRE.ECS`、`08_PBRSpheresECS`、`14_PBRColor3DSpheresECS`）。

最近推进（Phase C 批次 8）：

- 已完成：将高层统一入口接入实际运行时可视化控件：基于 ECS `TextComponent` 的 Debug HUD 面板（非仅日志/环境变量）。
- 已完成：新增 HUD 交互：`F7` 显隐面板，`F8` 循环切换 `BindSlotSummaryLogMode`（`Off -> Throttled -> EveryFrame -> Off`），并在 HUD 文本中实时显示当前模式。
- 已完成：HUD 与统一入口联动：`SetDefaultEcsDebugConfig(...)` / `SetBindSlotSummaryLogMode(...)` 变更后会同步刷新面板内容。
- 已完成：新增 HUD 后关键目标复编通过（`ULRE.Work`、`ULRE.ECS`、`08_PBRSpheresECS`、`14_PBRColor3DSpheresECS`）。

最近推进（Phase C 批次 9）：

- 已完成：HUD 控制面板扩展为三项统一调试开关：`BindSlotSummaryLogMode`、`DescriptorContractDiag`、`MaterialBindingQuery`。
- 已完成：新增运行期热键：`F9` 切换 `DescriptorContractDiag`，`F10` 切换 `MaterialBindingQuery`；并与 `F7/F8` 共同形成完整运行时调试入口。
- 已完成：`AppFramework` 新增对应高层 API（Set/Toggle + 状态查询），统一写回 `DefaultEcsDebugConfig`，避免面板状态与配置模型分离。
- 已完成：HUD 文本实时显示三项状态与控制提示，形成“可见状态 + 可交互控制 + 统一配置下发”的闭环。
- 已完成：扩展后关键目标复编通过（`ULRE.Work`、`ULRE.ECS`、`08_PBRSpheresECS`、`14_PBRColor3DSpheresECS`）。

已完成项：

- 已完成：`MaterialResourceDomain` 拥有 MI/MIT GPU 缓冲与脏区上传能力。
- 已完成：Descriptor 绑定优先 domain-direct MI/MIT SSBO，保留 legacy fallback 兼容。
- 已完成：运行期过渡诊断（`DomainDirectSummary`）覆盖 MIT attempt/semantic-off/reason。
- 已完成：`RenderItem` 已具备显式实体材质绑定模型，完成从“松散 resolved 字段”到“显式绑定结构”的第一阶段迁移。
- 已完成：`RenderItem` 兼容桥字段已删除，实体材质绑定模型完成第二阶段收口。

进行中项：

- 后续演进建议：在 Editor 专用面板复用同一 `AppFramework` 调试 API（无需新增并行配置结构）。

## 5.1 已完成验证快照

- 构建回归：`01_draw_triangle`、`03_BasicLitSunDirectionECS`、`04_BlendModeMaskedECS`、`05_BlendModeDitherECS`、`06_BlendModeA2CECS`、`07_BlendModeTransparentECS`、`08_DomainIsolationDemo`、`08_PBRSpheresECS`、`09_BasicLitMeshesECS`、`10_TextureBlinnPhongMeshesECS`、`14_PBRColor3DSpheresECS` 均可成功产出可执行文件。
- 运行日志抽样（`run01.log` / `run08.log` / `run14.log`）：
  - `01`：`ResolvedSlotSummary` 与 `DomainDirectSummary` 持续输出，未检出 `item skipped (no draw call)`、`material=null`、`ResolveMI` 失败行。
  - `08`/`14`：`domain_direct=1`、`items=101`、`resolved_slot=101`、`mi_direct=1`、`mi_fallback=0` 与预期一致。
  - 三组日志未检出 `BindMaterialSlot` 的 untagged caller 告警。
- 硬白名单门禁验证：在 `ULRE_BIND_MATERIAL_SLOT_REQUIRE_TAG=ON` 下运行 `08`，未检出 `blocked untagged caller` / `untagged caller detected`，说明当前调用面可通过硬门禁。
- 硬白名单扩展验证：在 `ULRE_BIND_MATERIAL_SLOT_REQUIRE_TAG=ON` 下运行 `03`、`09`，未检出 `blocked untagged caller` / `untagged caller detected`，且未检出 `item skipped (no draw call)` / `material=null`。
- `08`：天空球 `mi_id=-1` 回归已恢复，保持可绘制。
- `14`：`mit_attempt=0, mit_semantic_off=2` 与当前材质契约一致。

## 5.2 最近新增修复（2026-04-11）

### A. MI 槽初始化与写入边界

- 在 `AllocMaterialInstanceSlot` 路径，新增“分配后零初始化 + 有界拷贝”。
- 在 `CreateMaterialInstance` 路径，新增“仅 MI 材质分配槽 + 分配后零初始化 + 有界拷贝”。
- 在 `MaterialInstance::WriteMIData`，新增按 `GetMIDataBytes()` 截断。

效果：

- 修复了 `mid` 正确但 `mtl` 出现垃圾值的问题。

### B. 无 MI 材质回归修复

问题：

- `draw_triangle`（`VertexColor2D`）无 MI 材质被错误要求分配 MI 槽。
- 导致 `ResolveMI` 返回空槽，进而无绘制指令。

修复：

- `CreateMaterialInstance` 仅在 `material->hasMI()` 时分配 MI 槽。

效果：

- 解除无 MI 路径阻塞，恢复绘制链路。

### C. Collect 路径槽位回写收敛（Phase B 清理切片 5）

改动：

- `RenderPrimitiveCollectSystem` 新增“是否需要绑定槽位”的判定：仅在材质模板、domain、mi_id、VIL、preset 或 material_preset 发生变化时才执行 `BindMaterialSlot`。
- 对 deferred primitive 保持首次绑定行为（保证运行态可绘制状态构建）。
- 增加 `BindMaterialSlot` 失败告警，避免静默失败。

预期效果：

- 在不改变绘制行为的前提下，进一步收敛共享 Primitive 的可变写入面。
- 为后续彻底收口 `BindMaterialSlot` 调用边界提供稳定过渡。

### D. Collect 绑定边界可观测性（Phase B 清理切片 6）

改动：

- 在 `RenderPrimitiveCollectSystem` 每帧汇总日志中增加 `BindSlotSummary`。
- `attempt`：尝试执行 `BindMaterialSlot` 的次数。
- `success`：实际绑定成功次数。
- `noop`：槽位已匹配，跳过绑定次数。
- `skip_domain_direct`：domain-direct 路径按规则跳过绑定次数。
- `failed`：绑定失败次数。

效果：

- 从“只能靠代码阅读判断边界”升级为“运行期可量化边界收敛效果”。
- 为 Phase B 收尾（白名单调用点 + 剩余副作用清零）提供数据门禁。

## 6. 为什么“100 实体创建 100 Primitive”不是正确解

该做法仅通过增加对象数量绕过键冲突，牺牲了几何复用目标，属于战术规避，不是架构修复。

## 7. 风险与验证清单

### 风险

- batch key 变化可能影响排序/分组。
- descriptor 绑定可能存在历史上对 Primitive 身份的隐式依赖。
- `BindMaterialSlot` 副作用存在隐式耦合。

### 验证清单

1. 10 几何 + 100 MI 场景应产生 100 个有效 MI 上传语义。
2. 共享 Primitive 场景帧间稳定。
3. 无 override 的旧示例行为不变。
4. 过渡诊断与 MIAB 指标应与预期一致。

当前验证状态：

- (1) 已在回归样例上完成阶段性验证（`08`、`14`）。
- (2) 当前补丁集稳定；`01_draw_triangle` 已完成一次实际运行烟测，连续渲染循环与提交链路正常（见 5.3）。
- (3) 已完成一轮扩展构建扫测（见 5.1 构建回归列表）；运行期批次扫测已全部完成（用户回报）。
- (4) domain-direct 模式下以 `DomainDirectSummary` 为主，MIAB 统计为辅。

## 5.3 运行期烟测方案与调试入口

### A. 运行期烟测目标

- 验证“可编译”之外的最小运行闭环：窗口可启动、帧循环可持续、渲染提交链路无显式失败。
- 复用 Phase C 新增统一调试入口，在运行期快速切换门禁与诊断噪声等级。

### B. 建议执行批次（Debug）

批次 1（核心基线）：

- `01_draw_triangle`
- `08_PBRSpheresECS`
- `14_PBRColor3DSpheresECS`

批次 2（材质/混合模式覆盖）：

- `03_BasicLitSunDirectionECS`
- `04_BlendModeMaskedECS`
- `05_BlendModeDitherECS`
- `06_BlendModeA2CECS`
- `07_BlendModeTransparentECS`

批次 3（domain 与网格扩展）：

- `08_DomainIsolationDemo`
- `09_BasicLitMeshesECS`
- `10_TextureBlinnPhongMeshesECS`

### C. 单样例执行建议（每个样例）

1. 启动样例并观察是否进入稳定渲染循环（至少数秒）。
2. 在运行中执行调试热键组合：
  - `F7`：显示/隐藏 ECS Debug HUD
  - `F8`：循环 `BindSlotSummary` 模式
  - `F9`：切换 `DescriptorContractDiag`
  - `F10`：切换 `MaterialBindingQuery`
3. 重点检查日志不应出现：
  - `item skipped (no draw call)`
  - `material=null`
  - `ResolveMI` 失败
  - `blocked untagged caller` / `untagged caller detected`（硬门禁场景）
4. 关闭样例并记录是否有异常退出或显式错误。

### D. 当前烟测结果（新增证据）

- `01_draw_triangle` 已执行一次实际启动烟测：
  - 观测到持续帧循环（`RenderGraph` 帧开始/结束持续推进）；
  - 观测到提交链路正常（`SwapchainSubmitSystem`、`Present result=0`）；
  - 未见显式致命错误后手动结束进程。
- 批次 1/2/3 已全部执行完成（用户回报“已全部测试”），当前无新增阻断项。

结论：

- 运行期最小闭环与批次覆盖均已具备；后续重点转为长期稳定性与回归自动化。

## 8. 待讨论问题

1. MIT 是否应始终进入 dedup key，还是仅在 texture-array 模式启用？
2. `mi_id_override` 是否保留，还是迁移为显式 `EntityMaterialBinding`？
3. Phase C 完成后，Primitive 还是否需要保留可变材质字段？
4. 是否继续采用 feature-flag 渐进迁移以降低样例回归风险？

---

用于后续设计评审与实施跟踪。
