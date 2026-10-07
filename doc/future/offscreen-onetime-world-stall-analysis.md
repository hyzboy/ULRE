# 离屏一次性世界（RenderOnce）永不绘制——专项分析与交接

> **用途**：本文是 `RenderToTexture` / `RenderToTextureColorDepth` 离屏内容空白问题的
> **独立分析档案**，为"另开会话专门处理"准备：含症状定性、完整取证时间线、四层叠加
> 根因、当前工作树的在途修复（未提交）、**验收判据清单**、以及仍未解释的疑点。
> 新会话从 §6「待办与验收」开工即可，**不要重走 §3 的取证老路**。
>
> **状态（2026-10-07，最终验收已跑）**：修复①②③④已落**工作树、未提交**；
> 全量构建 RC=0；回归全绿（八台架逐字节对基线 / ATS 契约 / ATU 05149=0 / 门 45=0 / CSM PASSED）；
> **但主判据未过**：接管链已全通（`出口=ok`+`ENGAGED`+标签/布局全对），离屏世界派发后
> `DrawCount=0 / b0=0` 依旧——新决定性证据见 §7 P1（实体行 `transform_id=0`）。
> ⇒ **本档就是新会话的任务书：从 §7 P1 接手，勿重走 §3。**
> 已入库前史：`71cc6390c`（pass 身份推导=层①）、`b0430f3c1`（AABB 哨兵=同期 2D 全黑的
> **另一个**根因，与本问题无关，勿混淆）。
>
> 关联档案：`doc/future/gpu-cull-flicker-postmortem.md`（交换链闪烁三线结案史）、
> `doc/future/entity-rows-gpu-cull-plan.md` §11（原始测量记录，本问题=§11.17 待追加）。

---

## 1. 症状与定性

- **现场**：`RenderToTexture`（离屏球 → 贴主场景立方体）与 `RenderToTextureColorDepth`
  （color+depth 双输出）的**离屏内容为空**。用户 RenderDoc 截帧：离屏 pass 的 cmd 里
  **只有 `vkCmdBeginRendering/vkCmdEndRendering`，中间零条绘制指令**；主场景立方体因
  贴的是空纹理而异常。
- **对照**：`ShadowMap` 的离屏（同一 `ECSContext::RenderTo` 通路）**正常**。
- **定性**：这不是"渲染错误"，是**该 pass 根本没有绘制**——批从未被 GPU 接管，
  而 A6-d/T4c4 删净 CPU 绘制兜底之后，"未接管"≡"不绘制"（引擎不变式：**不接管=什么都不画**）。

## 2. 为什么只有这两个示例中招（拓扑独特性）

| 维度 | RTT / RTT-ColorDepth | ShadowMap / ATS / Cascade / 六台架 |
|---|---|---|
| 离屏世界 | `OffscreenWorld` **独立子世界**（自有批表/自有 `GPUCullSystem`） | 主世界自渲 shadow RT（共享批对象） |
| 离屏驱动方式 | **`RenderOnce()`——一辈子只渲染一帧**（Init 时） | 每帧驱动（每帧一次 `RenderTo`） |
| pass 身份 | `req.target == 子世界自己的 render_target` ⇒ 判 Main/forward ✓ | `is_shadow_pass` 显式声明 ✓ |

⇒ 只有"**独立世界 × 单帧渲染**"的组合同时踩满 §3 的四层。**每帧驱动的世界会在第 2 帧
自愈**（各层缺陷本质都是"晚一帧"），所以从未暴露——这是本缺陷族潜伏至今的原因。

## 3. 取证时间线（四层根因的发现顺序）

> 手段备注：`ECS_TAKEOVER_TRACE=1`（guard 出口/flush 逐帧追踪）、`CULL_SSBO_DUMP=1`
> （派发后全量回读，见闪烁案后转正的 `GPUCullSystem::DumpCullSSBOs`）、世界名标注
> （判据行加 `世界=%s`——两个世界混打一份日志时唯一可靠的区分键）。

1. **第一层（pass 身份推导判据错）**：`46e2da935` 把"非交换链目标 ⇒ shadow pass"
   当兜底推导 ⇒ 独立离屏世界被判 shadow ⇒ collect 走阴影精简链只收 ShadowCaster ⇒
   子世界无 caster ⇒ **收集为空**。已修（`71cc6390c`：判据改为 `req.target != saved_target`，
   ⚠ 必须比**快照**——函数中段 `:638` 已把 `render_target` 切到本 pass 目标，比现役值恒假）。
2. **第二层（guard 早于系统初始化）**：修完①后 trace 显示离屏世界 `guard-entry` 进了、
   却无任何"出口="行 ⇒ 唯一无声出口 = `BeginBatchTagWrite` 在 `!batch_tag_buffer_` 返 null
   ⇒ `if (!tags) return;`。原因：guard 跑在 **RenderBatch** 相位，而 `GPUCullSystem` 的延迟
   初始化在 **RenderFrameSync** 的 `Update()`——新世界首帧 guard 时资源从未建过。
   修复①：guard 入口 `gpu_cull->EnsureInitialized()`（提公开）；修复②：该 null 出口改
   `GLogError` 响亮（A6-d T4b fail-fast 政策）。
3. **第三层（undsettled 定帧护栏锁死单帧世界）**：修完②后离屏出现
   `出口=undsettled`——`IsDispatchSettled()`（dispatch_count_≥2）要求"派发攒满 2 帧"才接管。
   这是 T3g 相位序（dispatch 早于 guard）时代的遗迹；T3i 把 cull 移到 guard 之后，
   同帧已闭环，护栏只剩伤害：**RenderOnce 世界永远等不到第 2 次派发** ⇒ 永久 undsettled。
   已删（连带"槽首写不接管"——其"退 CPU 账"前提在 T4c4 后已不存在；
   `IsSlotLayoutWritten`/`slot_layout_written_` 随之零读者，删除）。
   `IsDispatchSettled` 仍被 `CPUCullReferenceSystem` 用作**采样同步**（非接管门）⇒ 保留。
4. **第四层（脏行上传早于本帧 collect）——真正的"最后一步"**：修完③后离屏首帧终于
   `出口=ok 槽=4 就绪=1 几何批=1 辖批=1 items=1 标签项=1` + `ENGAGED batches=1/1`，
   但 `CULL_SSBO_DUMP` 显示派发后 `DrawCount=0`、`b0=0`，而 `[BatchInfo] Slot4 capacity=1`、
   `[BatchTag] Slot4 有效标签=1` 全部正确。⇒ 标签/布局对、**行不对**：实体行表与 4-ID 表
   的脏行上传挂在 `SyncWorldAddresses()`，而它只在**帧首 `SetFrameIndex`** 调用 ⇒ 传给
   GPU 的是"上一帧 collect 标的脏行"⇒ **首帧世界的行全零**（`geometry_id==0` ⇒ 着色器
   在视锥测试前 return ⇒ 0 幸存）。每帧世界第 2 帧自愈；单帧世界永不自愈。
   修复：`PrepareRenderPassSetup` 在 **RenderCollect 之后**补一次 `SyncWorldAddresses()`
   （WA 行 `memcmp` 未变不提交、脏行只传本帧新标的 ⇒ 近零成本）。

## 4. 当前在途改动（工作树，未提交）

| 文件 | 改动 | 层 |
|---|---|---|
| `src/ecs/core/Context.cpp` | ① `foreign_target` 比 `saved_target` 快照（已在 `71cc6390c`，本树再动）④ collect 后二次 `SyncWorldAddresses()` + 注释写明单帧世界不自愈 | 1/4 |
| `src/ecs/support/PrimitiveBatchPipeline.cpp` | ② guard 入口 `EnsureInitialized()`；`BeginBatchTagWrite` 失败改响亮；③ 删 undsettled/槽首写两块；判据行加世界名 | 2/3 |
| `inc/hgl/ecs/support/GPUCullSystem.h` | `EnsureInitialized` 提公开（含相位时序警示注释）；删 `IsSlotLayoutWritten`+`slot_layout_written_` | 2/3 |
| `src/ecs/support/GPUCullSystem.cpp` | `EndBatchLayout` 删 `slot_layout_written_[slot]=true` 写点 | 3 |

⚠ 编辑工艺坑（本轮各踩一次，写给新会话）：
- `ECS_TAKEOVER_TRACE` 曾驱动绘制热路径**无条件同步回读**（每批每帧 GPU stall，污染帧序判据）——已收进门控；**未门控的观测本身会改变被观测系统**。
- 世界名插值若引用作用域里不存在的变量（如 `context_`）是**静默的格式串/参数错位**，改 trace 行必须编译验证。
  （本轮 PMR flush 行想加世界名时发现其作用域**没有**世界句柄 ⇒ 该改动未落盘，勿在文档/提交信息里声称已加。）
- 日志宏（GLogInfo/GLogError）展开内含 `if` ⇒ 单语句 `if/else` 分支体必须上花括号（C2181）。

## 5. 判据体系（本问题专用）

- **正向硬判据**（每帧/每 pass 一条）：
  `[T3i][TRACE] 世界=<名> frame=… pass=0 出口=ok 槽=… 就绪=1 几何批=1 辖批=1 items=N 标签项=N`
  + `ENGAGED: 世界=<名> batches=1/1` ⇒ 接管真发生（缺失=静默退路，永不接受"像素看起来对"）。
- **GPU 侧实证**（`CULL_SSBO_DUMP=1`，派发后回读）：离屏世界所在槽（RTT=4）
  `Main: [b0=≥1]` 且 `[DrawCount] 全局幸存数 ≥1` ⇒ 行、标签、计数三账齐。
- **画面判据**（示例侧，尚无自检钩子——见 §6 TODO-5）：离屏纹理回读
  `distinct` 应 ≈53（离屏球场景实测）；主场景立方体贴图非纯色。
- **回归判据**（不许被本修复扰动）：
  `PBRSpheres 0x3DD88A0999CDC486`、`ShadowMap 0xA8A369516CB04571`、
  `CascadeShadowMap 0x1CDAADEBF18B7CE4`、`GPUDrivenCull(GC_STATIC=1) 0x90E0745C2C7B8599`、
  `AlphaModes 0x2FE87B15EE8B8A16`、`CoverageVariants 0x8EED38BAD9B8181E`、
  `LoadScene 0xC9E38ED2F49A069B`、`PlaneGrid3D 0x4125C3C830CB92D3`
  逐帧摘要（`<PREFIX>_SELFCHECK=1 FRAMEDIGEST_PERFRAME=9`，**cwd=仓库根**）每图唯一+横向相等；
  ATS 契约（`ATS_SELFCHECK=1`）D1/D3/D4 全 PASS；门 45/0；
  `TestCSMIncrementalPass` rc=0；`AsyncTextureUpload` rc=0 且 05149=0。

## 6. 待办与新会话开工清单

1. ~~跑全量判据~~ **已跑（2026-10-07 最终验收）**：构建 RC=0；回归八台架/ATS/ATU/门/CSM
   **全绿**；RTT 两例 `出口=ok`+`ENGAGED` ✓ 但派发后 `DrawCount=0/b0=0` ✗ ⇒ 主判据未过，
   根因收敛到 §7 P1（实体行 `row[Transform]=0` 到不了 GPU）。
2. **新会话第一件事 = 做 §7 P1 的对照实验（回读 CPU 权威行 vs GPU 份）**，在方向 1/2
   里二选一，一次定案。判据同 §5。
3. **计划档补 §11.17**：本文 §3 的四层浓缩版 + "单帧渲染世界"作为新不变式测试床。
4. **契约固化**（防回潮，零兼容政策的制度化）：
   - `TestCSMIncrementalPass` 机制层加 needle：guard 入口必须 `EnsureInitialized`；
     "undsettled/槽首写"两个旧护栏**禁复活**（照 M 系列格式）；
   - 新增**单帧离屏世界回归床**：RTT 类示例补帧摘要自检（TODO-5），判据=离屏纹理
     非纯色（`RenderOnce` 后第 2 帧读 `GetColorTexture`）——否则这条拓扑再次失明。
5. **示例自检件**：给 `RenderToTexture`/`ColorDepth` 装 `FrameDigest`（共用件
   `example/Shared/FrameDigest.h`，接法照 ATS：门控 + 确定帧 + `*_SELFCHECK` 前缀惯例），
   基线由三次连跑一致后回填。
6. 验收全绿后**分两笔提交**：修复（§4 五文件）一笔、文档/契约（§6.3-4 + 本档收尾）一笔。
   提交信息用中文，显式列文件，勿 `-A`。

## 7. 仍未解释的疑点（判据若不合先看这里）

- **P1 ⇒ 已定性，新会话的起点**（最终验收实证，`CULL_SSBO_DUMP` 扩展段 `[EntityDump]`）：
  RTT 离屏世界首帧派发后的完整图景 =
  ```
  [BatchTag]  Slot 4 Pass 0: 有效标签=1 (b0:1)     ← 层②③修复生效：标签/布局都写上了
  [BatchInfo] Slot 4 Pass 0 Batch 0: capacity=1    ✓
  [CameraInfo] Slot 4 row=4: pos=(-3.99,-3.99,2.05) 六面平面完整（离屏相机）✓
  [EntityDump] Slot 4 Entity 0: flags=0x3, transform_id=0, geom_id=1   ★
  [DrawCount] 全局幸存数: 0 / [b0=0]
  ```
  **着色器在 `transform_id==0 ⇒ return`（视锥测试前）踢掉了唯一实体**——`geom_id=1`
  证明修复④（collect 后二次 `SyncWorldAddresses`）已把**4-ID 表**脏行传到 GPU；但
  **实体行表的 `row[Transform]` 仍是 0**。写者=`TransformSystem::WriteEntityTransformRows`
  （`TickTransform` 相位；`OffscreenWorld::Render` 里 `world_->Tick()` 在 `RenderTo` **之前**
  ⇒ 时序上应已写完）。⇒ 待查方向（按序）：
  1. `row[Transform]` 的写入是否要求实体在 `Mobility` 分类里被变换侧登记（Static 首帧
     只在**行创建时**标脏？RTT 实体创建于 `BuildSphere`（world Initialize 之前/后？）⇒
     标脏发生在 `entity_gpu_table` 尚未 Ensure 时，脏位丢失）；
  2. `UploadEntityGPUDirtyRows` 的 `GetDirtyCount()==0 ⇒ 一行都不传` 短路：实体行在
     首帧之前就"写完并清零脏"，而当时 `entity_gpu_view` 还没建（Initialize 在 Create
     之后？）⇒ 行从未到过 GPU，之后不再有脏；
  3. 对照实验（一次即定案）：在 `[EntityDump]` 同处回读 **CPU 权威** `entity_gpu_table.Row(0)->row[1]`
     ——CPU 非 0 而 GPU 份 0 ⇒ 上传路径断（方向 2）；两边都 0 ⇒ 变换侧根本没写（方向 1）。
  - 附带事实：离屏世界帧槽**恒 4 不轮转**（`出口=… 槽=` 多次渲染不随帧变）——`ShadowMap`
    同样恒 4 但不坏（每帧重传掩盖）；对 RTT 无害（只渲染一次），但**恒槽 = 跨帧在飞
    覆写同一份**的理论与闪烁案 §11.11 同源，若本问题定案后仍要单独立项查离屏 RT 的
    frame index 推进（`rt->GetCurrentFrameIndex()` 对 OffscreenRenderTarget 是否恒 4）。
- **P2**：`undsettled` 行里 `几何批=%u` 曾打印未初始化值（trace 格式串/参数错位在
  修 undsettled 时顺手核对；若新会话再动这些行，警惕 `%s` 与 `.c_str()` 的错位）。
- **P3**：`CPUCullReferenceSystem` 的采样门还在用 `IsDispatchSettled()`——语义是
  "派发至少 2 次后再对拍"（同步时机），与接管无关；若未来重构派发记账，注意这条。

## 8. 一句话交接

**四层叠加的"晚一帧"缺陷族（pass 推导 / 初始化时机 / 定帧护栏 / 脏行上传时机），
每层都被"每帧驱动世界会在第 2 帧自愈"所掩盖，只有 `RenderOnce` 的单帧离屏世界把它们
逐层暴露。①已入库；②③④在工作树，回归全绿、主判据差最后一环：实体行 `row[Transform]`
的脏行为什么没进 GPU 份（`[EntityDump]` 已钉死现场，§7 P1 给了对照实验）。**
新会话从 §7 P1 的三选一核查开始，判据见 §5。
