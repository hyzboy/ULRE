# §2.2 问题 6 详述：可见性三种表示、两种极性 —— 权威是缺席集，GPU 位是**扁平**的

> **审计出处**：`doc/ecs-quality-audit-2026-10.md` §2.2「并行表家族」第 6 条（原文："可见性三种表示、两种极性：`VisibilityDataStorage` 的**缺席集**（自注"唯一真值"）vs `EntityGPUFlags::Visible` 的**正位** vs 每帧重算的 `StrategyFact::EntityVisible`"）
> **结论一句话**：三种表示**都该存在**（服务不同消费面），但**继承语义只落在权威上**：`SetEntityVisible` 把该实体自身镜像进 GPU 行、**不传播到后代**，而权威谓词 `IsInvisible` **含祖先继承** ⇒ 父不可见时，CPU 侧判定为不可见、**GPU 行仍然说可见**。这是"可证"的一处失步，且没有任何门。

---

## 一、三种表示各是什么

| 表示 | 位置 | 极性 | 语义 |
|---|---|---|---|
| **① 缺席集**（权威） | `VisibilityDataStorage`（`inc/hgl/ecs/support/VisibilityDataStorage.h`）：`ankerl::unordered_dense::set<EntityID> invisible_entities` + `ThreadMutex` | **负向**（集合里的 = 不可见；默认可见） | 文件头自述"**唯一真值**：由世界通过 `ECSContext::SetEntityVisible` 写入（A5a 起组件级可见性副本已删除）"；`IsInvisible` = **直查集合 + 祖先链上溯**（`:26-38`），`IsDirectlyInvisible` = 只查自己 |
| **② GPU 行正位** | `EntityGPUFlags::Visible = 1u << 1`（`inc/hgl/ecs/core/EntityGPU.h:30-35`），落在 `EntityGPU::flags` | **正向**（置位 = 可见） | 实体建立时置位（`EntityGPUTable.h:97-115`，注释写着"Visible 默认置位是**与真值同源**的要求……之后只有 `SetEntityVisible` 会改它，两条路径闭环"）；写入口 `EntityGPUTable::SetVisible`（`:132-141`） |
| **③ 策略事实** | `StrategyFact::EntityVisible`（`RenderStrategyTable.h:80-81`，注释"读法 = `ECSContext::IsEntityVisible`，含祖先继承，已算完"） | **正向** | 每帧由 collect 现算：`out.facts.entity_visible = world->IsEntityVisible(...)`（`RenderPrimitiveCollectSystem.cpp:794`），喂 `EvaluateRenderNeed` |

### 消费者

- ① 的读者（CPU）：`ECSContext::IsEntityVisible`（`Context.h:774-777`）→ collect（`RenderPrimitiveCollectSystem.cpp:794,1929,2046`）、`LineRenderPipeline.cpp:443`、测试 `TestVisibilityStorage`；
- ② 的读者（GPU）：`entity_gpu_cull.glsl:226-232` —— `flags & 1`（Active）与 **`flags & 2`（Visible）**，两个位都不过就直接 `return`（不参与剔除）；
- ③ 的读者：策略表 `EvaluateRenderNeed`（`RenderStrategyTable.h:114` 把 `entity_visible` 折进 `fact_mask`）。

---

## 二、已证的一处失步：继承不传播到 GPU 行

唯一写者 `ECSContext::SetEntityVisible`（`inc/hgl/ecs/core/Context.h:760-774`）：

```cpp
if (visible) visibility_storage->SetVisible(id);
else         visibility_storage->SetInvisible(id);

// T3b：镜像到实体行（唯一真值仍是 VisibilityDataStorage；本表是派生视图）
entity_gpu_table.SetVisible(id, visible);      // ← 只写**该实体自己**
```

而权威谓词是**含继承**的（`Context.h:774-777`：`IsEntityVisible(id) = !visibility_storage->IsInvisible(id)`，内部"O(1) 直查不可见集合 + **祖先链上溯**"）。

⇒ **父不可见时**：
- CPU 侧：`IsEntityVisible(父) = false`、`IsEntityVisible(后代) = false` ✓（策略事实正确，收集/剔除的 CPU 口径正确）；
- GPU 行：`后代.flags & Visible` **仍为 1** ⇒ GPU 剔除把后代当候选实体，照常读 4-ID / 算 AABB（多算；**不画**由 batch tag 兜住 —— 与问题 3 同族）。

同型的第二个入口：**实体在"父已不可见"之后才建立**时，建立路径无条件置 `Visible` 位（`EntityGPUTable.h:97-115`）⇒ 与权威谓词（继承）同样不一致。

### 为什么现在看不出来

GPU 侧对"陈旧可见"有两道兜底：`geometry_id == 0` 跳过（`entity_gpu_cull.glsl:247-249`，对应问题 3 的实体表陈旧档）与 batch tag `0xFFFFFFFF` 跳过（`:333-334,402`）。所以后果落在**剔除账 / AABB 判据**（以及与台架 CPU 参考对拍时可能出现的差异），而不是幽灵绘制。

---

## 三、极性：不是"两种极性谁对"，而是"边界必须只在一处换算"

- ① 负向、②③ 正向 ⇒ 每次跨边界都要换算。换算写反一次就**静默反相**，而且有两种典型错法：
  - 用 `!IsDirectlyInvisible(id)` 冒充 `!IsInvisible(id)`（漏掉祖先继承 ⇒ 后代被误判可见）；
  - 在 ② 的写入处"顺手"用 `SetVisible(id, !IsInvisible(id))` 试图传播，但只写自己 ⇒ 正是第二节那处失步。
- 当前代码里这个换算是**安全的**（`SetEntityVisible` 直接用传入的 `visible`，不重算），但**头注没有把"①是负向、②是扁平正位、③每帧现算带继承"写在一处** ⇒ 下一个改这块的人要在三个文件里拼出这个语义。

---

## 四、设计意图（为什么三种都该在）

| 表示 | 存在理由 |
|---|---|
| ① | CPU 权威 + **层级继承** + 线程安全（`ThreadMutex`）+ 默认可见（O(1) 查空集） |
| ② | 着色器的 O(1) 读法：GPU 剔除在 dispatch 里按 `entity_index` 枚举，不能回 CPU 查集合；`EntityGPU` 行是它唯一输入面 |
| ③ | 策略判定的输入形状：`EvaluateRenderNeed(mask, facts)` 需要"已算完（含继承）"的布尔事实，而不是"要不要上溯祖先"的问题 |

⇒ 这条**不是"可删冗余"**，而是"**必须定权威 + 定传播规则**"。

---

## 五、处置选项

### 选项 (a)：把继承传播进 GPU 行（改 `SetEntityVisible`）

- 形态：`SetEntityVisible(id, visible)` 在写权威之外，按**后代链**把 `entity_gpu_table.SetVisible(后代, visible)` 一并刷（父不可见 ⇒ 后代全清；父恢复 ⇒ 后代按各自权威态恢复 —— 注意"各自"：后代可能自己也被显式设过不可见，所以恢复不能一律置 1）。
- 需要的能力：**列出后代**。当前 `VisibilityDataStorage` 只有"上溯"（`IsInvisible` 的祖先链），没有后代枚举 ⇒ 要借变换树的父子链（`TransformDataStorage` 的 `parent_indices`）或维护反向索引。
- 成本：中（新代码 + 语义细节：恢复时必须以权威为准，而不是简单取反）。
- 收益：GPU 行与权威**在继承维度也同源**，剔除候选集与 CPU 口径一致。
- 风险：中（传触碰可见性语义；需要 `TestVisibilityStorage` 增加"父不可见 ⇒ 后代 GPU 位清 0"的用例 + 平台对拍）。

### 选项 (b)：GPU 位改为"每帧按权威重算"（不动写者）

- 形态：在每帧收集/上传处，对**本帧参与渲染的实体**用 `IsEntityVisible` 刷 `flags` 位（或只清不可见的），使 ② 永远滞后最多半帧。
- 成本：中（每帧 O(参与渲染实体)，且要落在上传计划里：刷位要 `MarkDirty`）；语义清楚（② 彻底降级为"每帧派生量"）。
- 收益：从根上消除"继承不同步"（不再依赖写入点穷举）；与 ③ 同源（同一个判据）。
- 风险：中低（多一次每帧遍历；但这类遍历 collect 已有）。

### 选项 (c)：把"扁平、不继承"写进契约（零代码，最小步）

- 形态：在 `EntityGPU::Visible` 与 `EntityGPUTable::SetVisible` 的头注里明写：**该位是扁平派生位（不继承祖先），消费者不得仅凭它判定"该画"**，并以 batch tag / 4-ID 表为最终闸门。
- 成本：~10 行注释。
- 收益：把"隐式依赖兜底"变成**显式契约**；后续审计/新人不至于把它当 bug 乱改。
- 局限：不修失步本身，只是承认它并限定其影响面。

---

## 六、建议

1. **(c) 立刻做**（零风险；它同时是 (a)/(b) 的前置说明）。
2. **(b) 优先于 (a)**：每帧按权威重算比"在写入点穷举传播"更难写错（传播要在恢复时逐后代判断自身状态，容易出新 bug）；且它与 ③ 天然同源。
3. **(a) 只在有实测证据表明"GPU 侧多算的 AABB 判定确实造成问题"时再做**（例如台架对拍数字偏、或剔除耗时占比异常）。
4. 顺带：这条与问题 3 的 (b) 场景是**同一处**（"停止 collect 时 Visible 位是否被清"）——两条一起拍板，别分开改。

---

## 七、未证实 / 需要确认

- 是否存在**后代枚举**能力（我只确认了祖先上溯；`TransformDataStorage` 的父子链是否可直接遍历子树未查）；
- 台架（`CPUCullReferenceSystem`）的可见性口径是否也走 `IsEntityVisible`（含继承）——若是，则 (a)/(b) 修好后它的对拍数字可能**变化**（变正确），需要按"预期变化"而非"回归"判读；
- `TestVisibilityStorage` 现在的用例覆盖了"父不可见 ⇒ 子不可见"（CPU 侧 ✓ `:115-143`），但**没有**覆盖"GPU 行位是否同步"（我按当前树未见此断言）。

## 八、落地记录（2026-10-14）：选项 (c) —— 权威链契约

**选型**：(c)（零代码契约）先做，它是 (a)/(b) 的前置说明；§六 的推荐顺序照此执行。

**产出**：

1. **新增契约档** `doc/visibility-authority-chain.md` —— 三种表示 / 两种极性 / 传播规则（I1–I6）/ 已知失步与两道闸门 /
   后续 (b)(a) 路径与判据 / 实测取证表 / 变更记录。**语义只此一处**（原先散在三个文件头注里，没人能拼全）。
2. **头注四处**改成「扁平派生位」显式契约并指向该档：
   `inc/hgl/ecs/core/EntityGPU.h`（`Visible` 位）、`inc/hgl/ecs/support/EntityGPUTable.h`（`SetVisible` + `Activate` 默认置位）、
   `inc/hgl/ecs/support/VisibilityDataStorage.h`（权威 + 负向 + 含继承）、`inc/hgl/ecs/core/Context.h`（`SetEntityVisible` = 边界换算唯一处 + 两条错法禁止）。
3. **契约针三条**（`TestCSMIncrementalPass` 的 A7a 组 ④）：存在性「扁平派生位」；
   禁复活 `IsInvisible` / `GetVisibilityStorage`（派生行表不许回查权威）。
4. **同题过期口径回扫**：ECS 层主档把 `VisibilityComponent` / `VisibilitySystem` 当现行的四处 + banner ⑧；
   `simple-sphere` 的 tick 序；以及 `ShadowProxy.h` / `EntityGPU.h` / `EntityGPUTable.h` / `CPUCullReferenceSystem.cpp` /
   `PrimitiveBatchPipeline.cpp` 里「CPU FinalizeBatch / D3 段」的旧提法（那段已随批 D 删除）。

**红的样子（实测原文）**：
- ① 删掉 `EntityGPUTable.h` 的「扁平派生位」契约注释 ⇒ `TestCSMIncrementalPass` **rc=25**：
  `Test 25 Failed: 实体行位的「扁平派生位」契约注释丢失（下一个人会把 ② 当含继承的权威） -- '扁平派生位' not found in EntityGPUTable.h`；
- ② 把 `IsInvisible` 写回 `EntityGPUTable.h` ⇒ **rc=25**：
  `Test 25 Failed: 派生行表回查权威（在世界里再造一份继承语义 ⇒ 与权威双真值） -- 'IsInvisible' 重新出现在 EntityGPUTable.h（禁复活）`。
两次改动均**逐字节还原**（sha256 复核）。针在运行期读源码 ⇒ 两条红都**不需要重编**。
**验收**：两配置 rc=0 / 0 error；门 45/0；`ctest -L ecs` 11/11；`-L shadergen` 13/13；四示例帧摘要逐位不变；
接管台架 `GC` / `DWS` PASS（可见性正是这两例覆盖的路径）。

**未做（本版边界）**：(c) **不修**失步本身。(b)（每帧按权威重算）留 §五，与「问题 3 的 ②」同批拍板后再开。

### 8.1 观察面（③）与「问题 3 的 ②」捆绑裁定（2026-10-14 同批）

- **③ 观察面落地**：`EntityGPUTable::VerifyVisibleMirror`（门 = 台帐 `ULRE_ECS_DEBUG_API`；接在 collect 的 4-ID/阴影事实发布点；
  **有界点名**（每实体行一次）+ `GetVisibleMirrorCount()` 取数；**不做 fail-fast 门** —— 它报的是**预期偏差**（本失步按构造不错画）。
  它是 (b) 的量化与判据前置：走 (b) 后该计数必须归零。契约针三条（本体 / 取数口 / 接线）。
- **「问题 3 的 ②」裁定 = 不清位**：着色器闸门顺序（`flags&1` :225 → `flags&2` :229 → 4-ID :244 → `geometry_id == 0` return :248）
  表明昂贵部分在 4-ID 门之后，而 4-ID 表已在时点 2 显式清零（报告 03 §10.6）⇒ 残留 `② = 1` 的代价只有一次表读；
  且 ② 的权威定义是"该实体自身的可见开关"，拿它表达"在批里"会叠加第二种语义。详见契约档 §5。
