# 远期工作清单（Backlog）

> 本文档收录两轮 ECS/WorkObject 深度清理（2026-09-20 ~ 09-22，批次 1–5d，
> 提交 ffea8bb78…e417d5ab3）之后**有意留下**的远期项：要么需要设计讨论，
> 要么有明确的触发条件（等用例出现再做）。每项标注现状、做法、触发条件
> 与规模。完成一项划掉一项，不再回写正文细节——实现说明属于提交信息。
>
> 关联文档：`doc/render-target-standardization-design.md`（RT 标准化主线）。

## 🚧 当前未结项索引（唯一入口，2026-10-12 建立 · **2026-10-13 更新**）

> 目的：**一眼看全"还没做完的事"**。本索引只写「在哪、什么状态、下一步/谁拍板」，细节一律留在各自主档
> （材质链评审档 / ECS 审计档 / 本档 A~E 线），**不复制**。完成一项在这里划掉并注日期。
> 取证标注：〔档〕= 主档内记录；〔实测 日期〕= 建立/更新本索引时现场核对。

### 1. 材质链计划（主档 `doc/future/shadergen-material-chain-review.md`；批表 §196，执行记录 §6–§19.8）

| 项 | 状态 | 判据 / 下一步 |
|---|---|---|
| ~~**S4 = D 层去双真值**~~（计划内最后一个批次；2026-10-12 **已整批完成**） | **S4 全部完成（2026-10-12）：S4a=D1（§19.6）、S4b-D4（§19.7）、S4b-D2（§19.8）** | 已完成，见评审档 **§19.6（D1 表驱动必需槽）/ §19.7（D4 归一化取用口）/ §19.8（D2 剔面语义）**；施工单与红的样子都在 §19 内 |
| **S3b-3：6 个顶点节点枚举归位** | 未开工〔评审档 §18.1〕 | `VertexInputMode`/`MappingMode`/`OrientationMode`/`ScaleMode`/`ProjectionMode`/`MaterialVertexNormalMode` 的 `Get*Name` **全仓 = 0**（名字只活在 `MaterialDefinitionFile.cpp` 的本地单向表）⇒ 按 S3b-2 形态搬回枚举定义旁；需动各自枚举手（部分属 `hgl::graph`）〔实测 Get*Name 全 0〕 |
| **诊断表前缀：解析器路径未设表名** | 未做〔实测 2026-10-12〕 | 校验器的 8 段已修，但 `ParseDefinition` / `ParseRenderState` / `ParseTransformGraph` 的失败点**没有表前缀** ⇒ 日志只说 `cull_mode = "Bogus"`，不说 `render_state.pipeline.`（判据 = 在材质 TOML 写 `[render_state.pipeline] cull_mode = "Bogus"` 后的日志原文）；修法 = 各段入口 `SetParseTable(...)`，与校验器同形 |
| **S4a 的分歧场景无生产者**（可选保险） | 未做〔评审档 §19.6 诚实边界〕 | 「表标 `required=false` + 该模块缺失 + 模板属 ForwardUnlit/Sky」目前**没有任何生产者** ⇒ 要这道保险得先造生产者，再加门 case「表中 optional 槽缺失 ⇒ 组合器不得失败」 |
| **棋盘格标记色 + 2D/3D 分派**（功能项） | 未开工；TODO 已写进 `MaterialRecipe.h` 规则表上方〔档〕 | 两套 checkerboard 源读 `EmissiveSurface` 标记色；2D 走 NDC/UV、3D 走面空间；回退 ID 需按绘制维度分派；门/契约同步 |
| **过期声明订正（3 处）** | 未做〔实测原文已定位〕 | ① `doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md:1136-1139`、② `doc/future/HANDOFF_2026-10-01.md:55` 仍写"棋盘格不在回退链上、需单独一轮"（2026-10-12 已落地）；③ `doc/future/lighting-shadow-domain-split-plan.md` 的 **`T6b` 标题缺 ✅**——实测**已落地**（`LightRow`/`addr_lights`/`LightSystem`/`MainLightShadowSystem` 均在位、`sun_direction` 在 ShaderLibrary 归零）；另有 `MaterialRecipe.h` 注释里的 `SchemaGate:3534` 行号已漂移 |

### 2. ECS 审计（主档 `doc/ecs-quality-audit-2026-10.md` §2.2；共识档 `doc/ecs-audit-consensus-and-task-plan-2026-10-11.md`；逐条报告 `doc/analysis/ecs-audit-2026-10/`）

已定：**已做 2 条**（#4 `GpuMirrorBuffer.h` 抽取、#5 删 `EntitySlot::alive`）+ **复扫关闭 2 条**（非法相机槽单点化、`frame_index` 三份属不同物件状态）。**已落地 4 条（含 #1(a) 与 #2 整条）**：

- **#1（01 掩码是 `component_rows` 的缓存视图）** —— **(a) ✅ 已落地 2026-10-13**：`Entity::SetComponentRow` 与 `DetachAllComponents` 走 `#if ULRE_ECS_DEBUG_API`（开关见 §7 台帐）全表对拍，Release 宏=0 零成本。剩 **(b)** 删掩码、`HasComponentType` 改读 `component_rows[type] != 0`（`GetComponentMask()` 汇总要现场拼 ⇒ 策略路径 O(16)）／**(c)** 保持现状 —— **待你拍**
- ~~**#2（02 组件列表 4~5 份）**~~ —— **✅ 已落地 2026-10-13**：`VerifyAfterWrite` 接三个写入点（挂卸 / 可见性 / 变换行**三条路径**），台帐宏门住（Release 空 inline ⇒ 生产零开销）；「权威链」头注在位（含 `row[Transform]` 两个权威点 + `type[16]` 有意冗余两警告）；顺手修「槽复用 `Activate` 不清 `type[]/row[]` ⇒ 对拍假红」；源码契约 **Test 49**（存在性 + 计数 + 禁复活）；**红的样子** = 破坏 `Refresh` 行号写入 ⇒ 899 条点名失步，还原 0 条。详见审计报告 02 **§六 落地记录**
- **#3（03 RenderItemDescriptor 双表）** —— 先答"池**无条件写** / 实体表**只在进批判据内写**是**有意**还是**疏漏**"；**(b)** 一侧派生另一侧需先有生产者 —— `PublishRenderIDs(...)` **全仓 0**〔实测〕
- **#4（06 可见性三表示两极性）** —— **(c)** 权威链文档（零风险）+ **(b)** 每帧按权威重算 —— 未落地〔实测 `Recompute/Refresh/RebuildVisib*`、`UpdateVisibilityFrom*`、`PropagateVisibility` 全 0〕
- **#5（07 材质四条键空间）** —— 不该并表；**(a)** `VariantKeyOf` 前缀共用编解码 ~50 行，**最便宜**〔实测 `VariantKeyOf` 全仓 0〕
- **#6（08 `MaterialRuntimeRow` key 存 3.5 份）** —— 已订正"可省字段 = 0"；只需**不变量文档 + debug 门** ~15 行
- 挂同一次拍板：**旧档 §2 #5**（`AddFreeIndex` 有序维护 vs 栈消费）、**9 套系统容器合并**（共识档 §2 明确"不单独立批"）
- 小决定：**`HasComponentType()`**（生产零调用）删 or 留〔实测〕

### 3. 既存红 / 环境（与任一在途批次无关，但挡"全绿"口径）

| 项 | 状态 |
|---|---|
| **示例自检门：锚定值只在 Debug 配置成立**〔2026-10-13 订正；先前记的「7/13 FAIL ⇒ 疑似基线陈旧」与「GC 门 flaky」**两条都是我误诊**〕 | 触发 `*_SELFCHECK=1`；**每例有自己的入参要求**，读示例头注再跑：`AlphaTestShadow` 需 `--selfcheck`、**`GPUDrivenCull` 需 `GC_STATIC=1`（冻结相机；只带 `GC_SELFCHECK=1` 时相机在动 ⇒ 摘要逐次不同，我据此误判成「门 flaky」）**、`DualWorldShadows` 自判两世界隔离。**判据：Debug 13/13 PASS（逐位稳定；含 GC/ATS/DWS 各按其入参）**；**Release 只有 4 例 PASS**（PBRSpheres/AlphaModes/CascadeShadowMap/CoverageVariants/IBLEnvironment/PlaneGrid3D/GPUDrivenCull 均 FAIL） —— 同一张锚定值在 **Debug** 拍的，Release 因浮点/优化差异像素不同（`GPUDrivenCull` Release 冻结后两次 `1F32397E53E54C57`/`1F32397E53E54C57`，稳定但≠锚定值）：**不是回归、不是基线陈旧、不是 flaky**。⇒ **待你定**：门的配置口径（Debug-only 注明 / Release 另钉一份 / 只断言跨配置稳定的量）；可选加固 = `GC_SELFCHECK=1` 而无 `GC_STATIC=1` 时自动冻结（PBR 就是自动冻结）或 fail-fast 提示 |
| CMCore 独立仓 2 项 SEGV | `OrderedMapBasicOperations` / `ObjectManagerTest`，与本仓无交集 |
| GUI 类示例在桌面会话 | 环境问题，未触碰 |

### 4. 建议顺序（按"先删遗留/未使用 → 再改进"，2026-10-13 重排）

1. ~~**ECS #2 对拍器接入**~~ —— **✅ 2026-10-13 完成**（同 §2）
2. **ECS #5 `VariantKeyOf` 前缀共用** + **#6 不变量文档/debug 门**（同为"把口头约束变成 fail-fast"；接入形态照 #2：台帐宏 + 源码契约针 + 红的样子）
3. **开关台帐的两条待定 + 自检门的配置口径**（§7 / §3，零~低成本，等你一句话）
4. **S3b-3 顶点节点枚举归位**（与 S3b 同族收尾）
5. **stage B `receive_shadow` 幅度漂移专项**（未 bisect 的行为漂移；修完 `ATS_SELFCHECK=1` 必须 rc=0）
6. 文档卫生：**§1 的 3 处过期声明** + C 线 doc 十篇（可与任意批并行）
7. 功能项：棋盘格标记色上色 + 2D/3D 分派
8. **ECS #3 / #4**（"有意 vs 疏漏"与可见性权威链——牵动 GPU 行语义，放最后）

### 5. stage B 存储形欠账（主档 `doc/future/stage_B_debt_list.md`）

| 项 | 状态 | 判据 / 下一步 |
|---|---|---|
| **§2.9 末期同批**（已在档内决议"统一标记、不再各自开条、不顺手做"） | 未开工〔档 §1.5 / §2.9〕 | 纹理引用配置的数据形态（按定义变长、容量 1024/定义）+ 与它同批的另两项；行号冻结表里唯一带 ⏳ 的行 |
| **`receive_shadow` 生效幅度漂移未定位** | 未开工〔档 §2.9 后段〕 | 实测 `600662 → 593213`（`bias` 侧基本未动）⇒ 中间某提交改了生效幅度但**未 bisect**；档内已定"不顺手修"：待开专项批（先按配方打印 pass 相机 / 相机行内容 / 受影像素方向），修完 `ATS_SELFCHECK=1` 必须 rc=0 |
| **`RenderItem` 虚接口 vs `PositionSourceSpec`/`TransformPolicySpec`** | 待定（无定稿）〔档 §1.2 后段〕 | 现存形态 =「虚接口 + 默认实现」，非「组件字段」；是否借 stage B 降为**每实例 slot 上的选择器**（去掉虚函数）—— 需拍板 |
| `.ulrescene` 直载 | **已延后**（用户拍板 2026-09-30）〔档 §1.4〕 | 前提 = 全部 Component ID 化 + 访问器完成；本计划内不做 |

### 6. 本档内残留（细节见下文各线，不在此复述）

B 线 7 项小项 · A6 只剩 cubemap 六面 · E1/E2 未来项（用户留置）· 示例**干净退出**泄漏 + 间歇 CRT abort（退出码 3，判据看日志不看退出码）。

### 7. 编译期开关台帐（`inc/hgl/common/RenderOptions.h`）—— **✅ 主体已完成 2026-10-13**

> 目标（用户口径）：所有编译期开关集中一处，下次不用全仓找；校验面走宏 ⇒ **生产构建零运行时开销**。

**已完成（`f1cff9422` 及其后续 `f45980347`）**：台帐建立（§1 帧槽/容量常量、§2 校验面 + 回退开关、§3 CMake-only、§4 处置记录）；
**规约：所有开关永远有定义、值域只有 0/1 ⇒ 判断一律 `#if X`（禁 `#ifdef` / `defined()`）**；
全仓自有开关站点（ECS 21 处 + VK 回退 22 处）全部 `#if` 化；修掉 `DefaultSystems.cpp` 的
`defined(ULRE_ECS_DEBUG_API)` **Release 泄漏**（原会把 `LineStatsSystem` 也注册进去）；`RenderStrategyParity.h` 的本地默认块收归台帐；
`Entity.cpp` 两处 `#ifndef NDEBUG` ⇒ 台帐宏；`VKConfig.h` **已删除**、include 直指台帐（用户操作）；
`ULRE_ENABLE_RELEASE_SHELL` 删除（空开关）；`AssimpImport` `EXCLUDE_FROM_ALL` 屏蔽（旧转换器，不兼容，不处理）。
验收：两配置 **rc=0 / 0 error / 392 目标**、门 **45/0**、`ctest -L ecs` **12/12**、CSM **1/1**；
二进制级判据 = 校验面串 Debug 有 / Release 无、回退块 `InitDescriptorPool` 两配置都在、示例摘要三项逐位不变。

| 待定 | 说明 |
|---|---|
| **① `ULRE_ECS_DEBUG_API` 的默认语义** | 台帐默认 = AUTO（`NDEBUG` ⇒ Debug 1 / Release 0）；CMake 的 option 只表示"**强制在所有配置打开**"（不再下发 `=0`，否则压过 Debug 默认）。⇒ 确认这个口径；若要 Debug 默认也关，改台帐一处即可 |
| **② 验收口径要不要钉一条带宏的跑** | 建议：验收集里必须有一条 `-DULRE_ECS_DEBUG_API=ON`（可在 Release 上开）的构建 + 门/台架跑，否则这部分代码**长期不编译会 rot**；配套源码契约针钉住各对拍调用点存在 |
| **③ 后续接入** | 审计 §2 里其余对拍器（**#3 = 报告 03 渲染项双表** / **#6 = 报告 08 MaterialRuntimeRow key**）统一挂 `ULRE_ECS_DEBUG_API`；**模板已由 #2 立好**：台帐宏门住 + 源码契约针（存在性/计数/禁复活）+ 红的样子（破坏后必须点名失步）|
| **④ 可选加固** | `GC_SELFCHECK=1` 而无 `GC_STATIC=1` 时自动冻结（PBR 即自动冻结）或 fail-fast 提示（见 §3） |

## ❌ 已否决（勿重开；附判据出处）

> 目的：**结论要被找得到**。写"替代方案/建议"之前先扫这一节 —— 已否决的东西再被当候选提出来，
> 等于把当初的实测与裁定作废重做一遍。

| 已否决项 | 理由（出处） |
|---|---|
| **4 级联合并成单 prepass / 单 cmd（= A6）** | 各级更新节奏独立（只有级联 0 每帧全量；其余走静态缓存 + 条带）⇒ 稳态 1.00 离屏 pass/帧，无「每帧 4 次提交」；各级 `mobility_filter` 不同 ⇒ 无共享 prepass 可合并；条带是局部写（scissor）⇒ 合并 = 整图重建，把 D5 的收益还回去。`doc/csm-mechanism.md` §6 + 本档 D7（用户裁定） |
| **MSAA（含 `samples` 字段）** | 永久不做；`RenderTargetDesc::samples` 与恒拒分支已整删（`82e488103`）。本档 A6 节 |
| **② meshlet 几何 / ④ 实例化**（GPU 剔除线） | won't-do，相关字段/管线已整体删除（池行 128→104 B）。`doc/future/entity-rows-gpu-cull-plan.md` §8.7 A1/A4 |
| **DGC（device-generated commands）** | 开发机无支持；形态 B 只依赖 mesh shader，不需要。同上 |
| **`ulrescene` 直载（该计划内）** | 本计划不做。同上 |
| **对 `SkyInfo` 加专用门 `W.sky-info-struct-parity`** | 结构未定型、只 2 消费者；改为通用对拍机制（T3 触发制）。`doc/future/sky-domain-and-provider-params-plan.md` §7 |
| **给离屏 RT 传 `slot_count>1`（= A7 的动作）** | 上沿全槽排空已吃掉重叠；收益上限 ≈ 0.19 ms/pass；带空间 4 条已被 CSM 占满。见本档 A7 |
| **把 D5/S5「条带 vs 整级重建」的内容差异当引擎缺陷追（= S6 已定案）** | 差异是**诊断自身相位假象**（行走中读回 A、随后读到的 `offset` 跨帧 ⇒ 整图错误映射；签名 = 最优位移顶到扫描边缘且残余 0）；修正采样时机后 **316 轮 GPU 对拍零差异**。见本档 D5/S6 |

## A 线：渲染架构演进

### A1. GPU 提交原语升级（semaphore 链 / per-frame 资源多份化） —— **✅ 已完成（2026-10-07 复核）**

> **订正（2026-10-07 复核，按符号取证）**：本节列的三个前置缺陷与「单份 host-visible 内存」**全部已消解**——
> ① `DeviceQueue::Submit` 已改为 waits[]/signals[] 列表（多等待、多信号）；
> ② 离屏 `RenderTargetData::Submit` 恒 signal 本 RT 的 lane（timeline 信号量）；
> ③ 主帧提交经 `Context` 的帧车道等待累积（`AddFrameLaneWait(rt->GetLane(), rt->GetLaneValue())` + `SetSubmitWaitsFromFrameLanes()`）await 各 pass 的车道 ⇒ 跨 pass 双向排序成立；
> ④ per-frame 资源多份化已落地：`HGL_FRAME_SLOT_TOTAL=8`（`inc/hgl/common/RenderOptions.h:17`）、L2W ring 深度 ≡ 帧槽空间（`:33`）、相机行 16 槽 × 8 帧槽。
> ⇒ 「每帧 2 次 CPU-GPU 同步点」已由 ③④ 解除；本卡的触发条件（多逐帧离屏 RT / 级联 pass 链）也早已满足（CSM + T3i 多批）。

- **现状**：三个前置缺陷——`DeviceQueue::Submit` 只支持单个 wait semaphore；
  `SwapchainRenderTarget::Submit(Semaphore*)` 显式忽略外部 wait_sem；
  离屏 `RenderTargetData::Submit` 在 `wait_sem==nullptr` 时不 signal。
  **且 Camera/Viewport UBO 与 L2W ring 段是单份 host-visible 内存**
  （`CriticalPerFrame` 仅为内存分类标签，无轮转副本）——RenderTo 覆写
  它们前必须等在途主帧（上沿 fence），离屏提交后等离屏 fence 确保 GPU 消费完
  光源 UBO 后才还原主相机数据（下沿 fence），每帧 2 次 CPU-GPU 同步点即源于此。
- **做法**：wait semaphore 列表化（或 timeline semaphore）；离屏提交恒
  signal；`RenderTo` 把待等信号量传递给主帧提交。**或** camera/viewport
  UBO 与 L2W 段 per-frame 多份化（按 frame_index 轮转副本）——任一路径
  均可解除 fence 等待。
- **规模**：6–7 文件，~150 行，触及队列提交原语。
- **触发条件**：出现多个逐帧离屏 RT（fence 等待逐个累积）、级联 pass 链，
  或 shadow pass 双向 fence 等待成为帧率瓶颈时。单 shadow map 场景
  当前方案正确且可接受。

### A2. ~~RenderPassRequest（RT 标准化收官）~~ **已完成（f99f8dda1，2026-09-23）**

- **已落地**：`RenderPassRequest{target, clear, use_target_clear, delta_time, camera}` +
  `ECSContext::RenderTo(request)`（三参重载保留为包装）；CameraSystem
  pass 级相机覆盖（override_camera，RenderTo 期间只解算覆盖相机、结束
  自动恢复）；常规更新中主从相机严格隔离（仅主相机响应输入与写入共享数据）；
  viewport 随 RT 切换由 RTSystem SyncSubsystems 内建同步。
  离屏提交后立即 WaitFence 彻底根治 UBO 踩踏造成的闪烁。
  ShadowMap 已迁移（ActivateCamera hack 与 viewport 手动防护删除，
  sun 向量与迁移前一致且阴影稳定无闪烁）。
- **衍生待办**：CameraSystem 多相机共享数据（单份 camera_data）仍是
  "串行 pass"模型——真并行多视口（同帧多相机同时渲染）需要共享数据
  实例化或全局 SSBO 化（见 A8），留待 A3/多视口用例出现时评估。

### A3. RenderGraph 跨 RT pass 链 ✅ **已结案：删除死字段（`34af98709`，2026-10-07）**

> **判定 = 删除而非实现**：`Pass::renderTarget` **全仓无一处设置**（唯一构造点位置传 nullptr），
> 执行器那段「设了会被忽略」的告警因此**永不可能触发** ⇒ 死 API + 死告警，且类注释还宣称
> `multi-RT rendering`（与实现相反）。引擎**已有在用的多 RT 机制** = `ECSContext::RenderTo()` +
> RenderPassOptions（CSM 四张 RT 与离屏世界都走它，并已含离屏槽带/帧槽分份）⇒ 在 pass 上再实现一套
> 属同一职责的第二套实现。将来做后处理链请基于 `RenderTo`，**勿复活该字段**（`RenderGraph.h` 已留注）。

- **现状**：`RenderGraph::Pass::renderTarget` 是死字段（执行器只
  LogWarning 不生效）；真正多段渲染需要命令缓冲跨 RT 切换点
  （EndRendering/BeginRendering）+ 跨 RT 提交同步（依赖 A1）。
- **触发条件**：后处理链、多视口同帧输出等用例出现时。

### A4. ~~Shadow 的材质/光照集成~~ **已完成（c293b895c，2026-09-23）**

- **已落地**：SceneBinding 补齐 ShadowInfo UBO（Set 0, Binding 5）；新增 `pcf_shadow.glsl`；
  标准 Lit / Lit IBL 材质原生集成阴影计算，移除专用 ShadowReceiver 材质；地面与全场景网格均支持自遮挡与互投。

### A5. ~~深度比较采样~~ **已完成（2026-09-23）**

- **已落地**：`sampler.toml` 中配置 `ShadowPCF` 采样器为 Reversed-Z `GreaterOrEqual` 硬件深度比较（Linear 滤波 + ClampToEdge）；
  `bindless_textures.glsl` 补充 `Sample2DArrayShadow`；`pcf_shadow.glsl` 全面切换至硬件深度比较采样（Hardware PCF），消除软件手动比较并享受硬件 2x2 面积双线性平滑。

### A6. cubemap / CSM / MSAA —— **MSAA 永久不做（用户 2026-10-07 拍板）；本节只剩 cubemap 六面**

> **订正（2026-10-07，用户拍板）**：**不支持 MSAA（永不）** ⇒ `RenderTargetDesc` 的 `samples` 字段与
> `IsValid()` 里那条 `if(samples != 1) return false;` 是**只用来拒绝**的残留（注释自称「阶段 D」），
> 按零兼容纪律**连字段一起删**（全仓读写者只有 `RenderTargetDesc.h` 自身两处；删后需清树全量重编）。
> 本节由此**只剩 cubemap 六面（点光源阴影）**一项，且它属**新特性**而非欠账 ⇒ 做不做需另行拍板。
> CSM 部分（array + 条带滚动）**早已落地**（见本节正文与 D5）。
> **已执行（`82e488103`）**：`samples` 字段 + `IsValid()` 那条恒拒分支已删（全仓读写者确认为 `RenderTargetDesc.h`
> 自身，另三处 `VK_SAMPLE_COUNT_1_BIT` 属 `VkAttachmentDescription`，另一个结构）；清树全量重编后八台架摘要逐位不变、
> 门 45/0、TestCSM / ATS / ATU 全绿。

- **现状**：`CreateFBO` 的 `fb_info.layers=1`、`BeginRendering` 的
  `layerCount=1` 硬编码；`RenderTargetDesc::samples != 1` 被 `IsValid()`
  直接拒绝（不假装支持）。
- **触发条件**：点光源阴影（cubemap 6 面）、CSM（array）、前向 MSAA。
  desc 字段已预留位置。
- **CSM 滚动缓存**：**已落地**（2026-09-26）——固定 4 级 `ShadowInfo`、级联 split、光空间
  texel snap、环形 offset + 有效区域 + CPU 缓存状态、array layer view（`SetCascadeTexture(handle, layer)`）、
  静态物体筛选（条带路径 `mobility_filter=Static`）、失效重建（`InvalidateStaticCache`）**全部接通**，
  横向锚定（texel 口径 B 档）+ 环形条带滚动 + 接缝可达性判据均已验收（backlog D5 S1/S2/S4 ✅）。
  本卡剩余：cubemap 6 面（点光源阴影）与前向 MSAA 仍未实现（`fb_info.layers=1`、
  `BeginRendering` 的 `layerCount=1` 硬编码、`RenderTargetDesc::samples != 1` 被 `IsValid()` 拒绝）。

### A7. 离屏 RT in-flight 槽 —— **✅ 已量化结案：按数据不做（2026-10-07）**

> **原始口径（同日复核）**：`RenderTargetData` 已有完整 in-flight 槽机制（`slot_count`/`slot_index` + 每槽一份
> `{cmd_buf, queue(1 fence)}`，`BeginRender()` 复用槽前 `queue->WaitLastSubmitFence()`）；但
> `RenderTargetDesc::slot_count` 默认 1（`inc/hgl/graph/render/RenderTargetDesc.h:78`）且**全仓无人传 >1** ⇒
> 行为 = 「每帧一次 CPU 等待」。当时结论是「触发条件已满足 ⇒ 只需调用方按需启用」。

**判据来源**：探针 `PREPASS_TIMING` 的**正式基线已在 T8（`f8760056b`）**量过（`CascadeShadowMap` 1600×900、
稳态 60fps、50 窗口 = 1500 帧）：离屏 pass **1.00 次/帧**、合计 **3.76 ms/帧（≈主帧 19%）**，细分 begin 2.32 / draw 0.23 /
submit 0.047 / 上沿 0.83（突发最大 4.39）/ **下沿 0.23**；预热窗口 1.30 pass/帧、合计 13.9 ms/帧。见 `doc/csm-mechanism.md` §6。
下表为本次**复测复核**（`PREPASS_TIMING=1` = 每主帧一行 ⇒ 噪声大，只作趋势用；单位 µs）：

| 场景 | 离屏 pass/帧 | 下沿等自身（本 RT 自己的 fence） | 上沿等主帧（共享 per-frame 资源排空） | begin（collect/装配/派发） | 主帧均值 |
|---|---|---|---|---|---|
| `CascadeShadowMap`（增量） | 1.00 | 182~251 | 725~12962 | 2166~2400 | 16.7~25.7 ms |
| `CascadeShadowMap`（`CSM_CACHE_DIFF=1`） | 1.00 | 195~238 | 839~19294 | 2172~2386 | 16.7~31.3 ms |
| `DualWorldShadows`（子世界每帧离屏） | 2.00 | **376~405** | 9~37 | 1145~1685 | 16.1~17.3 ms |

**结论：三条独立理由，任一条都足以否掉「调用方传 `slot_count>1`」这个动作。**

1. **收益上限 = 实测「下沿等自身」≈ 0.19 ms/pass**，且只在「同一帧对同一 RT 提交多次」时兑现（`slot_count≥2`
   ⇒ 第 2 次提交不必等第 1 次完成）。单 pass/帧的 RT（CSM 增量恒 **1.00 次/帧**）收益 **≈ 0**；DWS 两 pass 也
   只有 ~0.2 ms/帧 ≈ 主帧 1.2%。何况呈现模式**硬编码 FIFO**（`src/SceneGraph/module/SwapchainModule.cpp:75`）⇒
   主帧均值恒 = 16.6 ms = vsync，探针那 19 ms 的「上沿」主要是在**等 vsync**，不是真实争用 ⇒ 时间类判据在当前
   呈现模式下不可比。
2. **上沿全槽排空已经把重叠吃掉了**：每次离屏 pass 前 `RenderTo` 会 `saved_target->WaitFence()` **排空主帧在途 GPU**
   （为护住单份 Camera UBO / L2W ring，见 `inc/hgl/vk/VKRenderTargetData.h:21-27`）。既然 CPU 每帧都被这个排空
   拽住，离屏 RT 多几个 in-flight 槽**换不来 CPU/GPU 重叠**；量到的大头（排空 + `begin` 1.1~2.4 ms）与
   `slot_count` **无关**。
3. **代价与收益差两个量级**：`slot_count` 同时决定该 RT 的 **per-frame 数据槽带宽度**，而离屏带空间总共只有
   `HGL_FRAME_SLOT_TOTAL - HGL_FRAME_SLOT_MAIN = 4` 条，**CSM 一条级联占一带 ⇒ 4 条级联即占满**
   （`inc/hgl/graph/module/OffscreenSlotBands.h` 原话）。给级联 RT 拿 `slot_count=2` ⇒ `HGL_FRAME_SLOT_TOTAL`
   8→≥12 ⇒ 所有 per-frame-slot 数据 ×1.5（L2W ring 深度 = TOTAL）+ 清树全量重编 + 全部基线重录，
   **换 ≤1% 帧时间**。

⇒ **本卡关闭（不做）**。若将来真要拿回这部分时间，正确对象是第 2 条指向的
**「per-frame 资源共享多份化」**（Camera UBO / L2W ring 按离屏 RT 各一份 ⇒ 排空可去；会牵动
「L2W ring 深度 == 槽空间」不变量），**前置** = 先有非 vsync 的度量手段（现在是 FIFO 硬编码）。

> ⚠ **别再把「4 级联合并成单 prepass」当作本卡的替代方案**：它 = **A6，已于 2026-10-07 否决**（用户裁定，
> 见 `doc/csm-mechanism.md` §6 与下方 D7 卡）。实测事实：**CSM 只有级联 0 每帧全量更新**，其余 3 级走静态滚动
> 缓存 + 条带脏矩形 ⇒ 稳态 **1.00 离屏 pass/帧**，根本不存在「每帧 4 次提交」；且各级 `mobility_filter` 不同
> （级联 0 = Movable、1..3 = Static）、strip 是局部写（scissor）⇒ 合并会把条带变整图重建、把 D5 省下的还回去。
> 能合并的只有 submit（47 µs）与部分等待，而按级联不可省的 `PrepareRenderPassSetup`（2.31 ms）才是大头。

### A8. ~~CameraInfo 全局 SSBO 化与 PushConstants 索引（多相机同帧并发）~~ **已完成（c7430f92f，2026-09-23）**

- **已落地**：在 `GlobalSSBOBufferRegistry` 中注册 CameraInfo（`sizeof(CameraInfo)` 步长），默认相机固定 0 号行，从属相机按需分配；
  PushConstants `RootAddresses` 末尾加入相机行号与 `_pad_camera`（**订正（2026-09-28）**：push constant 现为 **80B**，
  字段名已由 `camera_id` 改为 **`camera_row`**，首字段为 `addr_global_addresses`；相机存储本轮定稿**下沉世界级**
  （世界私有 16 槽 × 8 帧槽、0 号槽恒为本世界默认相机），见 `doc/world-addresses-and-camera-model-plan.md`）；
  GLSL BDA `CameraInfoBufferRef` 经宏 `#define camera` 透明解引用；
  ECS `CameraComponent` / `CameraSystem` / `Context::RenderTo` 全链路打通多相机隔离与 `camera_id` 下发，彻底解耦多 Pass 相机矩阵踩踏。

## B 线：ECS/框架残余小项（低优先，顺手做）

| 项 | 现状 | 备注 |
|---|---|---|
| RenderContext 类整体移除 | 只剩 `{GraphicsContext*}` 一个指针，src 25 处纯转发 | 波及 20+ 文件系统成员；下次动系统基类时顺带 |
| 组启停双写者 | 组件挂卸按计数开关 vs scene gather 全量重算 | 5d 已消全局 enabled；剩余两套可收敛为"仅 gather" |
| `RegisterGroup` 相位区间互写无校验 | 同名组不同相位区间静默覆盖 | 5–10 行 fail-fast 校验 |
| ~~destroy/IsDestroy 机制~~ | 恒 false 但 CMQT 仍 polling | **未做**：实现 `RequestExit()` 或删机制+改 CMQT |
| FreeCameraMode 空实现且为默认值 | 未设 control_mode 的相机零输入响应 | FirstPerson/LookAt 零使用，枚举可瘦身 |
| LineStatsSystem 默认注册 | 每 120 帧一条日志，`SetLogInterval` 零调用 | 降为按需注册 |
| ~~WorkManager"序列"空架子~~ | ✅ **已删**（本轮）：`OnChangeWorkObject(old,new)` 虚实全删，改裸 `cur_work_object` + 析构只解引用（所有权归创建方） | — |
| ~~TickObject 双名同物~~ | ✅ **已整删**（本轮）：`hgl/object/TickObject.h` 的 `IsTickable/SetTickable`（零引用）已删；旧版 `hgl/type/TickObject.h` + 空实现 `src/Object/TickObject.cpp` 连 CMake 条目一并删除（**无任何文件 include 该头**，唯一 include 者是它自己的空 cpp；引擎实际用的是 `hgl/object/TickObject.h`） | — |
| ~~AppFramework 空虚函数/死参~~ | ✅ **已删**（本轮）：`OnActive/OnClose/Tick` 三虚函数整删（零重写者，`app.Tick()` 唯一调用点在 CMQT 已移除）；`(void)argc` 保留（Vulkan 层未用实参） | — |
| ~~WorkObject::Render 死虚函数~~ | ✅ **已删**（本轮）：声明自称 deprecated「旧名」，但全仓零调用零重写（`WorkManager` 只调 `OnRenderPass`） | — |
| ~~RunFramework 3 参重载~~ | ✅ **已删**（本轮）：`RunFramework(title,w,h)` 递归调用自身（5 参形参类型不匹配 ⇒ 编译期必然失败）。零调用点，但写出即编译炸弹 | — |
| ~~WorkManager 继承 WindowEvent~~ | ✅ **已删**（本轮）：挂 `AddChildDispatcher` 却不重写 `OnEvent`/任何 `On*` ⇒ 每个输入事件白走一层分发 | — |
| ~~render_dirty 三件套~~ | ✅ **已删**（本轮）：`MarkRenderDirty` 零调用 ⇒ `IsRenderDirty()` 恒 true ⇒ `WorkManager::Render` 的节流分支永不生效 | — |
| ~~WorkObject 取值链四层 fallback~~ | ✅ **已收敛**（本轮）：`SetECSContext` 不再缓存 `render_context`，一律经 `world` 派生（`GetRenderContext/GetGraphicsContext/GetDevice/GetManager` 全 const） | — |
| ~~RunFramework 内 ObjectTracker 仪式~~ | ✅ **已移出**（本轮）：`initialize/shutdown_object_tracker` 改为显式 `EnableObjectTracking()/DisableObjectTracking()`（`HGL_ENABLE_OBJECT_TRACKER` 默认 0 ⇒ 原调用是空转） | — |
| `DetachAllComponents(bool)` 参数无效 | 只进日志不进分支 | 顺手 |
| ~~**`CascadeShadowMap` 编译失败（既有缺陷，非本轮引入）**~~ | **✅ 已否定（2026-10-12 复核）**：清整树 1798 obj 全量重编 **rc=0 / 0 error / 392 目标**，且 `CascadeShadowMap.exe` 冒烟 0 错误 ⇒ 原判据不成立（该条要求 `/t:Rebuild` 复核，已被整树重编覆盖）；**再复现才重开**。原判据与修法保留备查：`FrameDigest.h` 只在 `InstallFrameDigestHook` 签名里**提及** `graph::SwapchainRenderTarget`，从未 include 其定义（该头只包 `VKTextureReadback.h`/`VKFormat.h`/`FNV1a.h`），而 `CascadeShadowMap.cpp:1549` 又对它做 `dynamic_cast` ⇒ `error C2680: 'hgl::graph::SwapchainRenderTarget': class must be defined before using in a dynamic_cast` + `C3536`。**已用 git stash + 清 obj 双向验证：纯净基线上同样 FAIL(exit 1)** ⇒ 与本轮框架清理无关 | 修法：`FrameDigest.h` 补 `#include<hgl/vk/VKRenderTargetSwapchain.h>`。**注意**：该 TU 的对象文件一旦陈旧，增量构建会假绿（同 D2 卡 :271-276 的 tlog 陷阱）——判据必须是 `/t:Rebuild` 或先删 `.obj` |
| 示例**干净退出**时泄漏 + CRT abort（退出码 3） | `CascadeShadowMap` 关窗后：133 条 `[LEAK]`（TransferSrcBuffer 27 / Texture_ 21 / UBO:ShadowUBO 8 / Shader stage 7 / VAB_ 6 / SSBO:ECS:Batch:* …）+ `BufferManager::Release` 清理 → 弹 "Microsoft Visual C++ Runtime Library" abort 对话框，进程退出码 **3** | 2026-09-26 冒烟时发现（两次复现）。**与 D1/D8 无关**：示例自身未改，D8 diff 零资源生命周期行（已 grep 核对）。`shutdown_object_tracker` 只 delete tracker、不 abort ⇒ 来源在设备/静态析构期。**注意**：拿 CSM 示例做冒烟时退出码 3 不是回归信号，判据是日志内容（路由/缓存/0 VUID）。**2026-09-26 D3 冒烟第 3 次实跑 exit=0 且 0 VUID/ERROR** ⇒ 该 abort 是**间歇性**的（不是每次干净退出都触发），更不能用退出码当判据 |

## C 线：构建与文档

1. **~~TexConvCore 存量问题~~ ✅ 已修（2026-09-30）**：链接引用 `out\Windows_64_Release\TexImage.lib`，
   clean Debug 树后必失败——**根因是设计使然**：`TexImage.dll` 恒按 Release 构建（ImageMagick 上游不支持
   MSVC Debug 语义），Debug 宿主必须链它的 import lib（`src/Tools/TexConv/CMakeLists.txt:179-196`）。
   处置：`texconv_link_teximage()` 加 configure 期明确警告（缺库时给出路径/原因/修复命令，
   已受控验证"缺则打印、在则静默"）+ 本树实跑一次 `--config Release --target TexImage`。
   结果：**全量 Debug 构建 EXIT=0**（编译错误 0、链接/工具错误 0），TexConv.exe/TexConvCore.dll 正常产出。
2. **doc 十篇待更新**（逐篇清单见维护记忆）：
   - 命名类 5 篇：`GetMaterialSSBOBinding→GetGlobalSSBOBinding` 等
     （simple-sphere-material-shadergen-overview、
     ShaderGen_MaterialGLSL_DesignAnalysis、
     material-lit-recipe-to-shadergen-dataflow、ENVIRONMENT_SYSTEM、
     ShaderGen_ComposableTemplate 加"已实现"标记）；
   - 结构类 5 篇：ecs-layer-architecture-and-frame-flow（相位行号/
     RenderTo/OnRenderPass）、simple-sphere-ecs-render-chain（4-ID 基线
     标注）、gpu-driven-4id（直通已落地标注）、
     ecs/transform-data-management（传输层改 push constants）、
     ecs/primitive-geometry-vdm（结构图）。

## D 线：CSM/阴影后续（2026-09-26 review 消化 + masked 链深挖新卡）

> 来源：`doc/csm-review-2026-09-25.md`（A5/A6/A7-A10/T 系列剩余）+
> `doc/alpha-test-shadow-masked-caster-fix-chain-2026-09-26.md`（masked 链深挖
> 新卡）。快赢组（A9/A7/T10/T2/T7 + demote feature）已完成不再列。
> 深度图读回报证工具见 `example/Shadow/AlphaTestShadow.cpp` 的 DumpCascadeDepth。

### ~~D1. 深度镂空判读升级为自动契约~~ ✅ 已完成（2026-09-26）

- **现状**：depth-only 通道的 FS 剥除豁免（`IsFragmentShaderRequired`）与
  masked 链各层修复目前只有手动验证（`AlphaTestShadow` + 深度图读回，
  棋盘投影填充 ~57%=镂空）。豁免被改回时契约测试全绿但功能坏。
- **交付**（两段式，原计划"全收进 `TestCSMIncrementalPass`"不可行——该可执行
  文件**无 GraphicsContext/设备**，0 处设备引用，做不了读回）：
  1. `TestCSMIncrementalPass` **Test 11**：masked 链**源码契约**（10 条 needle）
     —— 剥离点必须检查 `keep_fragment_shader`、recipe 语义（`alpha_test`/
     `dither`）必须参与判据、`fragment_shader_required` 必须有生产者、masked
     caster 模板必须真评估 alpha（`ShadowCasterMasked`/`EvalAlpha`/
     `HGLApplyAlpha`）、forward 本体必须接线 alpha test、阴影 pass 必须判定
     `MaterialRequiresRecipeRuntimeRows`。诊断信息直指"哪条判据没了+后果"。
  2. `AlphaTestShadow` **自判自检**：第 45 帧读回深度图后立即算 c0 包围盒填充率
     并打印 `[D1-CONTRACT] c0 PASS/FAIL`；`ATS_SELFCHECK=1`（或 `--selfcheck`）
     时按契约退出码结束（0=PASS / 1=FAIL）。回归门 = 一条命令看退出码。
- **判读口径（2026-09-26 实测，已在代码里写死）**：填充率必须**相对非零
  像素的包围盒**统计，不能相对整图——c0 实测包围盒 112x58（占全图仅 0.62%），
  **框内填充 57.6%**；整图口径只有 0.4%，当断言用会立刻失败。只对 c0 断言
  （c1 静态层在本场景无 caster，全空是预期值）。判读带 50-65%：实心 ~100%、
  全空 ~0% 都在带外。
- **破坏验证（已验证非空洞）**：① 测试里把某条 needle 改错 → `Test 11 Failed`
  点名文件+needle+后果，退出码 11；② 把判读带下限临时改 0.90 → `[D1-CONTRACT]
  c0 FAIL: 包围盒填充率 57.6% < 90%`，selfcheck 退出码 1。两处均已还原并复跑绿。
- **剩余**：本契约覆盖"判据链还在（源码层）+ 链路端到端生效（图像层）"。D8 已用
  其破坏验证现场裁决并落地（删程序级扫描）→ Test 11 的 needle 随之收敛为
  9 条正向（recipe 判据 + masked 链）+ 2 条禁复活（见 D8）。

### D2. pipeline 缓存键纳入 shader 内容（新卡②）+ 同构排查（新卡③） ✅ 2026-09-26

- **原状**：`PipelineResolver::HashShaderStages` 用 VkShaderModule **句柄值**做
  `FinalPipelineKey::shader_stages_hash`——句柄在 module 销毁后可被新建模块复用，
  两个不同的 shader 会算出同一个 key → 错误复用 pipeline。
- **落地（身份改用内容，两处）**：
  1. `VulkanDevice` 新增 SPIRV 内容 hash 注册表（`Register/Unregister/GetShaderModuleHash`，
     键=句柄值、值=内容 hash、带互斥）；`CreateShaderModule` 创建成功后
     `FNV1aHasher64::AppendBytes(spv_data,spv_size)` 登记，`~ShaderModule` 注销
     （防句柄复用后残留旧身份）。全仓唯一 `vkCreateShaderModule` 调用点就在此处。
  2. `HashShaderStages(device,stages)` 改为查内容 hash；查不到 → `GLogError` 点名
     句柄+stage 并返回 0（键不完整 → fail-fast 重建，绝不静默退回句柄身份）。
  3. **同构**：`PrimitiveComponent` 的已解析管线条目收敛为单 map
     `{Pipeline*, ShaderProgramKey, has_program_key}`（原先 pipeline/program 两张
     平行 map + **program 指针比较**）；复用校验改为比对结构化 digest——program
     释放后新对象落到同一地址不再被误判成"同一个 program"。
- **同构排查（全仓）**：Line/Text 自持 pipeline **无**句柄/指针键控缓存（各自持单个
  pipeline / 按 `FontSource*` 分资源，统一走 `RenderPass::CreatePipeline` → 随本卡修复）；
  `g_device_map` 在设备析构里 erase；其余 `(uintptr_t)` 命中全是命名/日志。
  **遗留（记录待办，不在本卡）**：`resolvedRuntimePipelineMap` 以 `RenderPass*` 为键、
  值为该 pass 拥有的 `Pipeline*`，而组件侧只在材质/配方变化时清空、pass 析构无人通知。
  当前 `RenderPassManager` 按结构化 key 缓存 RenderPass 且只在 `Release()` 全清 →
  运行期不复用地址，**暂无实害**；一旦 RT 在运行期重建（A6/D5 级联重配置路径）就必须补
  「查 pass 名 + 校验 pipeline 归属」或「pass 析构时失效」，否则命中悬垂 Pipeline*。
- **验证**：
  | 门 | 结果 |
  |---|---|
  | `TestCSMIncrementalPass` | exit 0；Test 11 = 11 checks；**Test 12 = 7 checks**（pipeline 键身份契约：内容 hash 登记/消费/注销/全字节覆盖 + 2 条禁复活） |
  | `ATS_SELFCHECK=1 AlphaTestShadow` | exit 0，`c0 PASS: bbox=112x58 filled=3740 57.6%`——与 D8 基线逐字节同值 = **行为 no-op** |
  | `CascadeShadowMap` 冒烟 | 路由 105/100/4、0 `[ERROR]`、0 VUID；管线 **4 Created / 205 Reuse**（与 D2 前基线同值，无膨胀无复用退化）；`has no registered SPIRV hash` = 0（登记不漏、fail-fast 未触发） |
- **破坏验证**：把 resolver 的 `module_hash` 临时改回 `(uint64_t)(uintptr_t)stages[i].module`
  → `Test 12 Failed ... 'GetShaderModuleHash(' not found in VKPipelineResolver.cpp`，
  退出码 **12**；还原后复跑绿。
- **构建事故（记录，非代码缺陷）**：给 `VKDevice.h` 加成员的那次编辑**落在构建进行中**，
  MSBuild 的 tlog 此后不再重编 `VKDevice.obj`/`VKDeviceCreater.obj`（obj mtime 比头文件新、
  内容却是旧布局）→ 构造函数没构造新成员、使用者按新布局读 → 首次建 pipeline 键走**空 vptr**
  崩（cdb 栈：`ThreadMutexLock::ThreadMutexLock` ← `VulkanDevice::RegisterShaderModuleHash`）。
  清掉整棵 `find build -type d -path "*.dir/Debug"` obj 树重编后全绿。判定法：
  `ls -la <header> <obj>` 看 obj 是否早于头文件（详见技能 crlf-safe-editing）。

### ~~D3. receive_shadow / bias_multiplier 落地~~ ✅（2026-09-26）

- **原状**：`ShadowComponent` 四旋钮中 `receive_shadow`/`bias_multiplier` 零消费者
  （`CanCastShadow`/`GetShadowMaxDistance` 已消费）；文档
  （shadow-component-and-automated-pipeline-design.md:19）把它们写成已生效。
- **裁决**：**落地**（用户拍板）——两者都是**逐图元**着色决策，既不是材质业务
  数据也不是逐批共享量，所以随 per-draw 行表到达着色器。
- **做法**：
  1. **载体**：per-draw 行 `MaterialInstanceAddresses` **8B → 16B**，新增
     `shadow_flags`（bit0 = 不接收）与 `shadow_bias_multiplier`（局部偏差倍率）；
     行结构改为**唯一真源 X 列表** `HGL_MATERIAL_INSTANCE_ADDRESSES_FIELD_LIST`
     （`inc/hgl/graph/ShaderBufferSources.h`），CPU struct 与 GLSL struct 发射
     同源遍历——新增/改名/调序字段只改一处，没有手写漂移面。
  2. **写入端**：`PrimitiveBatchPipeline` 逐图元从 `ShadowComponent` 取
     `CanReceiveShadow()` / `GetBiasMultiplier()` 写行；未挂载组件时行保持
     零值 = 引擎默认（接收 + 倍率 1.0），与 D3 之前**逐字节一致**。
  3. **着色端**：`pcf_shadow.glsl` 新增 `GetShadowReceiveParams(data_index)`
     读行；`receive=false` 直接返回 1.0（不受影）；倍率逐级乘进
     `EvalCascadePCF` 的深度 bias 与法线偏移，并沿
     `EvalPCFShadowAt → EvalCascadeChain → EvalCascadeShadowAt → EvalCascadePCF`
     逐点穿线；`forward_lighting/flat_lighting/*.glsl.tmpl/identity.glsl` 四处
     签名同步传 `data_index`。
  4. **零值语义**：行内倍率 0 视为"不调节"（回落 1.0），保证旧零值行为不变。
- **规模**：代码 11 文件（`: `ShaderBufferSources.h` / `MaterialShaderEmitter.cpp` /
  `pcf_shadow.glsl` / `forward_lighting.glsl` / `flat_lighting.glsl` /
  `forward_lit.glsl.tmpl` / `forward_unlit.glsl.tmpl` / `identity.glsl` /
  `PrimitiveBatchPipeline.cpp` / `TestCSMIncrementalPass.cpp` / `AlphaTestShadow.cpp`），
  工作区累计 diff **+650/−38**（含 `AlphaTestShadow.cpp` 与 `TestCSMIncrementalPass.cpp`
  里前序 D1/D9 的未提交改动）+ 4 份文档。
- **证据**：
  - 源码契约：`TestCSMIncrementalPass` **Test 14 = 22 checks**（行结构单源、
    发射遍历、写入端取真值、片元端早退/缩放偏差，3 条"禁复活"needle）。
  - 运行期契约（`ATS_SELFCHECK=1 AlphaTestShadow`，**逐像素对比**三帧颜色）：
    默认帧 A → `SetReceiveShadow(false)` 帧 B：受影区 **18189 px 由"灰蓝"变回
    "红地面"**、反向 0 px（影子整体消失，方向 + 位置都锁定）；
    `SetBiasMultiplier(1000×)` 帧 C：外观变化 **600662 px**（倍率确实进入偏差计算）。
  - **对照组**（`ATS_D3_NOKNOB=1`：照常读回三帧但一个旋钮都不拨）：外观变化
    **0 px** ⇒ "读回+对比"链噪声底为 0，上面的差异只能归因于旋钮。
  - **破坏验证**：把 `params.receive` 写死 `true` + 深度 bias 不乘倍率
    → 运行期 exit **1**（两项都 0 px，并提示读回无差异）、源码契约
    **Test 14 exit 14** 点名 needle；还原后复绿。
  - 回归：`ATS_SELFCHECK` c0 仍 **57.6%**、`CascadeShadowMap` 冒烟路由/统计不变。
- **踩坑**：本场景地面是**饱和红**（R≈198,G≈1,B≈31）、影子压上去呈灰蓝——
  用"亮度（RGB 均值）"判方向会得出反直觉结论（红的均值比灰影低）；
  判据必须按**通道**（"是否变红"）而不是亮度。另：交换链颜色图实际布局是
  `PRESENT_SRC_KHR`，`Texture2D::GetImageLayout()` 停在 `SHADER_READ_ONLY_OPTIMAL`，
  拿它当 oldLayout 会被校验层判非法转换（拷贝不可信）。

### ~~D4. TransformComponent 同值短路~~ ✅（2026-09-26，改判为"静态写入语义化"）

- **原状**：`SetLocalPosition` 等 setter 无条件 `MarkDirty`（版本 +1），
  `ShouldUpdateTransform` 用"版本变了"当变更判据 ⇒ **同值**写也被判为变更 ⇒
  A3 链 `BumpStaticSceneRevision()` ⇒ 整段静态矩阵重写 + 全部静态级联缓存失效
  （当帧 4 级全量重绘）+ 变更记账 `MarkLocalChanged`（版本 + 掩码 + 子孙传播）连带标脏整棵子树。
- **关键澄清（先量化再动手）**：动静分离本来就在，且**没有**"静态每帧刷新 TRS/L2W"
  这回事——`Movable` 每帧全量写 ring 段、`Static` 走版本门控的**增量**写
  （`TransformSystem.cpp:207-281`），静态 L2W 只在被判脏时算。所以真问题不是
  "缺同值短路"，而是**"静态被写"这件事没有值比较、也没有留痕**。
- **裁决（用户拍板）**：**A′ 语义化**——不做同值短路。理由：短路只治"同值每帧写"
  这一种写法，救不了**每帧写变化的值**这种真误用（那时值在变，短路毫无作用）；
  而这类误用的正解是引擎早已有的 `SetMobility(Mobility::Movable)`（迁到 movable
  通道：每帧 ring 写 + 动态级联）。所以把"运行期写静态"做成 **API 语义**：
  一次性告警 + 把开发者引到正解，而不是替他猜。
- **做法**：
  1. `TransformComponent` 加 `static_runtime_write_armed`/`static_runtime_write_warned`
     + `ArmStaticRuntimeWriteWarning()` / `IsStaticRuntimeWriteArmed()` /
     `HasWarnedStaticRuntimeWrite()`；`WarnStaticRuntimeWrite(what)` 一条一次性
     `GLogWarning`（含 setter 名 + 实体名 + id + 代价说明 + 正解指引）。
  2. **八条写入路径**全部留痕：`SetLocalPosition/Rotation/Scale`、`SetLocalTRS`、
     `SetWorldPosition/Rotation/Scale`、`SetParent`（改父级同样让全部静态级联失效）。
  3. arming 点放在 `TransformSystem::SubmitTransformUpdates` 的 `has_dirty_static`
     分支（**在 `transform_buffer` 早退之前** ⇒ 无图形设备的单元测试路径同样成立；
     只在真有静态变更时付 O(N)，稳态仍然早退）——语义：**搭建/首次写入不告警，
     之后的写入才被 warn**（"运行期"的判定基准就是"已被渲染侧识别过一次"）。
  4. `Movable` 组件永不 arm / 永不告警；`SetMobility(Movable)` 之后写入不再是静态写入。
- **规模**：3 文件（`TransformComponent.h/.cpp`、`TransformSystem.cpp`）+ 测试 + 文档。
- **证据**：
  - 契约：`TestCSMIncrementalPass` **Test 15 = 11 源码契约 + 5 行为**（搭建期不告警、
    消费后 arm、运行期写入告警一次且不改写入语义、Movable 不受影响、迁移后不误报；
    8 个 setter + 实现 + 守卫 + arming 共 11 条 needle）。
  - **代价实测（根因钉死）**：注入探针 `ATS_D4_PROBE=1`（每帧对**静态**地面写**同值**）跑
    15 秒 ⇒ 告警 **1 条**（每组件一次，不刷屏）+ `invalidating static cascade` **872 次**
    （≈每帧一次）+ `100% Cached` **0 行** ⇒ 静态级联缓存彻底失效。探针已还原。
  - **误报门**：`ATS_SELFCHECK=1` 正常场景告警 **0 条**；`CascadeShadowMap` 冒烟告警
    **0 条**、缓存统计与基线一致 ⇒ 现有引擎/示例代码没有运行期写静态的调用点。
- **未采用（明确记录）**：B′ 同值短路、C′ 写静态自动迁移到 Movable。
- **已知调用点裁决（2026-09-26 用户）**：`example/Shadow/CascadeShadowMap.cpp:684-693` 的
  `InfiniteGround`（Static，按相机做网格吸附）**保持 Static**——吸附有同值守卫、跨格
  才写一次，接受「每跨格 1 次整级静态级联重建 + 首跨 1 条一次性告警」。该调用点同时
  解释了冒烟日志里那 2 次（长跑 16 次）`invalidating static cascade` 的来源：不是回归，
  是示例自身的刻意取舍（代码处已加注释说明）。

### D5. A8 scissor 增量分支：接通条带滚动（路线 A，S1 ✅ / S2 ✅ / **S3 ✅** / S4 ✅ / S5 ✅ / S6 ✅ —— **全档完成，无开放项**）
- **决策（2026-09-26 用户）**：走**路线 A**——接通 Toroidal 条带滚动，不删死分支；
  与 A6（4 级联合并）**解耦推进**（当时口径；**A6 已于 2026-10-07 否决 —— 见顶部「已否决」表**，条带滚动独立可用这点不变）。
- **主参数口径（用户裁定）**：横向锚定步长以 **shadowmap 侧 texel 数 `B_c`** 为主参数，
  世界米数是**派生量**（引擎数据模型本就是 texel 语义：`cache_offset`/`cache_valid_rect`
  是 `uvec`）。**世界米口径被否定**：`texel = 2r/M` ⇒ M 翻倍则同一米步长折合的 texel 数
  减半，"以米为准"无法保证跨分辨率一致。
  - 闭式（S1 已落地）：`L_c = 2·B·r0_c/(M − 1.416·B)`，`radius += 0.708·L_c`
    ⇒ `L_c/texel ≡ B`（环形偏移天然整数 texel）。
  - 代价：**精度损失 = 1.416·B/(M − 1.416·B)**，与切片半径/分辨率都无关（只看 B/M）。
    M=1024：B=16 → 2.26%、32 → 4.63%、64 → 9.7%、B=M/8 → 21.5%（旧口径一阶近似
    `1.416·B/M` 偏乐观，B=32 时 4.43% vs 实际 4.63%）。**"1/8 也行"被量化否决**：
    1/8 图恒为 21.5% 精度损失，且与分辨率无关（M 开到 4096 也一样，价码由 B/M 决定）。
  - **1/4 的正确位置不是"更新间隔"而是"放弃条带的上限阈值"**：偏移 > 1/4 图 ⇒ 放弃条带
    改整级重建（兜住瞬移/传送/极端速度），S3 落地。
- **当前档**：`cache_scroll_band_texels = {0,16,16,32}`（示例口径世界步长 ≈2.0/6.3/11.5 m）。
  更保守档 `{0,8,8,16}`（精度 1.1/1.1/2.3%，世界步长 1.0/3.1/11.5 m）备选。
- **S1 ✅（2026-09-26）**：`float cache_lateral_anchor_step = 2.0f`（共享世界米，已删）
  → `uint32_t cache_scroll_band_texels[kMaxShadowCascades] = {0,16,16,32}`（逐级 texel），
  闭式派生 + 退化 fail-safe（`B ≥ M/1.416` ⇒ 回禁用锚定，不除零）。
  - 修复的真问题：旧共享 2.0m 在 c3 折成 2.78 texel（**非整数** ⇒ 落地引入 ≤0.5 texel
    静态内容亚像素抖动），且 c3 条带仅 2.8 texel、扣 PCF 外扩 2 texel 后有效新内容
    ~0.8 texel（条带退化成"整条重叠带"）。
  - **验收**：`TestCSMIncrementalPass` **Test 16**（新增，行为 6 项 + 源码 5 条含 1 条禁复活）
    `[CSM-BAND] texel(bare)=[0.03819 0.11466 0.24092 0.60525] texel(anchored)=[0.03819
    0.11725 0.24637 0.63327] loss=[2.26% 2.26% 4.63%]` ⇒ 与闭式逐位吻合；Test 5A 重绘
    次数 `full=[600,33,17,6]`（旧 2m 口径 `[600,33,32,34]`，c2 −47%/c3 −82%）；Test 5B-1/5B-2
    （B=600 压力）/5C 全绿；`ATS_SELFCHECK=1` exit 0（D1/D3 契约不变 ⇒ 接收侧零影响）；
    CSM 示例冒烟 0 VUID、静态失效仍为 2 次（= InfiniteGround 跨格）。
  - **破坏验证 ×3**：①补偿系数 0.708→0.5（needle 全中）⇒ `锚定格 11.373 texel ≠ B=16` exit 16；
    ②旧世界米字段复活 ⇒ 禁复活 needle 咬 exit 16；③调用点改成共享 `[1]` ⇒
    `cascade 3 锚定格 16.000 texel ≠ B=32` exit 16。
- **S2 ✅（2026-09-26，含原计划的 S3 内容——跨格判据 + 复活 scissor 分支一并落地）**：
  - **控制器**：`scroll_offset`（`uint32` texel，真源）按跨格量累加 `O += shift`（mod M，`WrapTexelOffset`）；
    旧内容原地续用，只产出"新暴露条带"矩形（`AppendWrappedStrip`，跨贴图接缝时拆成两段，
    池容量 4 = 列/行各可拆 2）。跨格判据：`shift == 0` 命中 / `|shift| == B` 纯滚动 /
    其余（多格跳变、朝向变化导致格点不等距、该级 `B=0`）⇒ 整级重建 + **偏移清零**。
  - **写侧**：新增 `CascadeUpdateResult::light_view_draw = TranslateMatrix(offset.x*texel, −offset.y*texel, 0)·light_view`
    （y 取负：`OrthoMatrixReversedZ` 用 `2/(bottom-top)`、V 轴向下）；`EnvironmentSystem` 全量路径与
    条带 scissor 路径**都**改用 `light_view_draw`。⚠ 非零偏移下"整级重画"不覆盖整张贴图
    （光栅器无环绕，内容落在 `[O,1+O)` 被裁）⇒ 偏移必须与条带同帧落地、整级重建必须清零。
  - **`max_band_frac=0.25` 与 `min_band_texels=8` 经实现后判定为不需要**：瞬移/传送/多格跳变
    由"位移必须恰为 ±B"这一条判据直接兜住（比 1/4 阈值更严格），薄条带由 `B ≥ 16` 的
    默认档 + 配置建议覆盖 ⇒ 不再引入这两个字段（零冗余）。
  - **验收**：**Test 17**（新增，主用例 3 条不变式 + 4 项补充 + 子用例 (e)）`[CSM-SCROLL] frames=400
    crossings=[349,207,44] 条带覆盖检查=1602 次 命中帧=1135 反向跨格=268 次 off3=(0,0)` +
    `[CSM-SEAM] B=24 M=1024 覆盖检查=2577 次 跨缝拆分=8 次 反向跨格=297 次`；Test 5A
    `full=[600,2,2,3] band=[0,31,15,3]`（滚动不再是整级重建）；5B-1/5B-2/5C 判据同步改为
    "内容刷新帧（整级 或 条带）更新引用矩阵，纯命中帧矩阵不变"；`ATS_SELFCHECK=1` exit 0
    （D1 57.6% / D3 18189px·600662px 契约数值不变）。
  - **GPU 侧联调（临时探针，已撤）**：示例强行走 20s ⇒ `[S2-PROBE] 条带帧=121 累计矩形=124
    非零偏移帧=121 off=(48,1008) rect0=(0,1008,1024,16)`、**0 VUID/[ERROR]**。
  - **破坏验证 ×8（全部有牙，exit 17）**：偏移累加反号 / 写侧 y 符号反 / 条带起点错端 /
    跨缝不拆分 / 跨缝段宽度丢一 / 整级重建不清偏移 / 命中帧多画条带 / 条带宽度多一纹素。
  - **实现期发现（重要）**：`AppendWrappedStrip` 的跨缝拆分**只在 `M % B != 0` 时可达**
    （正向跨格条带终点恰为偏移、反向跨格起点恰为新偏移，偏移恒为 B 的整数倍 ⇒
    `M % B == 0` 时 `start+width ≤ M` 恒成立）；默认 `{16,16,32}`/M=1024 即不可达，
    因此测试用 `B=24` 专门覆盖该分支。另外**深度锚点 `cache_anchor_step` 跨步会清零偏移**，
    是偏移累积的最大杀手（跨缝需连续 40+ 次跨格不被打断）——子用例 (e) 为此把 `anchor_step`
    放到 4096 以隔离横向滚动。
- **S4 ✅（2026-09-26，接缝可达性量化——不需要改 shader，`wrap` 判定**被证安全**）**：
  - 判据（Test 18）：视锥切片 8 角点到方框边界的余量 `(1−max|ndc|)/2·M` 必须 >
    可达半径 `pcf_radius + 1.5m/texel_world`（1.5 = shader 的 `SHADOW_NORMAL_OFFSET_MAX`，
    即 normal-offset 的悲观上限；PCF 侧 Poisson 磁盘半径归一化到 1 ⇒ `1.0·pcf_radius`）。
  - **示例配置实测**：`margin=[23.2,26.5,34.9]` texel vs `required=[13.0,5.3,3.5]`
    texel ⇒ slack `=[1.8×,5.1×,10.0×]`（wrap 生效帧 459/381/308，防空跑守卫）。
    ⇒ 安全，但 **c1 只有 1.8 倍余量**，且余量随 `normal_offset`/`pcf_radius`/切分距离
    调整而消耗（Test 18 已把 `SHADOW_NORMAL_OFFSET_MAX = 1.5` 钉进源码契约）。
  - **两条可迁移结论**：① slack 与分辨率**无关**（实测 M=256/512/1024/2048 →
    1.9/1.9/1.8/1.8×），只随级联世界半径 r0 变化（`slack ≈ 0.17·r0`）——大贴图的收益是
    同一 B 对应更小的世界步长，不是更安全的接缝；② **配置约束：级联世界半径
    r0 ≲ 6m 时 seam 变可达**（Test 18 ③ 用 splits `{1,1,2,4}` 钉住该边界：
    margin 24~34 texel vs required 151~585 texel ⇒ 断言非恒真），这种配置须下调
    `normal_offset_world` 或该级关横向滚动。
  - **不需要"条带内 wrap、带外 clamp"**：那条备选方案针对的是"可见接收者能贴到 seam"，
    而 §4.6 的余量判据已证明够不到（1.8× 最小余量）；反过来若某配置真贴到 seam，
    正确的修法是调配置，不是给 shader 再加一层分支。
- **S5 ✅（2026-09-26，示例 stats 接通 + GPU 侧"整级 vs 条带"对拍工具落地）** —— 当时记的"发现真实差异、根因未收敛"**已由下方 S6 定案（诊断自身相位假象，316 轮 GPU 对拍零差异）**，本行状态是中间态、勿再当开放项：
  - **stats 接通**（`CascadeUpdateStats`/`GetUpdateStats` + 示例 1s 窗口汇总）实测：
    `[CSM Rolling Cache Stats] Cam=(120.3,-28.0,10.0) | C1: 11 strips(band=16t avg=0.29% off=(80,0) full=0) | C2: 4 strips(band=16t avg=0.10% off=(32,0) full=0) | C3: 1 strips(band=32t avg=0.05% off=(0,0) full=0)`
    ⇒ **每帧重画占比 0.05~0.31%**（整级重建是 100%）——滚动方案收益首次量化；
    顺带修掉旧 stats 的误报（原判据 `strips==0 ⇒ "100% Cached"` 会把整级重建帧算成命中）。
  - **对拍工具**（示例内，`CSM_CACHE_DIFF=1` + `CSM_AUTOWALK=<m/s>`，`CSM_CACHE_DIFF_FREEZE=0` 可关冻结）：
    A=环形滚动帧（offset≠0）、B=`InvalidateMainLightStaticShadowCache` 后的整级重建帧（offset=0）、
    B2=紧接着的第二次整级重建；A 按 `phys=(layout+O) mod M` 映射后与 B 逐纹素比。
    判据齐备：**形状分类**（A缺/A多/双方有几何）、**32×32 差异分布图**、
    **位移扫描**（x/y 各 ±24，验"是否整体错位"）、失败时落 A/B/D 三张 BMP（D=|A−B|×5）。
  - **结果（重要）**：`B vs B2 = 0`（整级重建逐位可复现、帧间内容稳定 ⇒ 方法可信）；
    **c2/c3 一致**（1~2 纹素、max|Δ|=1.25e-6）；**c1 每轮不一致 400~1900 纹素**
    （占窗口 0.1~0.3%，max|Δ|≈0.4，集中在剪影处；`A缺` 与 `A多` 并存、`双方有几何` 数十~数百）。
  - **已排除的四条**：① 动画伪影（改成"从第 0 帧就冻结可移动物体"后差异不变，B vs B2 仍 0）；
    ② 整体错位（位移扫描 x/y ±24 最优恒 (0,0) 且残余不变）；
    ③ 写侧平移量不准（`texel_size = 2r/M` 与 ortho `left/right/bottom/top = ∓radius` 严格一致
    ⇒ O texel 平移是精确整数，光栅化应平移不变）；
    ④ 两路径 caster 筛选不对称（`:381` 全量路径对静态级联同为 `mobility_filter=Static`）。
  - ~~**未收敛点**：c1 的差异既非位移也非动画，形态是"剪影处的亚纹素级内容差异"~~
    ⇒ **已被 S6 证伪**（定案 = 诊断自身相位假象：行走中读回 A、随后读到的 `offset` 可能跨帧 ⇒ 整图错误映射；
    修正后 316 轮 GPU 对拍**零差异**）。**别再按"引擎缺陷"追这条**；判据与命令见 S6 与 `csm-mechanism.md` §6/§7。
  - **计数器复核（2026-09-26，S5/S6 交界）⇒ 上面两条"未收敛"推断被证伪，用户日志全项自洽**：
    新增**单调计数器**（`GetUpdateCallCount/GetInvalidateCallCount/GetFullUpdateCallCount/
    GetStripUpdateCallCount/GetHitUpdateCallCount` + 示例窗口差分 + `[CSM Cache Counters]` 行）后实测：
    `Upd=60 Inv=2~4 frames=60 | C1: fullC=2~5 stripC=11 hitC=46` ⇒
    ① **`Upd == frames`**：每帧恰好一次 `Update()`，不存在"同帧多次 Update 覆盖 stats"；
    ② **`fullC == Inv`**：每次 `InvalidateStaticCache()` 都确实让每级各做一次整级重建 ⇒ **失效链正常**；
    ③ **`fullC + stripC + hitC == Upd`**：每帧每级必落一个分支（可作不变式）；
    ④ 用户日志里 `full=0` 满天飞**不是引擎没重建，而是当时那个字段只显示"窗口最后一帧"**
    （60 帧里只有 2~5 帧整级重建，末帧多半是命中）⇒ 已把 `full=` 换成**窗口真实次数**并删掉
    `last_full` 字段（消除误导）；
    ⑤ 用户日志里的大 `off` 是**负偏移的 uint32 表示**（`992 = -32`、`1008 = -16`、`976 = -48`），
    不是"累积 62 次未清零"⇒ 所有 `off` 都是 band 的整数倍，`(992,922)` 应为 `(992,0)`/`(0,992)` 笔误。
  - **待观察（非缺陷）—— 已按 2026-10-07 复核订正**：原注写"24 m/s 时 C2 偶发 `fullC=10`"偏低。
    本日实测（`CSM_AUTOWALK=24`，窗口≈50 帧）：**C2 `fullC=2~18`、C3 `fullC=2~33`**（个别窗口 C3 = 33/48 ⇒ 过半帧整级重建）。
    对照：同一构造 `CSM_AUTOWALK=4` 的对拍 **15 轮 + 15 自查全 0**（S6 定案在 HEAD 仍成立）、
    用户步行速度下重画占比 **0.05~0.16%**（S5 人工验收）⇒ **正常使用不受影响**；24 m/s = 86 km/h 属压测速度。
    **根因未定**（候选：`cache_anchor_step = 16 m`（`CascadedShadowController.h:140`）沿光轴跨步 ⇒
    `along_anchor_` 变化强制整级重建（`:392`）、或格点不等距 ⇒ `shift ≠ ±B` 走回退分支）。
    要定它就插桩 `CSM_PASS_LOG=1` 读 C2/C3 每帧 `kind=`/`off=` 序列 + 计数器差分（本批未做，**优先级低**：无真机需求）。
  - **人工验收（用户 2026-09-26 手动 WASD 行走，未用自动行走）**：向前移动**无可见接缝**；
    静止时静态级联**零更新**（`avg=0`，`100% Cached`）；`avg` 仅在**跨格的那一秒**出现
    **0.05~0.16%**（`off` 值同时也大）。⇒ 与自动行走 24 m/s 的 **0.29~0.31%** 互证：
    `avg ≈ n_crossing × (B/M) / fps`（每次跨格重画 `B×M` 纹素 = 贴图的 `B/M` = 1.5625%），
    B=16/M=1024/60fps ⇒ **1 次跨格/秒 = 0.026%** ⇒ 用户实测 **0.05% / 0.16% = 每秒 2 / 6 次跨格**；
    自动行走 24 m/s 的 **0.29~0.31% = 每秒 11~12 次跨格**（格长 `L = B·texel = 2.1m`、0.4m/帧 ⇒ 每 ~5 帧一次）✓
    **三组数字自洽 ⇒ 条带滚动在按设计工作**（`shift_x/shift_y` 各自必须 `=0` 或 `±B` 才走滚动，
    见 `CascadedShadowController.cpp:414-422`；**`avg=0` 是"该秒内没跨一个 texel"= 完全命中，
    不是失效**）；`off` 大 ⇔ "那阵子一直在滚"（整级重建会把 off 清零）⇒ 大 off 与 avg>0 同源不同因。
    代价换算：12 跨格/秒 = `12×1.5625% = 18.7%/秒`，而每帧整级重建 = `100%×60 = 6000%/秒`
    ⇒ **约 320× 的栅格化节省**（用户步行速度 2~6 次/秒 ⇒ 3~9%/秒 ⇒ 约 600~2000×）。
    人工也说明 S5 那条"条带内容 ≠ 整级重建"的差异在**当前示例配置下不可见**
    （与 S4 的 1.8× seam 余量结论一致）——该项已由 S6 查明为**诊断相位假象**（见 D5/S6）。
- **S6（新，S5 直出）**：定位"条带重画 vs 整级重建"内容差异的根因。
  - **已落地的仪器（用户选定"走引擎侧打日志"）**：`CSM_PASS_LOG=1`（默认静默）打印两类行：
    `[S6-PASS] cascade=N kind=full|strip mobility=M off=(x,y) view_t=(..) rect=(x,y,w,h) load_depth=D`
    与 `[S6-COLLECT] shadow pass mobility=M items=N idsum=0x… skipped(invisible/no_owner/no_transform)`
    （`RenderPrimitiveCollectSystem` 在收集末端统计产出图元数 + Σ(entity_index<<16|gen) 校验和）。
  - **已排除（实测）**：
    ① **收集/剔除侧**：条带帧与整级帧收到的 caster **完全同一批**（items=84、idsum=0x14820000、
    mobility 与 skipped 计数都一致）⇒ 不是"少了/多了 caster"。
    ② **写侧矩阵的浮点非不变性**：`P·T·V` 与 `P·V` 的差恰为理想值 16 纹素，**残差 0.0001 纹素**
    （Test 20 量化）。
    ③ **"格内矩阵恒定"前提**：纯平移 300 帧里矩阵逐位不同的帧有 278，但相对漂移仅
    **1.465e-07**（换算到贴图边缘 **0.0001 纹素**）⇒ 量级远不足以解释差异。
  - **关键线索**：差异 bbox=(509,186)-(1023,614)，**不含新画的条带**（条带在 `x∈[0,16)`，
    那里 0 差异）⇒ 条带写入本身是精确的。
  - **根因（已定）**：**原"对拍不一致"是诊断自身的相位假象，不是引擎缺陷。**
    对拍在相机**仍在行走**时读回 A：读回的深度图与随后读到的 `offset` 状态可能跨帧
    （相差一个跨格）⇒ 整图被错误映射，退化成"整图错位/平坦区差异"（实测最优位移顶到扫描
    边缘 `(-24,0)` 且 `残余=0`＝纯整数错位，正是这种签名）。
    **修正**：达到目标跨格数后立刻停走 + 静置 5 帧再读回（`CacheDiff::pending/settle`）。
  - **修正后的实测（GPU 侧对拍 A vs B 逐纹素）**：
    ① 停走捕捉 **239 轮 ⇒ 每轮 `不一致纹素总数=0`（平坦区 0）**，自查 B vs B2 也 0
    ⇒ 条带路径与整级重建**逐纹素相同**；
    ② 再把每轮"先行跨格数"递增（1,2,3,…，封顶 70）扫过环形偏移：**77 轮全部 0 差异**，
    offset 覆盖 16→976（几乎整圈），其中最后若干轮累计 70×16=1120 texel ⇒ **环形偏移
    已越过贴图尺寸发生回绕**，回绕后的写入/读侧映射同样逐纹素一致。
    **合计 316 轮 GPU 对拍零差异**。
  - **可复用教训**：任何"读回 GPU 资源 + 同时读引擎状态"的对拍/诊断，必须先在**状态静止**
    下取样，否则行走进程中取样会得到假差异（本次差点据此误改引擎）。
  ⇒ （原 RenderDoc 抓帧方案未采用：用户选定引擎侧日志路线，见上。）
  （E1 影像读回下沉 + CM2D TGA 仍是独立留置项，与本条无依赖。）

### D6. prepass 入口防御（T4） ✅ **已完成（`f1df79a5e`，2026-10-07）**

> **已落地**：prepass 内聚到 `BeginManagedRenderFrame`，判别式 = **主帧**
> （`need_swapchain_acquire`；离屏帧一律传 false）⇒ 离屏帧不重复 prepass、嵌套的阴影 pass 不递归，
> 位置保持 `AcquireSwapchainImage` 之前；`RenderGraph.cpp` 的单入口调用删除。契约 = Test 42 的
> **M12 两条**（正向钉「守卫 + 调用」一起在，禁复活钉 RenderGraph 不得再出现该调用）；带牙验证
> （破坏 A=守望去守卫 / B=单入口复活 ⇒ 各 rc=42 点名 M12；逐字节还原复绿）。行为中性：八台架摘要逐位不变。

- **现状**：自动化 shadow prepass 仅在 `RenderGraph.cpp:171` 一条入口被驱动，
  新增/绕过入口时阴影静默失效（无断言无日志）。
- **做法**：挂到 `BeginManagedRenderFrame` 并显式区分主帧/离屏帧，或入口
  缺失时告警。
- **规模**：~30 行。**触发条件**：新增第二条渲染入口时。

### D7. 性能账目 → EnvironmentSystem 拆分 → 4 级联合并（T8 → T6 → A6，大组按序）

> **状态（2026-10-07）：三项全部有结论 ⇒ 本卡关闭** —— T8 ✅（性能账目）、T6 ✅（T6a 搬运 + T6b 四步 + V7 台架）、
> A6 ❌（判为不成立并关闭，用户裁定）。T6b 的落地细节与实测数见下。

- **T8 ✅ 已完成（`f8760056b`，2026-10-07）**：探针 `PREPASS_TIMING=<N>`（默认零成本）量的是 CPU 可见账。
  实测（`CascadeShadowMap` 1600×900 稳态 60fps、50 窗口 = 1500 帧）：**离屏 pass = 1.00 次/帧**、合计
  **3.76ms/帧（占主帧 ≈19%）**，细分 = **begin 2.32ms**（`PrepareRenderPassSetup`：collect/批装配/派发）
  + draw 0.23ms + **submit 0.047ms** + 上沿等主帧 0.83ms（**突发**，最大 4.39ms）+ 下沿等自身 0.23ms。
  ⇒ **A6 的前提不成立**（「4 次 submit + 4 次全槽排空」不存在）⇒ **A6 已关闭（见下条 + 用户裁定）**；
  真正的大头是每 pass 的 `PrepareRenderPassSetup`（2.31ms/帧 ≈ 13.9%），属独立优化项。数据与结论见
  `doc/csm-mechanism.md` §8.5。
- **T6 → 拆成 T6a/T6b 两步（用户 2026-10-07 拍板；详见 `doc/future/lighting-shadow-domain-split-plan.md`）**：
  · **T6a ✅ 已完成（`e355f0da2`）**：CSM 域自 `EnvironmentSystem` **原样搬出** `MainLightShadowSystem`
    （12 方法 + 9 成员 + 日志前缀；`EnvironmentSystem` 只留天空/IBL，.cpp 427→99 行）。判据实测：
    八台架摘要逐位不变（D=0）+ ENGAGED x==y + 拒绝 0、ATS rc=0、离屏两例逐位不变、门 45/0、
    TestCSM 42 Passed（**T6a 当时基线**；Test 9D/10D 与三条 SlotGuard 契约已随内容改指新档）。
  · **T6b ✅ 已完成（4 步 + V7 台架，2026-10-07）**：
    ① 加法·结构 `10edaf040`（`LightRow` POD + 编译期契约）；② 加法·接线 `4d53abb34` + `b576daa7a`
    （`addr_lights` + `LightRowTable` + 门 `W.light-row-struct-parity` + Test 44）；
    ③ 减法 `dc49c0dd9` + `24cfb90e6`（`SkyInfo` 删 `sun_*`/`moon_*` 与 `halo_color`、光源定义 = 世界光源行 0
    （`LightSystem`）+ 影随光、四处 shader 改读光行、删三份死头、`CascadedShadowController.*` 归位 `render/shadow/`）；
    ④ P4 `ada3fa48b` + `b46fced20`（`ShadowInfo` 移出 `EnvironmentInfo` 并**世界私有化**：CPU 权威
    `ECSContext::shadow_info` + `InitializeShadowInfoTable` + 提交点 `CommitShadowInfo`；profile 侧 shadow ring /
    `GetShadowAddress` / `MaterializeShadowUBO` / `CommitMaterialized` / `MarkShadowDirty` 整删）；
    ⑤ **sky 段整删（2026-10-07，场景 A）**：`SkyInfo.h` 删除（两个外观字段 `base_sky_color` /
    `halo_intensity` **全仓无写者** ⇒ 取值搬成 `sky_atmosphere.glsl` 常量 `kBaseSkyColor` / `kHaloIntensity`，
    外观逐位不变）、`addr_sky` 退出世界表（80 → **72 B**）、GLSL `SkyInfoRef` / `#define sky` /
    `EnvironmentManager`（`sky_ubo` / `MaterializeSkyUBO` / `GetSkyAddress`）/ `EnvironmentSystem` 三件套整删
    ⇒ `EnvironmentInfo` 只剩 `ambient_env`；Test 45 的禁令改钉 `EnvironmentInfo.h`、Test 47 加 ⑥「sky 整删」11 条；
    ⑤ **V7 运行时台架** `c26df6009` + `79997b2d2`（`example/Shadow/DualWorldShadows.cpp`：两世界同帧各自
    开 CSM，翻转一个世界的主光 ⇒ **另一个世界帧摘要逐位不变**、自身必变；破坏验证证明判据有牙。
    顺带补两条引擎前提：级联 RT 数 = `CascadedShadowConfig::cascade_count`（离屏帧槽带全设备 4 条）、
    影子数据上传点 = 本世界自己的帧 RT（否则离屏世界不可能有阴影））。
    · 天空侧数据面的后续（不属 T6b）：T0 判据前置 `c67d09037`、T1 IBL 句柄归位 `c04d789a4`（`AmbientEnvInfo` +
      `addr_ambient_env`）、T3 触发条件改写 `d3c87ef86` —— 见 `doc/future/sky-domain-and-provider-params-plan.md`。
    · **判据实测**：门 **46/0**；`TestCSMIncrementalPass` rc=0（**48 个测试**；Test 22 = 25 条两世界隔离）；
      八台架 + RTT/RTTCD 摘要逐位不变；ATS rc=0；环境两例自检 rc=0；`DualWorldShadows` `DWS_SELFCHECK=1` rc=0。
  · **A10 现状订正**：其「重复 Enable 重建 4 RT」一半已由 C3①（槽带归还，`d9b2c6886`）消解（重复 Enable
    现为资源正确）；剩余价值 = 结构/头文件重量 + RAII（T6a 已兑现结构那一半）。
- **A6 ❌ 已判为不成立并关闭（2026-10-07，用户裁定「4 级联更新频繁都不一样，怎么会合并 submit」）**：
  三条结构事实各自都否掉「合并」——
  ① **每级更新节奏独立**：`need_full_update` 是**逐级联**触发的（级联0 / 锚点失效 / **位移非整步滚动**），
     中远景走静态滚动缓存 + **条带脏矩形**（跨格才整级重建）⇒ 同一帧里「哪几级要渲、渲多少」各不相同，
     **不存在「每帧 4 次 submit」的稳态形态**（实测稳态 = **1.00 次/帧**）；
  ② **每级 caster 集合不同**：`mobility_filter` = 级联0 收 **Movable**、1..3 收 **Static** ⇒ 没有「一份共享的
     prepass」可合并——合并要么遍历两遍（= 没合并），要么推翻「静态/动态分离」（**用户已裁定，勿翻案**）；
  ③ **条带是局部写**：strip 更新只写脏矩形（scissor）；合并成「单次 prepass」会把条带变成整图重建 ⇒
     放大写带宽，**把 D5 省下的又还回去**。
  ⇒ 能合并的只有 submit（实测 **47µs**）与部分等待；而每 pass 的 `PrepareRenderPassSetup`（**2.31ms**，
  collect/装配/派发）**按级联不可省**（各级光矩阵 / caster 集 / scissor 都不同）。**A6 关闭**；prepass 性能
  若要做，目标是「per-pass 装配 2.31ms」与「上沿等待突发（中位 0.27ms / 最大 4.39ms）」，与「合并级联」无关。
- **规模**：T8 半天（工具已有 DumpCascadeDepth 基础）；T6 2-3 天；A6 1-2 天。

### ~~D8. FS 剥除豁免的判据收敛~~ ✅ 已完成（2026-09-26，实测裁决：**删程序级扫描**）

- **实测证据（三层判据探针，AlphaTestShadow 实跑 `[D8-PROBE]`）**：所有程序
  `spirv-scan=0`、`text-scan=0` → `IsFragmentShaderRequired()` **恒 false**；
  `keep_fs=1` 只出现在 `recipe_at=1` 的程序上 ⇒ **唯一真正生效的判据是 recipe
  语义**，此前文档/提交声称的"SPIRV 扫描撑住 masked 影子"不成立：
  - SPIRV 扫描：`OpKill = 101`（真值 **252**）、cap `5407`（真值 **5379**）全错
    → 从不命中；且**唯一调用点在 stage 缓存命中分支**（冷缓存首编译根本
    不扫）。
  - FinalGLSL 文本扫描：结构性失明——实测 `shader-cache/stage/stage-16-*.frag`
    含 `HGLApplyAlpha(...)` 调用而 **0 处 `discard` 字面量**（include 的
    alpha_compositor 不在该文本里；glsl_len 12551/16673/16732 全部 0 命中）。
  - 且该处是**硬赋值**（`fragment_shader_required = ...find("discard")`），会把
    SPIRV 扫描的 true 覆盖成 false ⇒ **修常量也无效**，必须同时改三处。
- **裁决依据**：① 两层程序级实现在实测中都是死的；② 引擎全部路径的模板选择
  与 FS 保留用**同一输入**（normalize 后的 recipe `alpha_test`，见
  `ShaderProgramManager.cpp` 的 `masked ? ShadowCasterMasked : ShadowCasterOpaque`）
  ⇒ 未声明 alpha_test 的材质根本不会生成带 discard 的影子程序（ShadowCasterOpaque
  里没有 discard），判据完备；③ 符合零兼容偏好，不留恒假机制。
- **落地**（净 −45/+24）：删 `ScanSPVHasDiscard`、文本扫描块、
  `ShaderProgramManager::SetFragmentShaderRequired` 中转、
  `ShaderProgram::fragment_shader_required` 与 `IsFragmentShaderRequired()`；
  `RenderPass::CreatePipeline(ShaderProgram*, const MaterialRecipe&)` 判据收敛为
  `render_state.alpha_test || render_state.dither`；config 重载（line/text 颜色
  通道）显式传 `false` 并注明"depth-only 需求须走 recipe 重载"；原位置留注释
  说明被删原因（**勿再引入第二套判据**）。
- **禁复活契约**：Test 11 新增 2 条反向 needle（`ScanSPVHasDiscard`、
  `FragmentShaderRequired` 必须不存在）。
- **验收**：Debug 构建通过；`TestCSMIncrementalPass` 全绿（Test 11 = 11 checks）；
  `ATS_SELFCHECK=1` 跑 `AlphaTestShadow` → c0 仍 **57.6% PASS**（**删除 = 行为
  no-op**，实测证实）；`CascadeShadowMap` 冒烟无回归。
- **若改走"程序级扫描"**：属**重做**而非修常量——需扫在模块创建处并覆盖编译+
  缓存两条路径、OR 语义、补 `OpTerminateInvocation=4416`/
  `OpDemoteToHelperInvocation=5380`/cap `5379`，且删掉文本扫描（它只会覆盖结果）。

### D9. 行未就绪跳过路径的告警与收敛（A1-4 残留） ✅ 2026-09-26

- **原状**：`RenderPrimitiveCollectSystem` 阴影分支对 "masked caster 行未就绪" 的处理
  是**静默** `BumpStaticSceneRevision()` + `continue`（设计意图首帧收敛）；程序解析/
  几何/管线三条失败路径虽各有告警，但**逐帧刷屏**。四条路径都**无上限地每帧 bump**
  ——持续失败 = 静态级联每帧全量重画（`100% Cached` 再不出现）+ 该 caster 影子长期
  缺席，且（行未就绪那条）全程无日志。
- **落地**：per-primitive 计数 `MaterialComponent::shadow_retry_frames` +
  统一收敛入口 `RenderPrimitiveCollectSystem::AdvanceShadowRetry(material_comp, reason, primitive)`：
  1. 四条跳过/失败路径（行未就绪、程序解析、几何、管线）全部改走该入口，
     **原因字符串随调用点传入**（行未就绪 = `masked caster runtime rows not ready
     (forward chain has not materialized them)`）；程序解析失败的逐帧告警**移入
     forward 分支**，阴影侧只由收敛入口输出一条（不再双重刷屏）。
  2. 首次跳过 → `GLogWarning` **一次**（primitive 名 + 原因 + "前 120 帧每帧 bump 以收敛"）。
  3. 连续 `kShadowRetryFullBumpFrames = 120` 帧仍跳过 → `GLogError` **一次**，并把
     bump 降频为每 `kShadowRetryBumpPeriod = 60` 帧一次（**限速自愈**：不再每帧全量
     重画，但保留周期性重试，避免影子永久缺席）。
  4. 该 caster 成功产出本帧 render item（`shadow_program` 非空）→ 计数清零，
     下次失败重新告警、bump 回到每帧。
- **验证**：
  | 项 | 结果 |
  |---|---|
  | `TestCSMIncrementalPass` | exit 0；**Test 13 = 7 checks**（收敛入口/原因串/上限判定/降频公式/计数字段 + 2 条禁复活"逐帧刷屏告警"） |
  | `ATS_SELFCHECK=1 AlphaTestShadow` | exit 0，c0 仍 `57.6%`（bbox=112x58 filled=3740）；**跳过告警恰好 2 条**（MaskedCube / FallbackCube 各一条，均在第 1 帧）、达上限 0 条 → 一帧内收敛、无刷屏 |
  | `CascadeShadowMap` 冒烟 | 路由 105/100/4、0 VUID/ERROR、管线 4 Created/205 Reuse 与基线同值；告警 4 条（4 个 masked caster 各一条，首帧）、达上限 0 条、静态级联失效全程 2 次 |
  | **注入验证（门有牙）** | 把条件临时改成恒 `shadow_needs_rows`（强制持续跳过）→ **每 caster 恰好 1 条告警 + 1 条达上限报错**（共 4+4，无刷屏）；静态级联 revision 终值 **565** ≈ 4 caster × [120 + (N−120)/60]，N≈1380 次跳过帧 ⇒ 降频公式逐字命中（不限频应为 4×1380≈5520 次），即"限速自愈"确实生效。还原后复跑绿 |
- **教训**：`GLogInfo/GLogWarning/GLogError` 展开为 **`{...}` 块**（`CMCore/inc/hgl/log/Log.h:164-168`）
  → `if (x) GLogWarning(...); else ...` 触发 **error C2181: illegal else without matching if**
  （宏自带花括号 + 调用处 `;` 变空语句 ⇒ if 已闭合）。**if/else 链里用日志宏必须给分支加花括号。**
- **规模实际**：核心 ~50 行（含注释）；契约 Test 13 ~50 行。

### 留置

- **T5 ScenePipelineMode 空壳**：用户明确留置（未实现模式不加告警）。

## E 线：未来项（诊断工具下沉，待排期）

### E1. 附件读回落盘下沉为引擎基础功能 + 图像写出改用 CM2D（2026-09-26 用户留置）

**状态：✅ 已落地（2026-09-26）**。落点：
- 引擎新 API：`inc/hgl/vk/VKTextureReadback.h` + `src/Vulkan/VKTextureReadback.cpp`
  （`ReadbackTexture` / `ReadbackColorTarget` / `ReadbackDepthTarget`；同步、帧外、读回前后布局不变；
  原始字节行主序自上而下；`GetStrideByFormat` 定每像素字节数；已注册进 `src/Vulkan/CMakeLists.txt`）。
- 配套引擎修复：`RenderCmdBuffer::BeginRendering`/`EndRenderingPresent` 把 `newLayout` 同步写回
  纹理布局跟踪（`Texture::SetImageLayout`）——这条是**根因修复**：交换链颜色图此前跟踪值停在
  `SHADER_READ_ONLY_OPTIMAL`，示例只能硬编码 `PRESENT_SRC_KHR`；同时
  `VKBindlessTextureManager` 对不可采样布局回落 `SHADER_READ_ONLY_OPTIMAL` 注册。
- 示例：ATS `DumpCascadeDepth`/`DumpColorTarget`、CSM `ReadbackCascadeDepth`/`SaveDepthTga` 全部改调引擎 API；
  手写 BMP 头改 `bitmap::SaveBitmapToTGA(&out, rgb, w, h, 3, 8)`（CM2D，UPPER_LEFT 行序与读回一致，
  底行翻转随之删除）；诊断图扩展名 `.bmp` → `.tga`。
- 验收（全部满足，方法：与改造前日志逐项比对）：`ATS_SELFCHECK=1` rc=0；c0 填充率 57.6%
  （bbox 112x58 filled 3740）；D3 `18189 px / 0 px` 与 `600662 px` 不变；`mean_lum=113.3/112.3/134.0` 不变；
  `CSM_CACHE_DIFF=1 CSM_AUTOWALK=24` 74 轮 `不一致=0`；`TestCSMIncrementalPass` **21 Passed**/rc=0
  （新增 Test 21 源码契约 + 破坏验证双向咬住）。
- **遗留（新发现，非本次引入）→ ✅ 已解决（2026-09-27，见 `doc/csm-readback-and-leak-followup.md` 第 1 项）**：
  从帧外读回**交换链颜色图**曾触发 `vkQueueSubmit(): ... presentable VkImage ... has not been acquired`
  （改造前同样存在：旧代码硬编码 `PRESENT_SRC_KHR` 转的就是这张图，实测拷贝有效）。现在改为：
  交换链 RT 维护「颜色访问窗口」状态（`NextFrame()` 开、`Submit()` 收尾关）+ 帧内回读钩子
  `SwapchainRenderTarget::SetInFrameReadbackHook()`（在「本帧提交后、present 前」执行一次，
  示例在其中调 `ReadbackColorTarget`），读回侧按 `IRenderTarget::IsColorReadbackWindowOpen()`
  拒绝窗口外请求 ⇒ 校验层消息 0、契约数值逐位不变。
- **落盘精度与命名（2026-09-26 追加，用户裁定"走 A 方案 + 文件名写清楚宽高和格式"）**：
  诊断文件名统一 `<stem>_<W>x<H>_<tag>.<ext>`；深度除 8bit 可视化外一律**裸 float32 全精度落盘**
  （`_f32.raw`，零转换，numpy `np.fromfile(dtype='<f4').reshape(h,w)` 直读）；深度可视化改为
  **真单通道** 8bit TGA（`_r8.tga`，CM2D channels=1 ⇒ image_type=3，1MB/1024²）。
  独立验证（Python 复算，不经 C++）：`cascade_depth_c0_1024x1024_f32.raw` 复算 = filled 3740 /
  bbox 112x58 / 57.6%（= D1 契约）；`ats_d3_A_..._A2BGR10UN.raw` 复算 mean_lum = 113.3（= 日志值）。
- **⚠ 颜色目标实测是 10bit**：交换链颜色图 `VK_FORMAT_A2B10G10R10_UNORM_PACK32`
  ⇒ 旧 8bit BMP/灰度分析一直在**截断低 2bit**（D3 契约数值不受影响、仍有效，但精确分析必须读 `.raw`）；
  颜色落盘命名已标注源格式与截断视图（`_A2BGR10UN.raw` + `_A2BGR10UN_low8x3.tga`）。
  CSM 的 `csm_cachediff_c*_{A,B,D}` 同步落 `.raw`（D 标 `f32x5`）+ `_r8.tga`，但该分支只在"平坦区真有差异"
  时触发（S6 结案后差异恒 0），本会话未实测触发。

<details><summary>原始录入（2026-09-26 留置时）</summary>


- **现状**：`example/Shadow/AlphaTestShadow.cpp` 自带两套一次性取证工具，形态都是
  「RT 附件 → staging buffer → CPU → **手写 BMP**」，且各自重复实现了一遍
  immediate submit / 首尾 barrier / 布局还原 / staging 生命周期：
  - `DumpCascadeDepth(rt, name, DepthFillStats*)`（`:167-322`）：D32 深度附件（DEPTH
    aspect）→ 读回 → 8bit 灰度 BMP（reversed-Z：近亮远暗，镂空=clear 值）+
    同遍算 c0 包围盒内填充率（D1 契约）；
  - `DumpColorTarget(rt, filename, out_lum)`（`:324-470`）：`rt->GetColorTexture(0)`
    （COLOR aspect）→ 读回 → BMP + 逐像素低三字节均值（D3 契约）。
  - 两处都手写 BMP 头 + 底行翻转，且都带着同一条教训：**交换链颜色图在提交时点的
    真实布局是 `PRESENT_SRC_KHR`**，`Texture2D::GetImageLayout()` 停在
    `SHADER_READ_ONLY_OPTIMAL`，拿它当 oldLayout 会被校验层判非法转换。
- **目标（两件事）**：
  1. **读回下沉**：把「immediate submit + old/new layout barrier（含提交时点真实布局
     修正）+ staging 创建/回收 + aspect 选择」封装成引擎侧一次调用
     （挂在 `RenderTarget`/`Texture2D` 上的 `ReadbackImage(aspect, ...)` 之类，
     `src/Vulkan/` 内，目前全仓只有 `VKCommandBuffer.h` 出现过
     `vkCmdCopyImageToBuffer`，没有可复用的读回工具），示例侧只留
     「取哪张图 + 怎么判读」。
  2. **图像写出改用 CM2D**：删掉示例里手写的 BMP 写入，改用
     `CM2D/inc/hgl/2d/TGA.h` 的 `hgl::bitmap::SaveTga(filename, bmp)`
     （模板适配 `hgl::bitmap::Bitmap<T,N>`：`GetData/GetWidth/GetHeight/GetChannels/
     GetChannelBits`），或 `CM2D/inc/hgl/2d/BitmapSave.h` 的
     `SaveBitmapToTGA(os, data, w, h, channels, bits)`；容器/头/字节序全部交给 CM2D，
     不再自研。
     **坑**：BMP 是底行在前，TGA 用 `TGAImageDesc::direction` 表达方向
     （0=lower-left / 1=upper-left）⇒ 迁移时必须显式选对方向，否则读回的图上下翻转
     （现有 BMP 代码正是靠「底行在前」翻转过一次）。
- **规模估计**：引擎侧读回下沉 ~150-200 行；示例删 ~250 行；CM2D 写出替换 ~30 行。
- **触发时机**：不影响引擎正确性，**不排在本轮**；下一次需要图像/深度取证时顺手做，
  或与 **T8 量测**（性能账目，届时需要多次读回取证）一并做。
- **验收**：示例删掉自带 BMP 代码后仍能产出 D1/D3 契约（`ATS_SELFCHECK=1` exit 0、
  c0 填充率 57.6%、D3 `18189 px / 600662 px` 不变）；新 API 配
  `TestCSMIncrementalPass` 源码契约或单测；`res/`（用户自管）不动。

</details>

### E2. gather 侧运行长度合并：把「一项一条 ICB 命令」合批成实例（2026-10-05 用户留置）

**状态：🟡 已开工（2026-10-07）—— 技术文档（研究用）= `doc/instance-batching-and-static-bake-design.md`；执行方案 = `doc/future/e2-gather-run-length-merge-plan.md`（本稿定"怎么做 + 怎么验"：硬约束=合并命令的实例行号必须连续；形态 A 块内合并（推荐）/ B 全局扫描；判据必须换维度=命令数 + ΣY==项数）。** 现象详版 = `doc/future/entity-rows-gpu-cull-plan.md` §8.10 §八。

- **现象**：`GPUDrivenCull` 接管后 ICB 内 64 个 cube 是 **64 条 `(x,1,1)` 命令**（用户 RenderDoc 截帧实测），
  而 CPU 账路径是 **1 条 Y=64**（`src/ecs/support/PrimitiveBatchPipeline.cpp:733`
  `groupCountY = instance_count > 1 ? instance_count : 1`）；**像素一致 ⇒ 现有判据无感**。
- **为什么是欠账而不是 bug**：合批机制已在（`gl_InstanceIndex = first_instance + gl_WorkGroupID.y`，
  `src/ShaderGen/meshgen/MeshShaderHeaderGen.h:123`；**Y 维度 = 实例维度**）；本轮 ④ 只清了组件多实例，
  **CPU 侧自动合批保留** ⇒ 由「CPU 合、GPU 不合」的不对称产生。
- **要做的事**：gather 内按「相邻幸存项同几何（及发射相关行）」做**运行长度合并** ⇒ 命令数
  从「项数」降到「运行段数」。
- **代价**：需 run-start 标记 / 前缀和（gather 现为每项独立 invocation 的纯并行模型）⇒ 属并行性改造，
  **故不夹进 A6-b 机制层**（会污染机制层判据）。
- **判据**：**命令数**（回读 ICB 计数）= 1 条 + `Σcount` 不变 + 像素三方（CPU 账 / 仅 dispatch / 接管）全等；
  **像素对命令数永远无感**，必须补数值判据。
- **触发时机**：不影响正确性；与 Text/调试线无依赖，排在机制层 + 内容线之后；大批重复网格场景收益最大。

## 相机 / 地址表归口（2026-09-28 定稿）

权威口径与执行计划：`doc/world-addresses-and-camera-model-plan.md`。

- **三档地址分层**：`GlobalAddresses` = 跨世界共享资源池（MeshDrawParams 池、PBR/Emissive/Transmission 行池、
  调色板、**viewport**）；新增 **`WorldAddresses`** = 世界私有（相机行 / 渲染项表 / DrawItemID 表 /
  sky / shadow / env）；`pc_root` = 每批 / 每材质 / 本字体（新增 `addr_world_addresses`）。
- **相机下沉世界级**：世界私有 SSBO，**16 槽 × 8 帧槽**，0 号槽恒为本世界默认相机；
  三级解析（默认相机 → 最小 `EntityID.index` 相机 → `(0,0,0)` 强制常驻 fallback）；
  光相机随"投影阴影的灯光"创建、镜面未来同理（拥有者负责申请/归还）。
- **Env 随世界**：profile 生命周期跟随世界；sky / shadow / env 地址按世界发布（现状写全局字段 ⇒
  多世界同帧只有最后解析的 profile 生效，互踩）。
- 批次：**C0**（ShadowMap 相机行污染修复，已完成待提交）→ **C1**（双表 + 相机收口）→ **C2**（Env 归世界）
  → **C3**（灯光/镜子相机通用化）→ **C4**（ComponentData 骨架：CameraComponent 样板 → Transform/Geometry/Material）。
- 事实订正清单（`RootAddresses` 80B、`GlobalAddresses` 已是 SSBO+8 槽、Scene 集退场、`camera_row` 改名、
  "光源相机属另一个世界"是错误归因、4-ID 消费开关未启用 …）见该文档 §7。

## 关联顺序

```
A4(shadow 集成) ──建议在──► A2(RenderPassRequest) ──前置──► A3(跨RT pass链)
                                │                          │
A5(比较采样) ◄──同做───────────┘                          ▼
                                                   A1(提交原语) ──► A7(in-flight槽)
```

- **性价比最高的入口是 A2**：解锁 ShadowMap hack 清理（验证用例现成）
  与 A4 的光照矩阵通路，且不依赖任何其它项。
- B/C 线与 A 线无耦合，随手清。
- **D 线内部顺序**：~~D1~~ ✅（已锁死 masked 链：Test 11 源码契约 + AlphaTestShadow
  自判）→ ~~D8~~ ✅（实测裁决：程序级扫描恒 false → 删除，判据收敛为 recipe 语义）
  → ~~D2~~ ✅（pipeline 键改 SPIRV 内容 hash + 已解析管线身份改 program digest；
  Test 12 = 7 checks。同构遗留：`resolvedRuntimePipelineMap` 的 `RenderPass*` 键
  在**运行期 RT 重建**时才会变成悬垂，锁 A6/D5）→
  → ~~D9~~ ✅（阴影跳过路径统一收敛：一次性告警 + 120 帧上限后降频 bump，
  Test 13 = 7 checks）
  → ~~D3~~ ✅（接收侧旋钮落地：per-draw 行 8B→16B +
  行结构唯一真源 X 列表；Test 14 = 22 checks + 运行期逐像素对照/破坏验证）
  → ~~D4~~ ✅（静态写入语义化 A′：运行期写 Static 一次性告警 + 引导
  `SetMobility(Movable)`；Test 15 = 11 源码 + 5 行为契约，探针实测每帧同值写
  ⇒ 15s 内 872 次级联失效）→ **D5（决策项）** → **T8 量测** →
  T6 拆分 → A6 合并。D 线与 A 线 A1/A7（提交原语/in-flight 槽）强相关：
  A6 的 4 次全槽排空问题在 A1 的 per-frame 多份化落地后可能自然消失，
  两者做前先对齐。
