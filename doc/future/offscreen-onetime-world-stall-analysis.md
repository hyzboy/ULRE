# 离屏一次性世界（RenderOnce）永不绘制——专项分析与交接

> **⚠ 历史记录档**：本文写于对应日期，文中提到的已删符号（`PrimitiveComponent` / `MaterialComponent` / `RenderableComponent` / `TransformComponent` / `VisibilityComponent`·`VisibilitySystem` / `component_mask` / `RenderItemDataStorage` / `AssignTransformIndices` / `PositionSourceSpec`·`TransformPolicySpec` 等）按**历史口径**读。
> 现行权威：`doc/ecs-layer-architecture-and-frame-flow.md`（ECS 层主档；§2.1 对象模型 / §5 组件面）＋ `doc/visibility-authority-chain.md`（可见性）＋ `doc/backlog.md` §2（逐条落地记录）。

> **用途**：本文是 `RenderToTexture` / `RenderToTextureColorDepth` 离屏内容空白问题的
> **独立分析档案（已结案）**：含症状定性、完整取证时间线、五层叠加根因、
> **验收判据清单**、以及仍未解释的疑点。
>
> **状态（2026-10-07，结案）**：修复①②③④⑤全部落地；全量构建 RC=0；
> **主判据已过**：RTT 两例离屏派发后 `transform_id=1 / DrawCount=1 / COUNT_IN_BUFFER=1`
> + flush engaged；回归全绿（八台架逐字节对基线 / ATS 契约 / ATU 05149=0 / 门 45=0 /
> CSM PASSED，Test 42 机制层契约扩至 16 条含新增 M8/M9，破坏验证过）。
> §7 P1 定案 = **第五层**（变换提交相位晚于剔除派发），方向 1/2 均不成立，见 §7。
> 已入库：`71cc6390c`（层①）、`2f9a8a437`（层②③④）、本批（层⑤ + 契约 + 回归床 + 文档）。
> `b0430f3c1`（AABB 哨兵）= 同期 2D 全黑的**另一个**根因，与本问题无关，勿混淆。
>
> 关联档案：`doc/future/gpu-cull-flicker-postmortem.md`（交换链闪烁三线结案史）、
> `doc/future/entity-rows-gpu-cull-plan.md` §11.17（本问题浓缩版，已追加）。

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

⇒ 只有"**独立世界 × 单帧渲染**"的组合同时踩满 §3 的五层。**每帧驱动的世界会在第 2 帧
自愈**（各层缺陷本质都是"晚一帧"），所以从未暴露——这是本缺陷族潜伏至今的原因。

## 3. 取证时间线（五层根因的发现顺序）

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
4. **第四层（脏行上传早于本帧 collect）**：修完③后离屏首帧终于
   `出口=ok 槽=4 就绪=1 几何批=1 辖批=1 items=1 标签项=1` + `ENGAGED batches=1/1`，
   但 `CULL_SSBO_DUMP` 显示派发后 `DrawCount=0`、`b0=0`，而 `[BatchInfo] Slot4 capacity=1`、
   `[BatchTag] Slot4 有效标签=1` 全部正确。⇒ 标签/布局对、**行不对**：实体行表与 4-ID 表
   的脏行上传挂在 `SyncWorldAddresses()`，而它只在**帧首 `SetFrameIndex`** 调用 ⇒ 传给
   GPU 的是"上一帧 collect 标的脏行"⇒ **首帧世界的行全零**（`geometry_id==0` ⇒ 着色器
   在视锥测试前 return ⇒ 0 幸存）。每帧世界第 2 帧自愈；单帧世界永不自愈。
   修复：`PrepareRenderPassSetup` 在 **RenderCollect 之后**补一次 `SyncWorldAddresses()`
   （WA 行 `memcmp` 未变不提交、脏行只传本帧新标的 ⇒ 近零成本）。
5. **第五层（变换提交相位晚于剔除派发）——真正的"最后一环"**：修完④后 4-ID 表已到
   GPU（`geom_id=1`），但 `[EntityDump]` 仍 `transform_id=0`。关键取证捷径：该段读的是
   **CPU 权威表**（`GPUCullSystem.cpp` `GetEntityGPUTable().Row(e)`），不是 GPU 回读 ⇒
   派发时刻 **CPU 侧就没写过**，上传路径无罪（原 §7 P1 的方向 1/2 均不成立）。
   真因：`row[Transform]`（连同 **L2W 矩阵本体**）的唯一落盘点
   `TransformSystem::SubmitTransformUpdates`（→ `WriteEntityTransformRows`）只挂在
   `RecordPreparedRenderPhaseRange`——即 `BeginRenderPass` **之后**的绘制录制段 ⇒
   恒晚于同帧 `PrepareRenderPassSetup` 末尾（RenderFrameSync）的剔除派发。
   主世界帧 N 派发消费帧 N-1 的写入 ⇒ 第 2 帧自愈（实测 frame=0 恒 `DrawCount=0`、
   frame≥1 正常——A6-d 删 CPU 账后**每个应用首帧都静默空白一帧**，一直无人报故）；
   单帧世界永不。修复：`PrepareRenderPassSetup` 在 `SetFrameIndex` 之后、collect 之前
   补一次 `SubmitTransformUpdates`（动态段行号含帧槽 ⇒ 必须在 SetFrameIndex 后；
   同帧幂等——值变才标脏 + 版本记账 ⇒ 绘制录制段的第二次调用退化 no-op，主世界零行为变化）。

## 4. 落盘改动（五层修复，分三批提交）

| 文件 | 改动 | 层 |
|---|---|---|
| `src/ecs/core/Context.cpp` | ① `foreign_target` 比 `saved_target` 快照（`71cc6390c`）④ collect 后二次 `SyncWorldAddresses()` ⑤ collect 前一次 `SubmitTransformUpdates()`（M8-needle 锚） | 1/4/5 |
| `src/ecs/support/PrimitiveBatchPipeline.cpp` | ② guard 入口 `EnsureInitialized()`；`BeginBatchTagWrite` 失败改响亮；③ 删 undsettled/槽首写两块；判据行加世界名 | 2/3 |
| `inc/hgl/ecs/support/GPUCullSystem.h` | `EnsureInitialized` 提公开（含相位时序警示注释）；删 `IsSlotLayoutWritten`+`slot_layout_written_` | 2/3 |
| `src/ecs/support/GPUCullSystem.cpp` | `EndBatchLayout` 删 `slot_layout_written_[slot]=true` 写点 | 3 |
| `src/ecs/support/TestCSMIncrementalPass.cpp` | Test 42 机制层加 M8（变换提交先于派发）+ M9（guard EnsureInitialized / 禁两旧护栏复活）；16 条契约 | 契约 |
| `example/Offscreen/RenderToTexture.cpp`、`RenderToTextureColorDepth.cpp` | 帧摘要自检床（`RTT_SELFCHECK`/`RTTCD_SELFCHECK`，冻结自旋 + 第 8 帧 + distinct≥50） | 回归床 |
| `doc/future/entity-rows-gpu-cull-plan.md`、本文 | §11.17 五层浓缩 + 结案收尾 | 文档 |

提交切分：`71cc6390c`（层①）、`2f9a8a437`（层②③④ + 本文初版）、本批（层⑤ + 契约 + 回归床 + 文档收尾）。

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
- **画面判据（已装自检钩子，2026-10-07）**：`RTT_SELFCHECK=1` → 第 8 帧摘要
  `0xBA9CA214778AF3A4`（distinct=256，下限守卫 ≥50）；`RTTCD_SELFCHECK=1` →
  `0xD71ABABC2BE460C0`（同守卫）。PASS 退 0 / FAIL 退 1 / 读回失败退 2；
  基线由三次连跑一致后回填。离屏若再"永不绘制"，主场景贴图退化为纯清屏色 ⇒ 摘要立变。
- **回归判据**（不许被本修复扰动）：
  `PBRSpheres 0x3DD88A0999CDC486`、`ShadowMap 0xA8A369516CB04571`、〔2026-10-11：ShadowMap 台架已删除（旧 shadow-map 直采示例，CSM 路线取代；如需退化为单张阴影图，把 CascadeShadowMap.cpp 的 `cfg.cascade_count` 设 1 即可）——本行基线仅适用于删除前〕
  `CascadeShadowMap 0x1CDAADEBF18B7CE4`、`GPUDrivenCull(GC_STATIC=1) 0x90E0745C2C7B8599`、
  `AlphaModes 0x2FE87B15EE8B8A16`、`CoverageVariants 0x8EED38BAD9B8181E`、
  `LoadScene 0xC9E38ED2F49A069B`、`PlaneGrid3D 0x4125C3C830CB92D3`
  逐帧摘要（`<PREFIX>_SELFCHECK=1 FRAMEDIGEST_PERFRAME=9`，**cwd=仓库根**）每图唯一+横向相等；
  ATS 契约（`ATS_SELFCHECK=1`）D1/D3/D4 全 PASS；门 45/0；
  `TestCSMIncrementalPass` rc=0；`AsyncTextureUpload` rc=0 且 05149=0。

## 6. 待办与验收（全部完成，2026-10-07）

1. ~~跑全量判据~~ **已完成**：构建 RC=0；回归八台架/ATS/ATU/门/CSM **全绿**；
   RTT 两例 `出口=ok`+`ENGAGED`+派发后 `transform_id=1 / DrawCount=1 / COUNT_IN_BUFFER=1`
   ⇒ **主判据已过**。
2. ~~§7 P1 对照实验~~ **已定案**：`[EntityDump]` 本就读 CPU 权威表 ⇒ `transform_id=0`
   证明派发时刻 CPU 侧未写 ⇒ **不是上传断链（方向 2 排除）、不是 Mobility 登记（方向 1 排除）**，
   而是**第五层：变换提交相位晚于剔除派发**（见 §3.5 / §7 P1）。
3. ~~计划档补 §11.17~~ **已追加**（五层浓缩表 + 取证捷径 + "单帧渲染世界"回归床）。
4. ~~契约固化~~ **已落**：Test 42 机制层新增 **M8**（`SubmitTransformUpdates` 必须在
   collect/派发之前，needle 带注释锚）+ **M9**（guard 入口 `EnsureInitialized`；
   禁 `gpu_cull->IsDispatchSettled()` / `IsSlotLayoutWritten` 在接管路径复活）；
   16 条契约，破坏验证 rc=42 点名、还原复绿。
5. ~~示例自检件~~ **已装**：`RTT_SELFCHECK=1`→`0xBA9CA214778AF3A4`、
   `RTTCD_SELFCHECK=1`→`0xD71ABABC2BE460C0`（三次连跑一致后回填，见 §5）。
6. **分两笔提交**：修复（层⑤ Context.cpp）一笔、文档/契约/回归床一笔。
   提交信息用中文，显式列文件，勿 `-A`。

## 7. 疑点与结案

- **P1 ⇒ 已定案（2026-10-07，第五层）**。`CULL_SSBO_DUMP` 扩展段 `[EntityDump]` 显示
  离屏世界首帧派发后 `transform_id=0, geom_id=1, DrawCount=0`。**关键取证捷径**：该段读的是
  **CPU 权威表**（`GPUCullSystem.cpp` 的 `context_->GetEntityGPUTable().Row(e)`），不是 GPU
  回读 ⇒ `transform_id=0` 直接证明**派发时刻 CPU 侧就没写过** `row[Transform]`。
  因此原三选一里的方向 1（Mobility 登记/脏位丢）与方向 2（`GetDirtyCount()==0` 上传短路）
  **都不成立**——上传路径无罪，是**写者相位晚于消费者相位**：
  - `row[Transform]`（连同 **L2W 矩阵本体**）的唯一落盘点 = `TransformSystem::SubmitTransformUpdates`
    （→ `WriteEntityTransformRows`）；
  - 它此前**只**挂在 `RecordPreparedRenderPhaseRange`（`BeginRenderPass` **之后**的绘制录制段），
    而 GPU 剔除派发在同帧更早的 `PrepareRenderPassSetup` 末尾（RenderFrameSync）就读了实体行；
  - 主世界帧 N 派发消费帧 N-1 的写入 ⇒ 第 2 帧自愈（实测 frame=0 恒 `DrawCount=0`、frame≥1
    正常 ⇒ A6-d 删 CPU 账后**每个应用首帧都静默空白一帧**）；单帧世界永不。
  - **修复**：`PrepareRenderPassSetup` 在 `SetFrameIndex` 之后、collect 之前补一次
    `SubmitTransformUpdates`（同帧幂等 ⇒ 绘制段第二次调用退化 no-op）。实测离屏首帧即
    `transform_id=1`（静态 = 1+slot）、`DrawCount=1`、`COUNT_IN_BUFFER=1`。
  - 附带事实（**同日核查已定案**）：离屏世界帧槽恒 4 = 设计行为（`slot_count` 默认 1 ⇒
    `slot_index=(x+1)%1` 恒 0 ⇒ `GetCurrentFrameIndex()=data_slot_base+0`；首带基址=4）。
    "恒槽跨帧覆写"不成立——深度 1 带自保护（BeginRender 复用前 WaitLastSubmitFence）+
    RenderTo 下沿 rt->WaitFence() 双重串行化。**真缺陷是槽带只借不还**：Destroy/Release 不
    归还 `next_offscreen_slot_base`（RenderTargetManager.cpp:255 只增），CSM 一条级联占一带
    （EnvironmentSystem.cpp:190-197）⇒ 一次 Enable 即耗尽 4 带 ⇒ **这就是 C3 遗留缺陷①
    「二次 EnableMainLightShadow ⇒ 越界(起点=8)」的根因**；OffscreenWorld+CSM 同应用必
    fail-fast。**已修（2026-10-07，走 A，`d9b2c6886`）**：OffscreenSlotBands 位图 first-fit +
    Destroy 归还（Test 43 + 端到端探针二次 Enable first=1 second=1 验证），详见计划档 §11.17。
- **P2 ⇒ 已随层③清除**：`undsettled` trace 行（含曾打印未初始化 `几何批=%u` 的那条）在删
  护栏时整块移除；改 trace 行仍须编译验证格式串/参数对齐。
- **P3 ⇒ 仍在**：`CPUCullReferenceSystem` 的采样门用 `IsDispatchSettled()`——语义是
  "派发至少 2 次后再对拍"（同步时机），与接管无关；M9 的禁复活 needle 取**调用形态**
  `gpu_cull->IsDispatchSettled()` 只挡接管路径，不伤这条。若未来重构派发记账，注意这条。

## 8. 一句话交接（结案）

**五层叠加的"晚一帧"缺陷族（pass 推导 / 初始化时机 / 定帧护栏 / 脏行上传时机 / 变换提交时机），
每层都被"每帧驱动世界会在第 2 帧自愈"所掩盖，只有 `RenderOnce` 的单帧离屏世界把它们逐层暴露。
①②③④已入库（`71cc6390c`/`2f9a8a437`）；⑤（真·最后一环：`SubmitTransformUpdates` 相位晚于
剔除派发）本批落地。主判据全过（离屏 `DrawCount=1`）、回归全绿、契约 M8/M9 固化、RTT 两例
装帧摘要回归床。"单帧渲染世界"自此成为引擎的一条常驻不变式测试床。**
