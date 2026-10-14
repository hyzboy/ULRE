# 可见性权威链（定稿）：三种表示 / 两种极性 / 传播规则

> **性质**：**契约文档**（不是计划）。它把此前只散在三个文件头注里的语义**收在一处**。
> **出处**：`doc/analysis/ecs-audit-2026-10/06-visibility-three-representations.md`（问题详述 + 三个处置选项）；本档 = 该报告 **§五 选项 (c)** 的落地件。
> **关联**：ECS 层主档 `doc/ecs-layer-architecture-and-frame-flow.md`（§5 组件层 / §6 Tick 系统层）；策略表那条审计见 `doc/analysis/ecs-audit-2026-10/07-material-four-key-spaces.md`（③ 的消费面）。
> **复核基线**：`0b4a62479`（2026-10-14）。**符号名是稳定锚点，行号随重构漂移**。
> **范围**：可见性的三种表示、谁是权威、传播规则、边界换算的唯一位置、已知失步与它的闸门、以及后续 (b)/(a) 的路径与判据。

## 0. 一句话

三种表示**都该存在**（服务不同消费面），但**含继承的权威只有一个**：`VisibilityDataStorage` 的**缺席集**（负向）。
`EntityGPU` 行的 `Visible` 位是**扁平派生位**（不继承祖先）⇒ 消费者**不得仅凭它判定「该画」**；
「该画」的最终闸门是 **4-ID 表**（`geometry_id == 0` ⇒ 跳过）与 **批标签**（`0xFFFFFFFF` ⇒ 跳过）。

## 1. 三种表示

| # | 表示 | 位置（符号） | 极性 | 语义 | 消费者 |
|---|---|---|---|---|---|
| ① **权威** | 缺席集 `VisibilityDataStorage::invisible_entities`（+ `ThreadMutex`） | **负向**（在集合里 = 不可见；**默认可见**） | `IsInvisible` = O(1) 直查 **+ 祖先链上溯**；`IsDirectlyInvisible` = 只查自己 | CPU：`ECSContext::IsEntityVisible` → collect（`RenderPrimitiveCollectSystem`）、`LineRenderPipeline`、测试 `TestVisibilityStorage` |
| ② GPU 行位 | `EntityGPUFlags::Visible`（落在 `EntityGPU::flags`，表 = `EntityGPUTable`） | 正向 | **扁平**：只镜像 `SetEntityVisible` 落在这一个实体上的值 | GPU：`entity_gpu_cull.glsl`（`flags & 2u`） |
| ③ 策略事实 | `StrategyFact::EntityVisible` | 正向 | **每帧现算、含继承**（collect 里 `world->IsEntityVisible(...)`） | 策略表 `EvaluateRenderNeed(mask, facts)` |

## 2. 权威链不变量

- **I1 权威唯一**：① 是唯一真值；写者唯一 = `ECSContext::SetEntityVisible`（另有 `DestroyEntity` 的清理路径把该实体移出集合）。② 与 ③ 都是**派生量**。
- **I2 极性**：① 负向、②③ 正向 ⇒ **每次跨边界都要换算**。
- **I3 继承**：① **含**祖先继承；② **不含**（扁平）；③ 含（每帧现算）。
- **I4 边界换算只在权威一处**：`SetEntityVisible(id, visible)` 把**传入值**落两处、**不重算**。两种错法**显式禁止**：
  1. 用 `IsDirectlyInvisible` 冒充 `IsInvisible`（漏继承 ⇒ 后代被误判可见）；
  2. 在 ② 的写入处「顺手传播」（`SetVisible(id, !IsInvisible(id))` 之类）—— 只写自己，正是 §3 的失步。
- **I5 ② 的默认置位 = 权威的「扁平默认」**：`EntityGPUTable::Activate` 无条件置 `Visible`，语义是「该实体**没有被显式设过不可见**」，**不是**「此刻对权威可见」。
- **I6 建立顺序不构成契约**：父先不可见、后代后建立 ⇒ ② 对后代会短时与权威不符（§3 的第二个入口）。

## 3. 已知失步：继承不传播到 GPU 行（现状，未修）

- **症状**：`SetEntityVisible(父, false)` ⇒ ①/③ 都判后代不可见 ✓，而 ② 对后代**仍为 1**。
- **后果**：GPU 剔除把后代当候选 ⇒ 多读 4-ID、多算 AABB（**剔除账 / 计数偏**），但**不会幽灵绘制**。
- **两道闸门**（也是「为什么现在看不出来」的一部分）：`geometry_id == 0` 跳过（4-ID 表陈旧档）+ 批标签 `0xFFFFFFFF` 跳过（不属任何接管批）。
- **两道门都抓不到它**（2026-10-14 实测新增结论）：
  - `TestVisibilityStorage` 只测 CPU 侧（父不可见 ⇒ 子不可见 ✓），**没有任何 GPU 行位断言**；
  - **GPU 剔除的 CPU 参考实现**（`CPUCullReferenceSystem`）自己也是读 ②（`EntityGPUTable::IsVisible(*row)`）⇒ 它与 GPU 剔除**同读扁平位**、天然一致 ⇒ 台架报「0 不一致」**不是**「② 与权威同源」的证据。

## 4. 为什么不删任一（存在理由）

| 表示 | 理由 |
|---|---|
| ① | CPU 权威 + **层级继承** + 线程安全（`ThreadMutex`）+ 默认可见（O(1) 查空集） |
| ② | 着色器的 O(1) 读法：GPU 剔除在 dispatch 里按 `entity_index` 枚举，不能回 CPU 查集合 ⇒ 行是它唯一输入面 |
| ③ | 策略判定的输入形状：`EvaluateRenderNeed(mask, facts)` 要的是「已算完（含继承）」的布尔事实，而不是「要不要上溯祖先」的问题 |

⇒ 本议题不是「可删冗余」，而是「**定权威 + 定传播规则**」。

## 5. 后续路径

### (b) 每帧按权威重算（**推荐**）

在每帧收集 / 上传处，对本帧**参与渲染的实体**用 `IsEntityVisible` 刷 ②（或只清不可见的），使 ② 最多滞后半帧。

- 成本：中（每帧 O(参与渲染实体)；要落在上传计划里 ⇒ 刷位要 `MarkDirty`）；
- 收益：从根上消除「写入点穷举」⇒ 不依赖建立顺序；与 ③ 同源（同一个判据）。
- **判据**：(i) `TestVisibilityStorage` 增用例「父不可见 ⇒ **后代 GPU 行位清 0**」＋「父恢复 ⇒ 后代按**各自**权威态恢复」；
  (ii) 台架（`GC_STATIC=1 GC_SELFCHECK=1 GPUDrivenCull`）与四示例帧摘要**逐位不变**（② 变正确后不应改画面）；
  (iii) 红的样子：让刷位只写自己 ⇒ 新用例必须失败。

### (a) 把继承传播进 ② 的写入点（备选）

- **原报告 §七 的疑虑已查实（2026-10-14）**：**后代枚举能力已具备** —— `TransformDataStorage` 有 CSR 子表
  （`GetChildCount` / `GetChild` / `HasChild` / `CopyChildren`，`TestTransformFlatStorage` 已覆盖）⇒ (a) **不需要**新基础设施。
- 但恢复时必须逐后代**按各自权威态**判定（后代可能自己也被显式设过不可见），比 (b) 更容易写错 ⇒ 仍**推荐 (b) 优先**。
- (a) 的触发条件：只有实测表明「GPU 侧多算的 AABB 判定确实造成问题」（台架数字偏 / 剔除耗时占比异常）时才做。

### 与「问题 3」的关系

本议题的 ② 与 `03-renderitem-descriptor-two-tables` 里「4-ID 表陈旧档」的 (b) 场景是**同一处**
（「停止 collect 时 ② 是否该被清」）⇒ **两条一起拍板**，别分开改。

## 6. 本版（= 审计选项 (c)）的落地与守门

- **头注四处**（把「扁平、不继承」写成显式契约 + 指向本档）：
  `EntityGPU.h`（`Visible` 位定义）、`EntityGPUTable.h`（`SetVisible` 与 `Activate` 的默认置位注释）、
  `VisibilityDataStorage.h`（头注：权威 + 负向 + 含继承）、`Context.h`（`SetEntityVisible`：边界换算唯一处 + 两条错法禁止）。
- **契约针**（挂在 `TestCSMIncrementalPass` 的 A7a 源码契约数组）：
  - **存在性**：`EntityGPUTable.h` 含「扁平派生位」；
  - **禁复活**：`EntityGPUTable.h` 不得出现 `IsInvisible` / `GetVisibilityStorage`
    （派生表**不许**回查权威 —— 防止「在世界里再造一份继承语义」）。
- **红的样子**：把 `IsInvisible` 写回 `EntityGPUTable.h` ⇒ 契约 rc≠0 并点名（落地时实测，文案见落地记录）。
- **局限（如实记录）**：(c) **不修**失步本身，只把它从「隐式依赖兜底」变成「显式契约 + 影响面限定」。

## 7. 实测取证（2026-10-14，逐条现场复核）

| 事实 | 位置（符号） |
|---|---|
| 权威集合 + 互斥锁 + 上溯接口 | `VisibilityDataStorage`（`inc/hgl/ecs/support/VisibilityDataStorage.h`），实现 `src/ecs/support/VisibilityDataStorage.cpp` |
| 上溯走变换树（`TransformAccessor::GetParent()` → `TransformDataStorage::GetOwner()`） | `VisibilityDataStorage::IsInvisible` |
| 唯一写者（写权威 + 写 ② 两处一函数） | `ECSContext::SetEntityVisible`（`inc/hgl/ecs/core/Context.h`） |
| 清理路径（销毁时移出集合） | `ECSContext::DestroyEntity`（`Context.h`，`visibility_storage->SetVisible(id)`） |
| 存储归属 = **世界**（`ECSContext` 初始化列表构造 + `SetContext(this)`） | `src/ecs/core/Context.cpp` |
| ② 的写入口 | `EntityGPUTable::SetVisible` / `Activate` / `IsVisible`（`inc/hgl/ecs/support/EntityGPUTable.h`） |
| ② 的 GPU 读法（Active 位 + Visible 位两道） | `ShaderLibrary/ecs/entity_gpu_cull.glsl`（`flags & 1u` / `flags & 2u`） |
| ③ 现算点 | `RenderPrimitiveCollectSystem::EvaluateCollectStrategy`（`out.facts.entity_visible = world->IsEntityVisible(...)`） |
| 台架同读扁平位（对本失步免疫） | `CPUCullReferenceSystem`（`EntityGPUTable::IsVisible(*row)`） |
| 已删：组件级副本与其系统 | `VisibilityComponent.h`、`VisibilitySystem.cpp` **均不存在**；禁复活针在 `TestCSMIncrementalPass` 的「已删类型名零残留」组 |

## 8. 未证实项（原报告 §七 三条）——2026-10-14 全部查实

1. **后代枚举能力** ⇒ **有**（`TransformDataStorage` 的 CSR 子表：`GetChildCount` / `GetChild` / `HasChild` / `CopyChildren`；`TestTransformFlatStorage` 覆盖）。
2. **台架的可见性口径是否含继承** ⇒ **否**：`CPUCullReferenceSystem` 读 ②（扁平位）⇒ 对这条失步**天然免疫**（见 §3）。
3. **`TestVisibilityStorage` 是否覆盖 GPU 行位** ⇒ **未覆盖**（无 `EntityGPUTable` / `flags` 断言）⇒ 这是 (b) 落地时要补的用例。

## 9. 变更记录

| 日期 | 变更 |
|---|---|
| 2026-10-14 | 首版（= 审计 06 的选项 (c)）：三表示 / 两极性 / 传播规则与已知失步收在一处；头注四处改「扁平派生位」显式契约 + 两条禁复活针；订正同题旧口径（ECS 层主档曾把 `VisibilityComponent` / `VisibilitySystem` 当现行 —— 二者已删、存储归世界）；查实原报告 §七 三个未证实项 |
