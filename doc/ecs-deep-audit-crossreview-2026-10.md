# ECS 深度审计（2026-10）交叉评审 · 综合版 v2 —— 三方取证合流

> **版本说明**：v1（本档前一版）= 我对第一轮审计档的逐条复核；**v2 = 合流第二轮评审档**（`doc/ecs-deep-audit-second-crossreview-2026-10-11.md`，带独立探针实测）。我 v1 被推翻的结论逐条列入 **§6 变更记录**，正文按新结论重写。
> **三方代号**：**A** = `doc/ecs-deep-quality-audit-and-evidence-2026-10.md`（原始审计档）· **B** = 本评审（v1→v2）· **C** = `doc/ecs-deep-audit-second-crossreview-2026-10-11.md`（第二轮）。
> **取证基线**：`e8ee755fd`；**本档所有行号属该提交**，跨提交引用写成「文件:符号」。
> **性质**：只读评审 + 文档订正清单。不改引擎代码。
> **方法**：`git grep` / 读码逐条取证 + 可执行件实跑；**C 的探针数字我未独立复现**（只做代码级复核并注明来源），C 的结论同样不豁免取证。
> **执行入口**：按分歧度重排、可开工的清单在本档的派生档 `doc/ecs-audit-consensus-and-task-plan-2026-10-11.md`（无争议项直接在 §1 开工；分歧项挂 §2/§3 待拍板）。

---

## 0. 结论速览

### 0.1 一句话

**三方合流后，优先级最高的不是 A/B 争的那些"代码存在性"问题，而是 C 发现的 4 项正确性缺陷——其中 CSR 子表搬迁丢内容、`MigrateTransform` 非事务这两条会当场损坏现场数据（我已逐条代码级复核成立）；A 与 B 都把注意力放在"零调用/命名/冗余"上，方向错了半格。**

### 0.2 判定分布

| 对象 | 我方复核结论 |
|---|---|
| C 的 4 项 P1 | **4/4 成立**（3 项逐字复核；P1-4 的"未目视 GPU 残留"我同意留待示例目视） |
| C 的 4 项 P2 | **4/4 成立**，另我补 2 条（`TransformDataStorage::Clear` 全仓零调用者；`LinesComponent::version` 只写不读） |
| C 对 A/B 的 15 条裁决 | **采纳 12 / 部分采纳 2 / 1 条系对我档措辞的误读**（判据见 §3） |
| A 的 30 条断言 | 21 成立、9 需订正（同 v1）；**新增 1 条判错**：A §3.1「`RenderItemHandle 0` = 空句柄」错（无效值是 `UINT32_MAX`） |
| B（我）v1 | **10 处订正**（2 处事实错、4 处定级/措辞、2 处方案作废、2 处引用错）——见 §6 |

### 0.3 本次实测清单（可复查）

| 命令 | 结果 | 用途 |
|---|---|---|
| `ctest --test-dir build -C Release -N -L ecs` | **9**（`TestCSMIncrementalPass` 单列，LABELS `csm;pass`） | 订正 A §8.2 的"10 项" |
| `./build/out/Windows_64_Release/TestTransformFlatStorage.exe` | **rc=0**；Test 9 = 每行 **158 B** / 17 条平行数组 | A 的基线行成立 |
| `cl /Bv`（MSVC 14.51.36231 实跑） | **开关存在**：打印 Compiler Passes 版本表（cl/c1/c1xx/c2/link），随后 `D8003: missing source filename` | **推翻我 v1**「MSVC 无此开关」；C 正确 |
| `grep -c RenderSystemCore.cpp build/src/ecs/ULRE.ECS.vcxproj` | **1**（单条 `<ClCompile>`） | C 正确：源列表登记冗余、**无重复编译** |
| `grep -rn "TransformStorage()->Clear()"` | **0 命中** | 我新发现：`TransformDataStorage::Clear()` 全仓零调用者 |

---

## 1. C 的 4 项 P1：我的独立复核

### 1.1 `AddChild` 搬迁不复制旧区间（**成立** · `TransformDataStorage.h:571-589`）

```cpp
if (count == 0 || (offset + count) != child_pool.size())
    child_offset[id] = child_pool.size();   // ← 重指到池尾，旧 count 项没搬
child_pool.push_back(child);                // ← 只追加这 1 项
child_count[id] = count + 1;                // ← 却声明 count+1
```

- **机制成立**：本行 span 不贴池尾时被重指，旧子节点留在原（已成垃圾的）区间，`child_count` 仍报 `count+1`。
- **连带 UB**：`CopyChildren`（`:557-569`）用 `out.assign(begin+offset, begin+offset+count)`，搬迁后 `offset + count` 可**超出 `child_pool.size()`** ⇒ 未定义行为（`HasChild` `:548-554` 同区间读）。
- **触发面**：任何"父 X 先有子、另一父 Y 也有子、X 再加子"的装配顺序（正是 C 的最小触发 P→A、Q→B、P→C）。
- **我补**：`RemoveChild`（`:591-620`）在行内前移保持连续 ⇒ **"删除后再追加"同样落入丢内容分支**（count>0 且 span 不再贴池尾），回归用例应同时覆盖。
- **测试为何绿**：`TestTransformFlatStorage.cpp:702-704` 对**同一父**连续 `AddChild` 三次，`offset+count == pool.size()` 恒成立 ⇒ 只走续写分支。
- **放大面**：`TransformAccessor::MarkLocalChanged`（`TransformAccessor.cpp:73-100`）用它遍历后代标脏/版本传播 ⇒ 坏区间让后代上传漏项或读越界。

### 1.2 访问器 `SetParent` 成环 ⇒ 后代遍历不终止（**成立** · `TransformAccessor.cpp:218-239` + `:73-100`）

- `SetParent` 只把 `parent == id`（自我）归为无效（`:225`），**不拒绝祖先环**，随后立即 `MarkLocalChanged`。
- `MarkLocalChanged` 用显式栈展开 `CopyChildren`，**无 visited、无深度上限**（`:82-99`）⇒ A↔B 环下每轮继续 push，永不终止。C 的探针 >3s 未返回；我未复现，代码级结论一致（无终止条件）。
- **同意 C 的关键论点**：不能拿 `RebuildTopologyOrder()` 的环保护当安全证明——程序卡在拓扑重建**之前**。
- **我补**：这是**公共 API**（作者/示例侧可用）。修法二选一并用更稳：改父前做祖先链上溯检测（O(depth)、`SetParent` 内 fail-fast），或标脏遍历加 per-row epoch 标记（零分配、可终止）。

### 1.3 段预算拒绝非原子：`MigrateTransform` / `CreateTransform`（**成立**）

**(a) 迁移失败写错段**（`Context.cpp:862-906`）：

```cpp
ReleaseTransformSlot(!toMovable, old_slot);              // :872 先还旧段槽
const uint32_t slot = AcquireTransformSlot(toMovable);   // :874
if (slot == INVALID_TRANSFORM_SLOT) {
    ...
    (toMovable ? movable_transforms : static_transforms)[old_slot] = id;   // :887 ← 目标段 + 旧下标
    return;
}
```

三元两支都是**目标段**（`toMovable ? movable : static`），而下标用**旧段槽号** ⇒ ① 目标段满时把 `id` 覆盖到目标段 `[old_slot]`（挤掉别人的槽，正是 C 的 `a_movable=1` 且 `d_dynamic=0`）；② `old_slot >= 目标段长度` 时越界写（段数组定长、`operator[]` 无界检查）。旧段槽也没恢复（已进 free list）。

**(b) 访问器先改真源后迁移**（`TransformAccessor.cpp:500-512` → `:513-522`）：`storage->SetMobility(...)` 在 `MigrateTransform` **之前**执行 ⇒ 迁移失败时 storage 的 mobility、段账目、id 名单三方不一致。

**(c) 首建半成品**（`Context.cpp:922-941`）：`CreateTransform` 调 `RegisterTransform` 却**无法感知结果**（该函数返回 `void`，`:832-860` 段满只报一次错就 `return`）⇒ 返回**有效 id 但无 L2W 槽**（C 实测 `overflow_valid=1 / overflow_has_slot=0`，与代码一致）。`:931` 注释「不再造半个变换」只覆盖**总预算**拒绝，不覆盖**段满** ⇒ 注释只对了一半。

### 1.4 静态 `static_scene_revision` 信号链断裂（**成立** · 影响待目视）

- `TransformSystem::Update` 开头**无条件** `storage->UpdateDirtyWorldMatricesFlat()`（`TransformSystem.cpp:75-79`）；该函数把 `matrixDirty[eval_order[i]] = 0`（`TransformDataStorage.h:858-861`）清掉。
- 随后 `SubmitTransformUpdates`（`:195-199`）靠 `ShouldUpdateTransform`（`:531-551`）判 `has_dirty_static`，而它第一道门是 `if (!storage->IsDirty(id)) return false;`（`:533-541`）——dirty 已被上一步清空 ⇒ **恒 false** ⇒ `BumpStaticSceneRevision()`（`:224`）不执行。
- **生产时序正是这个顺序**：`PrepareRenderPassSetup`（`ContextRender.cpp:846-861`）里先 tick（`TransformSystem::Update`），帧入口最前段才 `SubmitTransformUpdates`。
- **测试为何绿**：`TestCSMIncrementalPass.cpp:1349-1362` 写入后**直接** `SubmitTransformUpdates()`，跳过 `Update()` ⇒ 覆盖不到"先 Update 再 Submit"。
- **删除路径同样不失效**：全仓 bump 点只有 5 处（`TransformSystem.cpp:224`、`MainLightShadowSystem.cpp:201`、`RenderPrimitiveCollectSystem.cpp:2140/2187/2200/2212`——后四条是解析失败的固化防御），`DestroyTransform → UnregisterTransform` 不在其中。
- **同意 C 的谨慎表述**：这是**信号链**结论，不是"GPU 已见残留阴影"；消费方是静态级联阴影缓存（`doc/csm-mechanism.md`）。
- **我补一句**：不是"调调用顺序"能修的——`Update()` 的 flat update 与 `Submit()` 的判据**共享同一个 dirty 位**，必须引入独立信号（结构事件位／"静态变更版本"），见 §4 批 0.4。

---

## 2. C 的 P2 复核 + 我补的两条

| # | 项 | 我的复核 | 证据 |
|---|---|---|---|
| P2-1 | 替换系统未从元素索引摘旧实例 | **成立** | `ContextSystems.cpp:155-177`：元素列表用 `*it == system`（比的是**新**实例）⇒ 旧实例留在 `systems_by_element_type`；组开关 `SetSystemGroupEnabled`（`:261-285`）从该列表重建快照 ⇒ 旧实例照样被 `SetEnabled`。**这是"多容器 = 多权威"唯一有实证的失步路径**（其余多容器属设计冗余） |
| P2-2 | 裸池查行不验活跃 + 析构后写对象 | **成立（两条）** | `ComponentPool.h:235-246` `Row()` 只判 `handle==0` 与 `next_index`，不查 ledger ⇒ `DestroyRow` 后仍返回非空指针，违背 `GetComponentByRow` 的"已释放返回 nullptr"；`DestroyRowAt`（`:209-220`）在 `row->~T()` **之后** `SetOwner(nullptr)`（写已结束生命期的基对象） |
| P2-3 | `Clear()` 重置世代 ⇒ 旧句柄复活 | **成立**，我补一条 | `EntityManager.cpp:96-122`（`slots.clear()` + `next_index=0`；被 `Shutdown` 在 `Context.cpp:271/320` 调用）。Transform 侧同构隐患在 `TransformDataStorage.h:869-893`（`generations.Clear()`），但 **`TransformDataStorage::Clear()` 全仓零调用者**（我新发现）⇒ 当前是"死 API + 潜在陷阱" |
| P2-4 | `Shutdown()` 半复位，旧变换行被新实体继承 | **成立** | `Context.cpp:264-291`（inactive 早退）/`:317-326`（active）：清实体、清 `static/movable_transforms` id 名单，但**从不**清变换行存储（上文 `Clear()` 零调用）⇒ 新实体复用旧 `EntityID` 后 `FindByOwner` 命中旧行。C 的二选一我同意：**要么完整复位（补 `storage->Clear()` 且世代不复位），要么复用后拒绝**（见 §7 P2） |
| B 补-1 | `LinesComponent::version`（`uint64`，`:67`；唯一写点 `TouchChange` `:70`）只写不读 | **成立**（C 指出，我复核） | 与 `dirty`/`change_mask`/`LineChange` 同批删；`TextComponent` 侧同名成员另有读者，改动按类型作用域 |
| B 补-2 | 删 `PrimitiveSortSystem` 的收益面 | **成立** | `PrimitiveRenderPipeline.cpp:29-46`：`RunCull`/`RunSort`/`RunBuild` **各自**再调一次 `impl_->PrepareFrame`，而 `PrimitiveBatchPipeline::PrepareFrame` **无 once-per-frame 守卫** ⇒ 现形态每帧多跑一次全量准备 + `Reset()`（A §3.5 已记该守卫缺失） |

---

## 3. 争议裁决表（三方合流）

| # | 议题 | 分歧 | 我的实测裁决（依据） |
|---|---|---|---|
| 1 | `MarkDirty()` "永不上传" | A/B 定高；C 说两方都重了 | **两方各半**：静态行上传按 `last_uploaded_version`（`TransformSystem.cpp:281-297` 的 else 分支）⇒ 不 bump version 的 `MarkDirty`（`TransformAccessor.cpp:529-533`）**静态行确实不上传**；动态行 `dynamic_force_full = (dynamic_count>0)`（`:254` → `:305-309`）每帧全量写 ⇒ **动态行照样上传**。全仓唯一调用者是**可动**手，且前一行 `SetLocalRotation`（`TransformAccessor.cpp:176-184`）已 bump version ⇒ **C 对**：该调用点冗余，不构成"当前时钟不上传"。合流结论 = **静态行语义陷阱 + 准死 API**，非当前故障 |
| 2 | 相机"双真值" | A/B 定高；C 说缺因果证据 | **采纳 C**：`GetCameraByEntity`（`Context.h:566-569`）经组件池，槽认领只在 `BindCameraResources`/`EnsureCameraSlot`（`CameraSystem.cpp:774-775`）⇒ 无"两份相机状态"证据；代码自带注释（`Context.h:559-561`）说的是**入口收敛**。降级为 API 收敛项 |
| 3 | RenderGraph 抬高 Update 下界 | A 判"名不符实"；C 说有意为之 | **采纳 C**：`PrepareRenderPassSetup`（`ContextRender.cpp:846-861`）已跑 Collect/Batch/Commit/Upload/FrameSync，随后 Update 自 `RenderDrawSubmit` 起是**避免在绘制段重跑前置**。剩余问题只是文档语义（`RenderGraph.cpp:107-112`） |
| 4 | `IsCurrentPassShadow` 9 处分类 | C 说 B 分类仍错 | **采纳 C，订正 B v1**：`:2372` 是 `SetRenderIDs(pass,…)` 状态写（`:2370-2372`）、`:2391` 是 D9 每实例重试计数复位（`:2388-2401`）⇒ **1 事实注入（:800）+ 6 程序选择/重试/状态（:1027,:1318,:2118,:2152,:2372,:2391）+ 2 诊断（:2421,:2425）**。B v1 把 `:2372/:2391` 记为"日志"是错的 |
| 5 | Lines 记账可删范围 | C 说 B 称 `MarkSynced()` 零调用是错的 | **措辞订正**：B v1 写的是"`MarkSynced()` **零读者**"，且已把调用点 `LineRenderPipeline.cpp:595` 列入删除清单 ⇒ C 的"零调用"是对我档**用词**的误读；但 B 承认该措辞紧挨删除清单易被读成"零调用"，v2 统一改为"**写者存在、效果无人读**"。可删范围 = 整条不再被消费的记账（`dirty`/`change_mask`/`LineChange`/`version`/`MarkSynced`/`ClearAllChanges`/`GetChangeMask`） |
| 6 | 重复释放的后果 | A 说下溢；B 说虚高；C 说少算 + 槽位别名 | **采纳 C，订正 B**：`GetActiveCount = GetCount() - GetFreeCount()`（`RenderItemDataStorage.h:99`）在 free_list 有重复项时是**少算**（`GetFreeCount` 多计）；真正危害是**同 handle 被两次分配**（槽位别名）。「下溢」与「虚高」都错 |
| 7 | 释放判定的实现方案 | B v1 提议"零 descriptor"判定；C 否决 | **采纳 C，B 方案作废**：`Allocate`/`AllocateContiguous`（`:39-58`）**合法**产生全零 descriptor ⇒ 载荷值不能当生命周期状态。改用显式分配状态（ledger／位图）或 `free_list` 去重校验 |
| 8 | `RenderItemHandle 0` = 空句柄 | A（及旧账本）如此写；C 判错 | **采纳 C**：`INVALID_RENDER_ITEM_HANDLE = UINT32_MAX`（`inc/hgl/graph/render/RenderItemDescriptor.h:44`），`Allocate` 首个返回值就是 `0`（`:39`）⇒ A §3.1 与旧账本错；**B v1 漏裁决此项** |
| 9 | `GetGPUVisibleCount` 缺 fence | A 定高；B/C 降级 | **合流**：函数无自守卫；两个调用点均已 fence（`GPUCullSystem.cpp:758-763`、`CPUCullReferenceSystem.cpp:416`）⇒ 定级"接口纪律"，无"当前读到旧帧"证据 |
| 10 | `AddSegment` 无回滚定级 | B v1 写"路径实际不可达"；C 要求收回绝对表述 | **各让一步**：预分配 + 前置有效性 + 每帧 `Reset` 使**常见路径**不可达，但无法证明不存在其它入口 ⇒ v2 改为"**防御性回滚，P3**"，删绝对表述 |
| 11 | `GetActiveCameraComponent` 隐式认领 | A/B 定高；C 归为命名/拆分 | **合流为 API 收敛项**（证据链不变：`CameraSystem.cpp:686-746`），非高严重度故障 |
| 12 | 有序 free list 弹最大索引 | A/B 判"自相矛盾" | **采纳 C**：O(N) 换**确定性复用次序**是取舍；改 LIFO 是优化，须同时说明"复用次序变化"的可见影响（`TestTransformFlatStorage` Test 13 已钉槽位契约） |
| 13 | CMake 双登记 | A/B 当"实际重复" | **采纳 C**：`ULRE.ECS.vcxproj` 中 `RenderSystemCore.cpp` **只 1 条** `<ClCompile>`（我实测）⇒ 源列表冗余，清理项 |
| 14 | `cl /Bv` | B v1 说"无此开关" | **采纳 C，订正 B**：实测存在（打印 Compiler Passes + `D8003`）。但 A 用它检查 `static_assert` 仍不成立（断言只由编译相关 TU 验证） |
| 15 | `PrimitiveSortSystem` | A 拍板保留；B v1 判据引 `RenderSort` | **采纳 C**：`ExecutionPhase` 枚举**没有** `RenderSort`（`System.h:22-49`：`RenderCollect, RenderBatch, RenderBufferCommit, RenderBufferUpload, RenderFrameSync, RenderDrawSubmit, RenderDebug, RenderStat, RenderSubmit`）⇒ B v1 该判据作废。合流：**保留**（与 A 拍板一致），收益面改写为"删掉可省每帧一次无守卫 `PrepareFrame`"（§2 B 补-2） |
| 16 | 多容器 = 多权威？ | A/B 倾向"多 = 冗余" | **采纳 C 的判据**：以**实际失步路径**定性；P2-1（系统替换）就是具体证据 ⇒ 收敛项落到"替换要摘旧实例"，而非"合并容器" |

---

## 4. 综合整改顺序（批 0 正确性 → 批 1 删除 → 批 2 语义 → 批 3 结构）

### 批 0 —— 正确性（**新增，优先级最高**；每项独立可编译）

| # | 内容 | 判据（红的样子） |
|---|---|---|
| 0.1 | `TransformDataStorage::AddChild` 搬迁补复制旧区间（`:571-589`） | 新增用例：交错装配（P→A、Q→B、P→C）后 `GetChild(P,0)` 必须是 A、`GetChildCount` 与实际子数一致；**反证**：把复制去掉即红。同补"删除后再追加"用例 |
| 0.2 | `TransformAccessor::SetParent` 环拒绝（`:218-239`）+ `MarkLocalChanged` 遍历可终止（`:73-100`） | 探针 `A.SetParent(B); B.SetParent(A)` 必须**返回**（改动前 >3s 不返回）且报一次明确错误；`TestTransformFlatStorage` Test 15 与拓扑契约仍绿 |
| 0.3 | 段预算事务化：`MigrateTransform`（`Context.cpp:862-906`）先取目标槽再提交换段，失败保留原段并回滚 mobility/名单；`CreateTransform`（`:922-941`）在登记失败时返回 `INVALID_TRANSFORM_ID` 并回收行 | 段满时 A 仍在原段、D 槽未被覆盖、无越界写；`overflow_has_slot == 0` ⇒ 返回无效值；新增"段满拒绝"用例 |
| 0.4 | 静态场景变更的**独立信号**（不再用 `matrixDirty` 代表"已通知失效"）；覆盖**移动 / 删除 / Mobility 换段** | 正常顺序（`Update → Submit`）移动静态物体 ⇒ revision **+1**；重复提交不再递增；删除最后一个静态实体 ⇒ **+1**。现有 `TestCSMIncrementalPass.cpp:1349-1362` 必须改成**走 `Update()`** 再断言（否则仍假绿） |

> 批 0 公共前置：`TransformDataStorage::Clear()` 是零调用死 API ⇒ 批 0.3 / 批 3 若要用它，先补调用点并让**世代不复位**。

### 批 1 —— 删除遗留与未使用（v1 四项不变）

| # | 内容 | 判据 |
|---|---|---|
| 1.1 | `ECSContext::GetAllEntities`（`Context.h:988-993`）+ `EntityManager::GetAllEntityPointers`（`EntityManager.h:121`、`.cpp:83-94`） | `git grep -n "GetAllEntities\|GetAllEntityPointers" -- inc src example` = **0**；`ForEachAliveEntity` 两入口保留（`RenderGraph.cpp:147-169` 在用） |
| 1.2 | `LinesComponent` 记账七件套：`LineChange`（`:23-27`）、`change_mask`（`:68`）、`GetChangeMask`（`:176`）、`ClearAllChanges`（`:177`）、`dirty`（`:47`）、`MarkSynced`（`:173`）、**`version`（`:67`）** + 写点（`:79,81,91,93,117,118,127,128`）+ 调用点 `LineRenderPipeline.cpp:595` | `git grep -n "GetChangeMask"` 只剩 `TextComponent`/`TransformDataStorage` 两族；`TestCSMIncrementalPass.cpp:5467` 一带的类型表针仍绿（钉的是 `ComponentTypeTable` 行 + `ComponentTypeOf`，不涉这些字段） |
| 1.3 | `CameraInputMapping` 的 `kActionResetView`(R)/`kActionFocusView`(F)（`.cpp:31-32` + 宏列表 `.h:25-26`） | `git grep -n "ResetView\|FocusView" -- inc src example` = 0 |
| 1.4 | `BoundingBoxAccessor` 删 `context`（`.h:28`）与 `GetContext()`（`.h:52`）、构造点 `Context.cpp:982,1000`、`static_assert 24→16`（`.h:126`） | 编译期 `sizeof(BoundingBoxAccessor)==16`；`TestBoundingBoxStorage` rc=0。**注**：`GetContext()` 全仓零调用者，且 `BoundingBoxDataStorage` **无** context 成员 ⇒ 直接删，不需 v1 提的 `storage->GetContext()` 反查 |

### 批 2 —— 语义订正

| # | 内容 | 判据 |
|---|---|---|
| 2.1 | `TransformAccessor::MarkDirty()`（`.cpp:529-533`）三选一（推荐：删公开 API + 改 `ClockUse.cpp:350`，该行本就冗余） | 删除后 `git grep -n "\.MarkDirty()"` 只剩存储自身私有路径；`TestCSMIncrementalPass.cpp:2000` 契约针仍绿 |
| 2.2 | `CameraSystem::GetActiveCameraComponent()`（`:322-331`）拆"只读查 / 幂等认领" | 只读口在无默认相机时不再写槽（`ClaimDefaultCamera` 不被调用） |
| 2.3 | `ComponentTypeTable.h:105` 88 B → **40 B** | `git grep -n "88 B" -- inc/hgl/ecs` = 0 |
| 2.4 | `LineCollectStats::culled_by_visibility` 拆出 `empty_components`（`LineRenderPipeline.cpp:433-437`） | 空线段不进可见性剔除数；日志字段可 grep |
| 2.5 | `RenderItemDataStorage::Release`/`ReleaseContiguous`（`:60-85`）补**显式**分配状态（**不用**零 descriptor 判据） | 新增"二次 `Release` 返回 false 且后续 `Allocate` 不再返回同一 handle"用例；`TestRenderItemDataStorage` rc=0 |

### 批 3 —— 结构（需拍板）

| # | 内容 | 判据 |
|---|---|---|
| 3.1 | 系统替换摘旧实例（`ContextSystems.cpp:155-177`） | 注册两次同类型后 `systems_by_element_type` 只留新实例；组开关不影响旧实例（反证：按现状注册两次可复现 C 的现象） |
| 3.2 | 裸池 `Row()` 补活跃判定（`ComponentPool.h:235-246`）+ `DestroyRowAt` 的 `SetOwner` 移到 `~T()` 之前（`:209-220`） | `DestroyRow` 后 `Row(h)==nullptr`；与 `SharedComponentPool` 语义对齐 |
| 3.3 | Shutdown/重用契约（`Context.cpp:264-326`）：补变换行存储复位（世代不复位）或复用后拒绝 | 未初始化世界 Shutdown → 新实体 `GetTransformID()` 不得命中旧行 |
| 3.4 | `TransformSystem` 裸遍历统一走 `ForEach*`（`:273-340`，共 5 处） | 手动跳洞点归零；`TestTransformFlatStorage` Test 15 仍绿 |
| 3.5 | 6 处进程级告警闩改 per-world（`GPUCullSystem.cpp:419,498`、`PrimitiveBatchPipeline.cpp:803,1002`、`PipelineMaterialRenderer.cpp:58`、`RenderPrimitiveCollectSystem.cpp:1454`） | 双世界各触发一次，两次都有错误行（现形态第二次静默） |
| 3.6 | `IsCurrentPassShadow` 9 处按 §3-#4 三分类写进 `RenderPrimitiveCollectSystem.h` 头注 | 9 处逐条归类；头注写清与 `RenderStrategyTable` 的分工 |
| 3.7 | CMake 源列表去重（`CMakeLists.txt:24,44` 与 `:47-49`）+ 有序 free list 取舍说明 | 每个文件名在 `ECS_SOURCE` 出现 1 次；free list 若改 LIFO 须说明复用次序变化（Test 13 契约同步） |
| 3.8 | 9 套系统容器（`Context.h:118-142,261`）与 A §2.2 六条待拍板**合并拍板** | 拍板后单独立批，本评审不动 |

---

## 5. 文件修正清单

### 5.1 本次就地改动面（4 个文件）

| 文件 | 修正 |
|---|---|
| 本档（`ecs-deep-audit-crossreview-2026-10.md`） | **v1 → v2 综合版**（整体重写，含 §6 变更记录） |
| `ecs-deep-quality-audit-and-evidence-2026-10.md`（A） | v1 已插「⚠ 交叉评审订正」块；**本批补 2 条**：§3.1「`RenderItemHandle 0` = 空句柄」错（`UINT32_MAX` 才是无效，`RenderItemDescriptor.h:44`）；§3.2 的 9 处分类订正为 **1+6+2**（`:2372/2391` 是状态写、`:2421/2425` 是诊断） |
| `ecs-quality-audit-2026-10.md`（旧账本） | v1 已插「交叉评审入口」块；同一条 `RenderItemHandle 0` 订正适用（其 §4.2 列了该约定） |
| `ecs-deep-audit-second-crossreview-2026-10-11.md`（C） | **新增**「综合结论指针」块：指向本档；标注其 4 项 P1 / 4 项 P2 已被独立**代码级**复核（3 项逐字、P1-4 影响待目视）；标注 3 处需订正（`MarkSynced` 措辞误读、探针文件未入库、测试数口径写 9+1） |

### 5.2 各档自身订正点

- **A**：§1.1② 机制句（`dirty` 无读者）· §1.2③ 已删 API 名 · §2.3 键字段名 · §3.1 `RenderItemHandle 0` + 光源行"8 处"→≈9 · §3.2 三分类 · §3.3 第二例换真闩（6 处）· §4.2 两个计数 · §5 行号四处 + `runUpdate` 名 · §6.1/§6.2/§6.3 定级与半句 · §6.4 补零调用者 · §7 重排 + 判据 · §8.2 测试数 9+1 · §8.3 `cl /Bv` 措辞（开关存在，但检不了 `static_assert`）。
- **C**：① "第二份称 `MarkSynced()` 零调用" → 实为"零**读者** + 调用点已入删除清单"；② 建议把 4 个探针文件（`ecs_crossreview_probe.cpp` + 3 个 `.log`）落到仓内，否则数字不可复查；③ "重新构建并运行 10 项测试"宜写成 **9（`-L ecs`）+ 1（`TestCSMIncrementalPass`）**，与 `ctest -N` 口径一致。
- **B**：见 §6 变更记录 10 条。

### 5.3 连带受影响文档集（批 0/1/2 落地时**同批**订正）

| 主题 | 文档 |
|---|---|
| 静态级联失效契约（批 0.4） | `doc/csm-mechanism.md`（§6 结论节）、`doc/ecs-layer-architecture-and-frame-flow.md`（tick/render 时序）、`doc/ecs-quality-audit-2026-10.md`（§8 P2 行） |
| 子表 CSR / 层级（批 0.1-0.2） | `doc/future/ULRE_Transform_System_Completion_Design_and_Plan.md`、`doc/future/stage_B_debt_list.md`、`doc/future/stage_B_execution_plan.md` |
| 段预算 / 迁移（批 0.3） | `doc/future/entity-rows-gpu-cull-plan.md`、`doc/global-addresses-bda-unification-plan.md` |
| `PrimitiveSortSystem` / `RunSorting` | `doc/ecs-layer-architecture-and-frame-flow.md`、`doc/render-flow-cull-sort-drawcall.md`、`doc/simple-sphere-ecs-render-chain.md` |
| `change_mask` / `LineChange`（批 1.2） | `doc/ecs-layer-architecture-and-frame-flow.md`、`doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md`、`doc/future/ULRE_Transform_System_Completion_Design_and_Plan.md`、`doc/future/entitygpu-and-ulrescene-design.md`、`doc/future/ULRE_Architecture_Technical_Review_20260930.md` |
| 88 B（批 2.3） | `doc/analysis/ecs-audit-2026-10/02-entity-component-list-copies.md`、`doc/future/ULRE_FINAL_TARGET_v2_设计约束.md`、`doc/future/ULRE_Phase0-1_….md`、`doc/future/ULRE_Architecture_Technical_Review_20260930.md`、`doc/bc5-normal-maps.md`、`doc/simple-sphere-ecs-render-chain.md`、`doc/global-addresses-bda-unification-plan.md`、`doc/gpu-driven-4id-draw-item-plan-2026-09.md`、`doc/meshlet-geometry-pipeline-implementation.md` |
| `BoundingBoxAccessor`（批 1.4） | `doc/future/ULRE_Phase0-1_….md`、`doc/future/ULRE_Transform_System_Completion_Design_and_Plan.md`、`doc/future/stage_B_debt_list.md`、`doc/future/stage_B_execution_plan.md` |
| `MarkDirty`（批 2.1） | `doc/ENVIRONMENT_SYSTEM.md`、`doc/GUI_Text_Render_Pipeline.md`、`doc/backlog.md`、`doc/analysis/ecs-audit-2026-10/03-…`、`06-…`、`doc/csm-review-2026-09-25.md` |

> 判据：改完 `git grep -n "<旧字面量>" -- doc/` 的命中必须落在**带标注**（`已删/已改/历史/~~`）的句子或其紧邻上下文里，或命中 0。订正清单自身点名旧符号是必须的，判定按"块"看。

---

## 6. 变更记录（B v1 → v2）

| # | v1 的说法 | v2 结论 | 原因 |
|---|---|---|---|
| 1 | §5.5 `MarkDirty` ✅真（"标的是永不上传的活"） | **两方各半**：静态行成立、动态行不成立；唯一调用点冗余 | 静态上传按 version（`TransformSystem.cpp:281-297`），动态 `dynamic_force_full` 每帧全量写（`:254,:305-309`） |
| 2 | §3.2 把 `:2372/:2391` 归为"日志" | 二者是**状态写**（4-ID 表按 pass、D9 重试复位） | 读码 `:2370-2372` / `:2388-2401` |
| 3 | §6.2 写"活跃计数**虚高**" | **少算 + 槽位别名** | `RenderItemDataStorage.h:99` 的减法方向（free 多计 ⇒ active 少算） |
| 4 | 批 2.5 "以 `items[h]` 零化判已释放" | **作废**，改显式分配状态 | `Allocate`/`AllocateContiguous` 合法产生全零 descriptor |
| 5 | §8.3 "MSVC 无 `/Bv`" | **错**：实测存在（打印 Compiler Passes） | 本机 `cl` 14.51 实跑 |
| 6 | §7.11 CMake 双登记（当"实际重复"） | **登记冗余、无重复编译** | `ULRE.ECS.vcxproj` 单条 `<ClCompile>` |
| 7 | 批 3.1 判据引 `RenderSort` 相位 | 该枚举值**不存在** | `System.h:22-49` 实测 |
| 8 | §6.1 `AddSegment` "中途失败路径实际不可达" | 收口为"常见路径不可达 + **防御性**，P3" | 无法证明不存在其它入口（C 的异议合理） |
| 9 | 未裁决「`RenderItemHandle 0` = 空句柄」 | **错**：`INVALID_RENDER_ITEM_HANDLE = UINT32_MAX` | `RenderItemDescriptor.h:44` |
| 10 | 未收录任何"正确性"项（只复述 A 的代码存在性） | **批 0 新增 4 项 P1**（C 发现，我逐条复核成立） | §1 |

---

## 7. 待拍板项

| # | 项 | 选项 | 推荐 | 影响 |
|---|---|---|---|---|
| P1 | 批 0 是否立刻开 | (a) 先开 0.1 + 0.2（子表搬迁 + 环检测） (b) 先做批 1 删除 | **(a)** | 两项是数据损坏级；删除批随时可做、不阻塞 |
| P2 | `Shutdown` 重用契约（批 3.3） | (a) 完整复位（补 `storage->Clear()` 且世代不复位） (b) 复用则拒绝 | **(a)** | 决定"世界可否重用"的对外语义，也决定 P2-3 世代陷阱的处置面 |
| P3 | `PrimitiveSortSystem` | (a) 保留（A 拍板 + C 建议） (b) 删除（省一次无守卫 `PrepareFrame`） | **(a)** 本轮保留；(b) 记 backlog，与"`PrepareFrame` 守卫"同批 | 收益是性能面而非结构面，适合与守卫一起做 |
| P4 | 路线图顺序 | (a) 批 0 → 批 1 → 批 2 → 批 3（本档） (b) A 的顺序 | **(a)** | 与项目口径一致（先正确性、再删遗留、再语义、再结构） |

---

## 8. 附录

### 8.1 复现命令集（cwd = 仓库根）

```bash
# 批 0 相关（读码定位；探针需 C 的 support 文件或自建）
sed -n '571,589p' inc/hgl/ecs/support/TransformDataStorage.h     # AddChild 不搬旧区间
sed -n '218,239p' src/ecs/support/TransformAccessor.cpp          # SetParent 无环检测
sed -n '862,906p' src/ecs/core/Context.cpp                       # Migrate 失败写目标段
sed -n '922,941p' src/ecs/core/Context.cpp                       # CreateTransform 无法感知登记失败
sed -n '195,224p' src/ecs/systems/tick/TransformSystem.cpp       # has_dirty_static 判据

# 计数与契约
ctest --test-dir build -C Release -N -L ecs                      # 实测 9
./build/out/Windows_64_Release/TestTransformFlatStorage.exe      # Test 9: 每行 158 B / rc=0
grep -c "RenderSystemCore.cpp" build/src/ecs/ULRE.ECS.vcxproj    # 实测 1
"$VS/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe" /Bv                 # Compiler Passes 版本表

# 文档侧回扫
git grep -n "88 B\|88B\|runUpdate\|RegisterRenderSystem\|INVALID_RENDER_ITEM_HANDLE" -- doc | head -40
```

### 8.2 未被本档独立复现的项（如实标注）

- **C 的探针数字**（`expected_first=2 actual_first=4`、`a_static=0 a_dynamic=1 d_dynamic=0 a_movable=1`、`overflow_handle=1 overflow_valid=1 overflow_has_slot=0`、`before=1 after=1 world_x=4.0`、父环 >3s）：我只做**代码级复核**（机制与代码逐字吻合），未跑 C 的 `ecs_crossreview_probe.cpp`。建议把 4 个探针文件落进仓内（`src/ecs/support/` 或 PR 附件），下一个会话可复跑。
- **GPU／视觉后果**：批 0.4 的"阴影残留"是信号链结论，未目视；`AddSegment` 错位、`SetParent` 环的可见后果同样需示例（本会话无声，GUI 示例有已知早退现象）。
- **全量构建与 10 项测试全绿**：本轮只跑 `ctest -N`（清单）与 `TestTransformFlatStorage`（rc=0）。

### 8.3 本档边界

只读评审：未改引擎代码；改动面 = §5.1 的 4 个文档。行号属 `e8ee755fd`，下一批开工前先 `git log --oneline -1` 核对是否仍在该提交。
