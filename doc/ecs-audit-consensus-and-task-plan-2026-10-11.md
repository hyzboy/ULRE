# ECS 审计结论分档与执行清单（2026-10-11）

> **本档用途**：把三轮审计/评审（见 §5 索引）的**全部**结论按「有没有分歧」重新分档，让**无争议项可以立刻开工**，有分歧项各自挂上待拍板点。
> **分档口径**（判"无争议"的规则）：
> · **无争议** = 事实与做法均无异议；**某一方"未提及"不算异议**（只算覆盖缺口，记进 §4）。
> · **确认问题、处理方式不一致** = 事实（现象/位置/计数）三方认可，但**做法有分叉**（含单方给出多选一）。
> · **三方争议** = 对**事实或定性**本身有分歧（谁对谁错见 §3 的实测裁决）。
> · **其它** = 覆盖缺口、文档面修正、未复现项、本档边界。
> **快照**：`e8ee755fd`；**行号均属该提交**，跨提交引用写成「文件:符号」。
> **性质**：只读评审 + 执行清单；本档自身**不改引擎代码**。
> **三方代号**：**A** = `doc/ecs-deep-quality-audit-and-evidence-2026-10.md`（原始审计档）· **B** = `doc/ecs-deep-audit-crossreview-2026-10.md` v2 综合版 · **C** = `doc/ecs-deep-audit-second-crossreview-2026-10-11.md`（第二轮，带探针）。

---

## 0. 执行口径（每一批都按这个验收）

1. **每批自身可编译 + 测试绿**；批与批之间可独立提交。
2. 验收门（三件套，缺一不算过）：全仓 Release **0 error** + `ctest --test-dir build -C Release -L ecs` **9/9** + `TestCSMIncrementalPass` **1/1**。
3. **每条删除/修正都要有"红的样子"**：删符号 ⇒ `git grep -n "<符号>" -- inc src example` 命中 **0**；改行为 ⇒ 新增/改造用例，并给**反证**（把改动去掉即红）。
4. **删头文件里的符号后按既有纪律清 obj 再全量重编**（本仓 ABI 错位会伪装成任意失败）。
5. 契约面已核：§1 全部符号在 `TestCSMIncrementalPass.cpp` 命中数为 **0**（`GetAllEntities`/`GetAllEntityPointers`/`LineChange`/`kActionResetView`/`AddFreeIndex`/`culled_by_visibility`/`BoundingBoxAccessor`/`MarkDirty`/`IsValidTransformID`）；`TestCSMIncrementalPass.cpp:5467` 一带钉的是 `ComponentTypeTable` 行与 `ComponentTypeOf<LinesComponent>` 映射，**不涉** `dirty`/`change_mask`/`version`。
6. **推荐开工顺序**：`1.1 正确性` → `1.2 删除` → `1.3 小修正`。理由：1.1 是数据损坏/信号链级，1.2/1.3 随时可做且不阻塞前者；用户的"先删遗留"口径在 1.1 之后立即执行，不冲突。

---

## 1. 无争议 —— 可直接开工

### 1.1 批 B：正确性（**推荐最先**；4 项，各自独立可编译）

#### B1 · CSR 子表搬迁不复制旧区间 —— 静默损坏子表
- **位置**：`inc/hgl/ecs/support/TransformDataStorage.h:571-589`（`AddChild`）
- **事实（三方无异议）**：`child_offset[id]` 被重指到池尾后**只追加 1 项**，却写 `child_count[id] = count + 1`；旧子节点没搬过去。`CopyChildren`（`:557-569`）随后用 `[offset, offset+count)` 读 ⇒ 可**超出 `child_pool.size()`**（UB）。
- **触发面**：任意"父 X 先有子、另一父 Y 也有子、X 再加子"的装配顺序（C 的最小触发 P→A、Q→B、P→C）。
- **实现提示**：`child_pool` 是 `std::vector<HandleID>`（`:81`）。**先 `reserve(size()+count+1)` 再 `insert(end(), begin()+offset, begin()+offset+count)`，最后 `push_back(child)`、`child_offset[id]=新起点`** —— 先保容量才不会在自引用插入时迭代器失效（"旧 span 成为垃圾、不回收"的既有设计保持不变）。
- **判据**：新增用例覆盖 ① 交错装配 ② `RemoveChild` 后再 `AddChild`（`:591-620` 前移保持连续，同样不贴池尾）③ `GetChildCount` 与实际子数一致、`GetChild(P,0)` 仍是首个旧子；**反证**：去掉复制即红。既有 `TestTransformFlatStorage.cpp:702-704` 只走续写分支，必须补交错用例。
- **连带**：`TransformAccessor::MarkLocalChanged`（`TransformAccessor.cpp:73-100`）靠该遍历传播脏标记 ⇒ 修好前"后代上传"会漏项/越界。

#### B2 · 访问器 `SetParent` 成环 ⇒ 后代遍历不终止
- **位置**：`src/ecs/support/TransformAccessor.cpp:218-239`（`SetParent`）+ `:73-100`（`MarkLocalChanged`）
- **事实（三方无异议）**：`SetParent` 只拒自环（`:225`），不拒祖先环；`MarkLocalChanged` 用栈展开 `CopyChildren`，**无 visited/无深度上限** ⇒ `A.SetParent(B); B.SetParent(A)` 后无限循环（C 探针 >3s 未返回）。**不能**拿 `RebuildTopologyOrder()` 的环保护当安全证明（卡在重建之前）。
- **实现提示**：改父前做祖先链上溯检测（O(depth)）并 fail-fast；标脏遍历加 per-row epoch 标记（零分配）作二道防线。
- **判据**：环探针**必须返回**且报一次明确错误；`TestTransformFlatStorage` Test 15（名单空洞安全）与拓扑契约仍绿。

#### B3 · 段预算拒绝非原子（迁移写错段 + 首建半成品）
- **位置**：`src/ecs/core/Context.cpp:862-906`（`MigrateTransform`）、`:922-941`（`CreateTransform`）、`:832-860`（`RegisterTransform`，返回 `void`）；触发方 `src/ecs/support/TransformAccessor.cpp:500-512`
- **事实（三方无异议）**：
  1. `:887` 的复位表达式 `(toMovable ? movable_transforms : static_transforms)[old_slot] = id;` —— **两支都是目标段**却用**旧段下标** ⇒ 段满时把 `id` 覆盖到目标段（挤掉他人槽），`old_slot` 超目标段长时越界写；旧段槽已进 free list 未撤销。
  2. 访问器在 `MigrateTransform` **之前**就改了 `storage` 的 mobility ⇒ 失败时 mobility/段账目/id 名单三方不一致。
  3. `CreateTransform` 调 `RegisterTransform` 却拿不到结果（void）⇒ **有效 id 但无 L2W 槽**（C 实测 `overflow_valid=1 / overflow_has_slot=0`）；`:931` 的"不再造半个变换"注释只覆盖总预算拒绝。
- **实现提示**：先取目标槽、成功后再提交换段 + mobility；失败保留原段（并回滚已归还的旧槽）；`RegisterTransform` 改返回 `bool`（或让 `CreateTransform` 检测 `TryGetTransformSlot` 失败）并在失败时回收已分配行。
- **判据**：段满用例下 A 仍在原段、D 槽未被覆盖、无越界；`CreateTransform` 在登记失败时返回 `INVALID_TRANSFORM_ID`。

#### B4 · 静态场景变更信号链断裂（移动/删除都不通知失效）
- **位置**：`src/ecs/systems/tick/TransformSystem.cpp:75-79`（`Update` 开头无条件 `UpdateDirtyWorldMatricesFlat`）、`:195-199`、`:531-551`（`ShouldUpdateTransform` 首门 `!storage->IsDirty(id)`）；`TransformDataStorage.h:858-861`（清 `matrixDirty`）
- **事实（三方无异议）**：正常帧序（`PrepareRenderPassSetup` 里 tick 先 `Update`、帧入口再 `SubmitTransformUpdates`）下，dirty 已被清 ⇒ `has_dirty_static` 恒假 ⇒ `BumpStaticSceneRevision()`（`:224`）不执行；`DestroyTransform → UnregisterTransform` 也不 bump（全仓 bump 点只有 `TransformSystem.cpp:224`、`MainLightShadowSystem.cpp:201`、`RenderPrimitiveCollectSystem.cpp:2140/2187/2200/2212`）。
- **口径**：这是**信号链**结论；"GPU 阴影残留"需示例目视（不写进本批判据）。
- **实现提示**：**不能**靠调调用顺序修（flat update 与 Submit 判据共享同一个 dirty 位）⇒ 需独立信号（结构事件位/静态变更版本），并把 Mobility 换段、删除一并纳入。
- **判据**：① 正常 `Update → Submit` 后移动静态物体 ⇒ revision **+1**；② 重复提交不再递增；③ 删除最后一个静态实体 ⇒ **+1**。现有 `TestCSMIncrementalPass.cpp:1349-1362` 写入后**直接 Submit、跳过 Update** ⇒ 必须改成走 `Update()` 再断言，否则仍假绿。

> ### ✅ 批 1.1 执行记录（2026-10-11）—— 四项正确性修复落地，全绿 + 四组反证精确变红
>
> **改动（B1/B2/B3/B4，代码 6 文件 + 用例 3 文件）**
> · **B1** `inc/hgl/ecs/support/TransformDataStorage.h`（`AddChild`：区间不贴池尾时**整段复制**旧子区间
>   `reserve + insert(end(), begin()+offset, begin()+offset+count)` 后再 `push_back`，`child_offset[id]` 重指新起点；
>   原实现只追加 1 项却写 `count+1` ⇒ 旧子节点丢失 + `CopyChildren` 可越界）
> · **B2** `src/ecs/support/TransformAccessor.cpp`：`SetParent` 改父前**上溯祖先链拒绝成环**（fail-fast，零分配）；
>   `MarkLocalChanged` 后代遍历加终止闸（`walker == id || steps > max_steps`）作二道防线
> · **B3** `inc/hgl/ecs/core/Context.h` + `src/ecs/core/Context.cpp`：
>   `RegisterTransform`/`MigrateTransform` 改返回 **`bool`**；两者统一为「**先取目标槽、成功后才交还旧槽**」的原子口径；
>   `CreateTransform` 登记失败即 `Deallocate` 回收行并返回 `INVALID_TRANSFORM_ID`（不再返回"有行无 L2W 槽"的半成品）；
>   `TransformAccessor::SetMobility` 改为**先换段成功、再改 mobility 真源**（被拒即原状返回 + 一条 warn）
> · **B4** `TransformDataStorage.h` 新增单调 **`static_change_seq`** + `GetStaticChangeSeq()`（六处 `++`：
>   `TouchChange`（静态行）/`SetDirty`（静态行）/`SetMobility`（值变化）/`Allocate`/`Deallocate`）；
>   `inc/hgl/ecs/systems/tick/TransformSystem.h` 加 `last_static_change_seq`；
>   `src/ecs/systems/tick/TransformSystem.cpp` 判据由「只看 dirty 位」改为 `has_dirty_static || static_scene_changed`
>   并在上报后消费序号 ⇒ 与 per-row 脏位生命周期解耦（`Update()` 先清 `matrixDirty` 不再能吞掉信号）
>
> **用例**
> · `src/ecs/support/TestTransformFlatStorage.cpp`：**Test 16**（CSR 搬迁契约：交错追加 / `RemoveChild` 后再加）、
>   **Test 18**（段预算拒绝的原子性：换边整体拒绝 / 目标段槽位不被挤 / 换边计数与 mobility 真源不动 /
>   段满创建回收行（**世代归零**，`Allocate` 只追加故以世代判泄漏）/ 失败后动态段仍可正常建行）
> · `src/ecs/support/TestCSMIncrementalPass.cpp`：**Test 9D 改造** —— 改走**正常帧序 `Update → Submit`**
>   （旧写法直接 `Submit`、跳过 `Update`，正好掩盖 B4 缺陷 ⇒ 原用例是假绿），并新增
>   「**删除静态实体 ⇒ `static_scene_revision` 必须 +1**」判据
> · `src/ecs/CMakeLists.txt`：两个测试目标加 `TIMEOUT`（防历史死循环挂住 ctest）
>
> **验证（口径：清 obj 全量重编）**
> · 全仓 Release：**`BUILD_RC=0` / `error C*|LNK|MSB` = 0 / `warning C*` = 0** / **389 目标**
> · `ctest --test-dir build -C Release -L ecs` → **9/9 passed**；`-R TestCSMIncrementalPass` → **1/1 passed**；
>   `TestTransformFlatStorage` 单跑 **rc=0**（Test 16/17/18 全过）
>
> **反证（四组：打回原形 ⇒ 精确变红 ⇒ 逐字节还原 ⇒ 复绿；`sha256` 还原一致 = True）**
> · **B3-a** 换段被拒仍去写「目标段 + 旧下标」 ⇒ Test 18 红：`被拒的换边把该行从源段摘掉了（源段查不到 id=0）`
> · **B3-b** 丢弃 `RegisterTransform` 的失败结果 ⇒ Test 18 红：`静态段满仍返回有效变换 id=1（半成品：有行无 L2W 槽）`
> · **B4-a** 判据回退成只看 dirty 位 ⇒ Test 9D 红：`moving a Static transform must bump static_scene_revision`
> · **B4-b** 不消费序号（不记 `last_seq`） ⇒ Test 9D 红：`unchanged scene must not keep bumping static_scene_revision`
>
> **反证过程中踩到并已固化的一条坑**：首轮 B1 反证**假绿** —— 把 `if(...)` 改成 `if(false)` 后 Test 16 仍过，
> 原因是 **MSBuild 未重编 include 了该头的测试 TU**（头改了、`obj` 复用旧头 ⇒ 跑的还是修复后代码）。
> 补 `obj` purge 后才精确变红 ⇒ **头文件级反证必须先显式 purge 依赖 TU 的 obj**（本档口径已按此执行）。
>
> **本批明确未做**：S1（系统替换要摘旧实例）/ S2（裸池 `Row()` 不验活跃）留给下一批；
> `RegisterTransform` 幂等路径未单独立用例；B4 的"GPU 阴影残留"仍只有信号链级证据（未做目视）。

### 1.2 批 A：删除遗留（4 项，独立可编译，最安全）

| # | 内容（`文件:符号`） | 三方一致点 | 判据（红的样子） |
|---|---|---|---|
| A1 | `ECSContext::GetAllEntities`（`Context.h:988-993`）+ `EntityManager::GetAllEntityPointers`（`EntityManager.h:121`、`.cpp:83-94`） | A 提出、B 复核零调用（全仓 5 命中全在自身）、C 明确"仍可保留"该事实 | `git grep -n "GetAllEntities\|GetAllEntityPointers" -- inc src example` = **0**；`ForEachAliveEntity` 两入口保留（`RenderGraph.cpp:147-169` 在用） |
| A2 | `LinesComponent` 记账七件套：`LineChange`（`:23-27`）、`change_mask`（`:68`）、`GetChangeMask`（`:176`）、`ClearAllChanges`（`:177`）、`dirty`（`:47`）、`MarkSynced`（`:173`）、`version`（`:67`）+ 写点（`:79,81,91,93,117,118,127,128`）+ 唯一调用点 `LineRenderPipeline.cpp:595` | C 确认"`dirty/change_mask` 无业务读者"并补 `version` 只写不读；"可清理的是不再被消费的整条记账" | `git grep -n "GetChangeMask"` 只剩 `TextComponent` / `TransformDataStorage` 两族；**按类型作用域**（`TextComponent` 的 mask 是活的：`TextRenderPipeline.cpp:664,944`） |
| A3 | `CameraInputMapping` 的 `kActionResetView`(R) / `kActionFocusView`(F)：绑定 `.cpp:31-32` + 宏列表 `.h:25-26` | A 提出、B 复核、C 保留 | `git grep -n "ResetView\|FocusView" -- inc src example` = **0** |
| A4 | `BoundingBoxAccessor` 删 `context`（`.h:28`）与 `GetContext()`（`.h:52`）、构造点 `Context.cpp:982,1000`、`static_assert` `24 → 16`（`.h:126`） | A 列为未对齐范式、C 列为"瘦身候选"、B 复核 `GetContext()` **全仓零调用者**且 `BoundingBoxDataStorage` **无** context 成员 ⇒ 直接删即可（不需 `storage->GetContext()` 反查） | 编译期 `sizeof(BoundingBoxAccessor)==16`；`TestBoundingBoxStorage` rc=0 |

> ### ✅ 批 1.2 执行记录（2026-10-11）—— 四项删除落地，全绿
>
> **改动（10 文件，全仓零调用已逐个复核）**
> · **A1** `inc/hgl/ecs/core/Context.h`（删内联 `GetAllEntities`）· `EntityManager.h`（删声明 + 改写上方 `ForEachAliveEntity` 注释里的旧接口指代）· `EntityManager.cpp`（删定义）
> · **A2** `LinesComponent.h`（删 `LineChange` 枚举 / `dirty` / `version` / `change_mask` / `TouchChange` / `GetChangeMask` / `ClearAllChanges` / `MarkSynced` + 8 处写点）· `LineRenderPipeline.cpp`（删唯一调用 `comp->MarkSynced()`；随之只写不读的 `comp_write_ok` 一并删）· `TextComponent.h`（**同族注释回扫**：线条侧已删、文本是唯一活消费者）
> · **A3** `CameraInputMapping.h`（X 宏列表删两行 + `X(ZoomOut)` 去续行反斜杠）· `CameraInputMapping.cpp`（删 R/F 两条 `BindKey`）
> · **A4** `BoundingBoxAccessor.h`（删 `context` 成员 / `GetContext()` / 构造第三参，`static_assert` 24→16，类文档同步）· `Context.cpp`（两个构造点改 2 参）
>
> **验证（口径：清 obj 全量重编）**
> · 清整棵 obj 树（1804 个 `.obj` + `*.module.json`）后：全仓 Release **`BUILD_RC=0` / `error C*|LNK|MSB` = 0** / **389 目标**
> · `ctest --test-dir build -C Release -L ecs` → **9/9 passed**；`-R TestCSMIncrementalPass` → **1/1 passed**
> · 受影响二进制单跑：`TestBoundingBoxStorage`（16 B 断言）/ `TestComponentMask` / `TestTransformFlatStorage` / `TestRenderItemDataStorage` 全部 **rc=0**
> · 编译告警：本批文件 **0** 条（全量日志 `warning C*` 计数也是 0）
>
> **残留判据（"红的样子"的实测值）**
> · `GetAllEntities` / `GetAllEntityPointers` / `kActionResetView` / `kActionFocusView` / `LineChange` / `MarkSynced()` → 全仓 **0**
> · `ClearAllChanges` / `GetChangeMask` → 只剩 `TextComponent.h:69` + `TextRenderPipeline.cpp:944` 一族（活路径）✓
> · `BoundingBoxAccessor.h` 内 `context` / `GetContext` → **0**；两个构造点已是 2 参形式 ✓
>
> **行尾/BOM**：10 文件逐一与 `HEAD` blob 比对（`Context.h`/`Context.cpp` = **BOM+LF**，其余按各自原状 BOM+CRLF / 无 BOM+CRLF）⇒ `loneLF=0`、`CRCRLF=0`、BOM 状态不变；`git diff --numstat` 全部是定向增删（最大一处 0/32），**无整篇翻转**。
>
> **本批明确未做**：`LineStyle::Dashed/Dotted`（旧账本另记）· `LinesComponent::local_bounds_valid`（活，保留）· `BoundingBoxAccessor.h` 里的 `class ECSContext;` 前置声明（**有意保留**：删它可能让只经该头间接可见 `ECSContext` 的 TU 断链，属零收益风险）。

### 1.3 批 C：小修正与统计（6 项）

| # | 内容 | 三方一致点 / 为何无争议 | 判据 |
|---|---|---|---|
| C1 | `ComponentTypeTable.h:105` 注释 88 B → **40 B**（对齐 `MaterialRuntimeTable.h:221` 的 `static_assert`） | 三方均列为"注释与断言冲突" | `git grep -n "88 B" -- inc/hgl/ecs` = 0（doc 侧见 §4） |
| C2 | `LineCollectStats::culled_by_visibility` 把"空线段"拆出去（`LineRenderPipeline.cpp:433-437`；真视锥/可见性剔除在 `:443-447`） | 三方均列为统计污染 | 空线段不再计入；日志新增字段可 grep |
| C3 | `LineRenderPipeline.cpp:570-573` 恒真守卫（`const uint32_t > UINT32_MAX`）删除 | 三方均列为"假信心" | 该三行消失；`git grep -n "kMaxTransformID" -- src` = 0 |
| C4 | `MainLightShadowSystem.cpp:206` 的 `deltaTime` 加 `(void)` 或直接删形参（`:206-366` 全函数只出现一次）；`TransformSystem.cpp:68-70`、`MaterialData.cpp:360-364` 已是 `(void)` 形态 | 三方均列为"接受即丢弃" | 编译 0 error；若删形参，调用点 `ContextRender.cpp:167` 同批改 |
| C5 | 6 处进程级告警闩改 per-world：`GPUCullSystem.cpp:419,498`、`PrimitiveBatchPipeline.cpp:803,1002`、`PipelineMaterialRenderer.cpp:58`、`RenderPrimitiveCollectSystem.cpp:1454` | A 提 2 处（第二例错，B 已纠正为真闩 6 处）；C 未涉及 ⇒ 无异议 | 双世界各触发一次，**两次都有错误行**（现形态第二次静默） |
| C6 | `TransformSystem` 裸遍历统一走 `ForEach*`（`:273-277,281-297,305-309,313-329,334-340` 共 5 处） | A 提出、B 复核（含 A 漏的第 5 处）、C 未涉及 | 手动跳洞点归零；`TestTransformFlatStorage` Test 15 仍绿 |

> ### ✅ 批 1.3 执行记录（2026-10-11）—— 六项小修正落地，全绿 + C5 反证精确变红
>
> **改动（15 文件 / 24 处）**
> · **C1** `inc/hgl/ecs/support/ComponentTypeTable.h`：`MaterialRuntimeSlot` 行宽注释订正为 **40 B**
>   （与 `MaterialRuntimeTable.h` 的 `static_assert(sizeof==40)` 对齐）
> · **C2** `inc/hgl/ecs/support/line/LineRenderPipeline.h` 新增 `skipped_empty`；`LineRenderPipeline.cpp`
>   的空线段分支改为计入该字段（原来是计入 `culled_by_visibility`）；**三处日志同步**加 `skipped_empty=%u`
>   （`LineRenderPipeline.cpp` / `LineCollectSystem.cpp` / `LineStatsSystem.cpp`）⇒ `culled_by_visibility` 自增点
>   全仓只剩**真可见性剔除 1 处**
> · **C3** `LineRenderPipeline.cpp`：删恒真守卫（`resolved > UINT32_MAX` 恒 false）⇒ `transform_id = resolved;`
> · **C4** `MainLightShadowSystem.cpp`：`RenderMainLightShadowPass` 的 `deltaTime` 显式 `(void)` 丢弃
>   （与 `TransformSystem.cpp` / `MaterialData.cpp` 同写法；该文件另外那处 `(void)deltaTime` 是原有的）
> · **C5** 6 处进程级告警闩 → **每实例**成员：`GPUCullSystem.h/.cpp`（`reported_misaligned_rows_` /
>   `reported_unsynced_map_`）、`PrimitiveBatchPipeline.h/.cpp`（`gpu_cull_reject_warned_` /
>   `gpu_cull_engaged_logged_`，`reject` lambda 改捕获 `this`）、`PipelineMaterialRenderer.h/.cpp`
>   （`engaged_logged_`）、`RenderPrimitiveCollectSystem.h/.cpp`（`arena_trace_done_`）
> · **C6** `src/ecs/systems/tick/TransformSystem.cpp`：`SubmitTransformUpdates` 内 **6 处裸遍历**统一走
>   `world->ForEach{Static,Movable}Transform`（槽号由 `slot_map` 反查，`static_slot_map[handle] = slot`
>   见 `RefreshHandleOrder`）+ 删掉仅供裸遍历用的两个局部引用
>
> **新增判据程序**：`src/ecs/support/ProbeWarnLatchPerWorld.cpp`（+ `src/ecs/CMakeLists.txt` 注册目标）
> —— 两个 `GPUCullSystem` 实例各触发一次「切片起点未 4 行对齐」，判据 = 该告警行数
>
> **验证（全量 Release，`-j 1` 收敛串行；`-j 4/-j 8` 会撞已知 applocal 竞态）**
> · `BUILD_RC=0` / `error C*|LNK*` = **0** / `warning C*` = **0** / **390 目标**（389 + 新探针）
> · `ctest -L ecs` **9/9**；`TestCSMIncrementalPass` **1/1**；`TestTransformFlatStorage` **rc=0**
> · **C5 探针：告警 2 行**（每实例 1；同实例重复触发不增行）
>
> **反证**
> · **C5（运行期，强）**：把闩打回进程级 static ⇒ 探针**只剩 1 行**；还原（sha256 一致）⇒ **2 行** ✅
> · **C1/C2/C3/C6（源码级，审计自身给的判据）**：`git grep "88 B"` 非 EntityGPU 命中 = 0；
>   `kMaxTransformID` = 0；`culled_by_visibility` 自增点 = 1；`SubmitTransformUpdates` 内
>   手动跳洞点 = 0（`ForEach*` 用点 6 处）
>
> **连带更新的两道源码护栏针**（我改动使它们的钉住文本失效，按新形态同步、意图不变）
> · `TestTransformFlatStorage.cpp`（Test 12）：`static_slot_map` 期望计数 **4 → 6**（C6 后经 slot_map 反查的用点变多）
> · `TestCSMIncrementalPass.cpp`（Test 42）：M4 守卫文本随 C5 改名；**并新增一条禁复活针**
>   （`static bool s_reported_misaligned` 不许回来 —— 进程级闩即缺陷形态）
>
> **本批发现（如实记录，均未擅自改语义）**
> 1. **C6 无运行时覆盖**：把一处 `push_back(*idx)` 改成 `push_back(id)`（槽号↔行号错位）后，
>    `TestTransformFlatStorage` / `TestCSMIncrementalPass` **全绿** ⇒ 上传路径（`SubmitTransformUpdates`
>    过了 `EnsureTransformBuffer` 之后）在无 GPU 设备环境下**根本没被执行**。C6 的等价性目前只有
>    源码级证据（`slot_map[handle] == slot` 的真源在 `RefreshHandleOrder`）。
> 2. **`RefreshHandleOrder` 的两处裸遍历必须保留**：它们是槽位映射的**真源**（要 `slot` 下标写进 map），
>    `ForEach*` 只给 handle ⇒ 那两处**不能**迁移，C6 的"跳洞点归零"是针对 `SubmitTransformUpdates`。
> 3. **C1 的审计判据过宽**：`git grep "88 B" -- inc/hgl/ecs` 不可能为 0 —— `EntityGPU.h` 与
>    `ProbeTransformDiagnostics.cpp` 里的 88 B 是 **EntityGPU 载荷**（另一件事，正确）。真判据应为
>    「`ComponentTypeTable.h` 内无 88 B」。
> 4. **两处 ArenaTrace 共用一个闩**（`RenderPrimitiveCollectSystem.cpp:1454` 与 `:1525`）：前者置位后
>    后者恒假 ⇒ `[ArenaTrace] translated:` 那行**实际从未输出**。本批只把闩改成每实例（行为不变），
>    是否拆成两个闩**留给后续拍板**。
>
> **未做**：S1（系统替换要摘旧实例）/ S2（裸池 `Row()` 不验活跃）；C5 另 5 处的"每实例"证据是源码级
> （探针只覆盖 GPUCullSystem 一节，其余 5 处触发条件需要批装配/设备）。

### 1.4 勿动清单（三方一致维持现状，**勿重开**）

| 项 | 一致结论 | 出处 |
|---|---|---|
| 有序 free list + 弹最大索引 | 是"O(N) 换确定性复用次序"的取舍，不是逻辑矛盾；改 LIFO 属优化且会改复用次序 | C（B v1 曾建议改，已收回） |
| 多容器/多副本本身 | 以**实际失步路径**定性，不因"个数多"就删；收敛对象是具体失步（见 §2-1） | C |
| 9 套系统容器合并 | 属结构性问题，挂旧档 §2.2「六条待拍板」一起定，不单独立批 | A §2.2 + C |
| `PrimitiveSortSystem` | 保留（形态 B 的正常结果） | A chunk 4 拍板 + C + B v2 |
| `TransformDataStorage::Clear()` 零调用者 | 现状保留（它是 §2-9 / B3 可能要用到的入口，先别删） | B 新发现，无人反对 |
| 材质四条键空间 | 不合并表，只共用前缀编解码 | A §2.2 报告 07 拍板 + B |

---

## 2. 确认问题，但三方处理方式不一致

> 每条：现象（三方共识）→ 各方做法 → **推荐** → 影响。**这些项不进批 1**，等拍板后再排批。

| # | 现象（共识） | 各方做法 | 推荐 | 影响 |
|---|---|---|---|---|
| 1 | `Entity::component_mask` 是 `component_rows` 的缓存视图，**生产期无一致性门**（对拍器只在 `TestComponentMask`） | A/B(旧账本)：①debug-only 对拍 ②删掩码改读 `component_rows`；C 未裁决 | ① 先做（成本低、零行为变化） | 改 ② 会让 `HasComponentType`/策略判定从 O(1) 变 O(16)，须连 `GetComponentMask()` 的 32 位汇总一起设计 |
| 2 | `RenderItemDataStorage::Release` / `ReleaseContiguous` 无"已释放"判定 ⇒ **同 handle 被两次分配（槽位别名）** | A 说"活跃计数下溢"；B v1 说"虚高"、方案是用零 descriptor 判已释放；C 说"少算 + 槽位别名"且**否决零 descriptor 方案**（分配路径合法产出全零载荷） | **采纳 C 的判定口径（少算/别名）+ C 的方案（显式分配状态：账目/位图）** | 不可回滚：二次释放造成的别名会画错东西，属正确性；建议与 B3 同批（都是"账目非原子"） |
| 3 | `RenderPrimitiveCollectSystem.cpp` 内 9 处 `IsCurrentPassShadow`（`:800,1027,1318,2118,2152,2372,2391,2421,2425`），策略表宣称"唯一判据" | A：当作"注释与代码矛盾"的缺陷要收敛；B v2 / C：分类为 **1 事实注入 + 6 程序选择/重试/状态 + 2 诊断**，不是策略表第二实现，只需**写清分工** | **只做头注**（写进 `RenderPrimitiveCollectSystem.h`），不动 9 处 | 若按 A 收敛会改动 pass 分流与 D9 重试链，风险远大于收益 |
| 4 | `SyncRenderTargetViewport()` 名字不表实情（只比分辨率并 `OnResize`） | A：列为"名不符实"（中）；B：行为**正确**（`GetViewport()` 返回分辨率，旧账本 §7 已核实），只是名松 | **改名或加注释**，不改行为 | 无行为风险 |
| 5 | `AddFreeIndex` 用 `lower_bound` 维护有序却按栈消费 | A/B v1：改 O(1) 尾部追加；C：现状是取舍，改可以但要说明复用次序变化 | 记为**优化项**（挂 backlog），不急 | 影响的是空闲序号复用次序（`TestTransformFlatStorage` Test 13 钉槽位契约） |
| 6 | `TransformAccessor::SetFixedPixelSizingContext()` 名字像设上下文，实际**即时改实体 SCALE**（`TransformAccessor.cpp:399-420`） | 只有 A 列该条（中），无人给做法 | 二选一：**改名**暴露副作用 / **拆**成"设上下文"与"Apply"两口 | 调用点集中在 gizmo 路径，改动面小 |
| 7 | 整改顺序 | A 的 §7 十一项（删除在第二阶段）；B/C：**正确性批先行**，删除/命名/结构分账 | **本档 §0.6 的顺序**（1.1 → 1.2 → 1.3） | 决定首个提交内容 |
| 8 | `PrimitiveSortSystem` 的**文档口径** | A 的路线图第 8 项写"移除并注销或补齐"；chunk 4 拍板"保留"；B v2/C：保留 | 只订正 A 的第 8 项措辞 | 文档一致性；顺带记 backlog：删它可省每帧一次无守卫 `PrepareFrame`（`PrimitiveRenderPipeline.cpp:29-46`） |
| 9 | `Shutdown`/重置契约（`Context.cpp:264-291` inactive 早退、`:317-326` active）：清实体不清变换行存储 ⇒ 新实体继承旧变换行；`Clear()` 重置世代 ⇒ 旧句柄复活 | C 给两选一：(a) 完整复位 (b) 复用则拒绝；A 未涉及 | **(a) 完整复位 + 世代不复位**（并给 `TransformDataStorage::Clear()` 补上调用点或明确其归属） | 决定"世界可否重用"的对外语义；与 B3 同族（账目/世代） |
| 10 | CMake 源列表重复登记 `RenderSystemCore.{h,cpp}`（`CMakeLists.txt:24,44` 与 `:47-49`） | A/B v1 当作"实际重复"；C 实测 `ULRE.ECS.vcxproj` 只 1 条 `<ClCompile>`；B v2 复核同 | 只做**登记去重**（清理项，低优先） | 无行为影响 |
| 11 | 局部 static 之外，`PrimitiveRenderPipeline::RunCull/RunSort/RunBuild` **各自**再调一次 `impl_->PrepareFrame`（无 once-per-frame 守卫） | A §3.5 记守卫缺失、B 补"删 Sort 系统可省一次"、C 未涉及 | 与 `PrepareFrame` 守卫同批处理 | 属性能面，非正确性 |

---

> ### ✅ §2 执行记录（2026-10-11）—— 八项落地（#1/#2/#3/#4/#6/#9/#10/#11），四组反证精确变红
>
> **代码 / 注释**
> · **#1** `src/ecs/core/Entity.cpp`：`SetComponentRow` / `DetachAllComponents` 加 **debug-only 掩码对拍**
>   （`#ifndef NDEBUG`：掩码写坏即 `GLogError`；Release 被 NDEBUG 剥掉 ⇒ 零成本、零行为变化）。
>   验证：`cmake --build build --config Debug --target ULRE.ECS` **rc=0**（该块真能编进去）。
> · **#2** `RenderItemDataStorage.h/.cpp`：新增**显式分配状态** `allocated[]`（+ `IsAllocated()`）——
>   否决"零 descriptor 判已释放"（`Allocate(RenderItemDescriptor{})` 会合法产出全零载荷）；
>   `Release` 重复释放 ⇒ false/no-op，`ReleaseContiguous` 只释放已分配槽且全空时整体 no-op，`Clear` 同步清。
> · **#3** `RenderPrimitiveCollectSystem.h`：**只做头注** —— 9 处 `IsCurrentPassShadow` 的分工
>   （1 事实注入 + 6 程序选择/重试/状态 + 2 诊断）写进头文件；不动实现（不收敛）。
> · **#4** 改名 `SyncRenderTargetViewport` → **`SyncRenderTargetViewportToExtent`**（3 处）+ 注释说明
>   它做的是"把目标的视口同步到该目标当前 extent"，不是 patch viewport；行为不变。
> · **#6** 改名 `SetFixedPixelSizingContext` → **`SetFixedPixelSizingContextAndApply`**（3 处：声明/定义/
>   gizmo 调用点）+ 既有注释同步；暴露"设上下文即刻改 `LocalScale`"的副作用（取"改名"选项，未拆两口）。
> · **#9** 世界复位契约：`TransformDataStorage` 新增**单调世代计数器** `next_row_generation`
>   （`Allocate` 用它发放；`Clear()` 不复位）⇒ 复位+重分配后旧句柄**不复活**；
>   `ECSContext::Shutdown` 两条路径都改为**复位变换行存储**（`TransformDataStorage::Clear()` 由此获得调用点，
>   不再是零调用者），消除"清实体不清行存储 ⇒ 新实体继承旧变换行"。
> · **#10** `src/ecs/CMakeLists.txt`：源列表里 `RenderSystemCore.{h,cpp}` 的**重复登记**删除
>   （保留 `ECS_SYS_00_RenderSystemCore_FILES` 变量，它同时挂着 `source_group`）。
> · **#11** `PrimitiveBatchPipeline`：`PrepareFrame` 拆为**真守卫**（帧号 + 结果缓存，早退返回上次结果）
>   + `PrepareFrameInternal`；新增 `GetPrepareFrameCount()` 判据口。与 2026-10-10 删掉的
>   `prepared_frame_index`（只有读一次再写回、无 early return = **假守卫**）明确区分。
>
> **判据用例（新增/扩展）**
> · `TestRenderItemDataStorage` **Stage 6（Test 63）**：重复释放 no-op + **回收两次分配不得返回同一 handle**
>   （原实现就是槽位别名）+ 连续区间重复释放整体 no-op。
> · `TestTransformFlatStorage` **Test 19**：复位前取得的访问器在"复位 + 重分配"后必须失效（世代不复位）；
>   `Shutdown` 后行存储 rows == 0。
> · 新目标 **`TestFramePrepareOncePerFrame`**：同帧 3 次 `PrepareFrame` ⇒ 准备 **1** 次；帧号推进 ⇒ 2 次；
>   null 入参不计数。（`LABELS "ecs;render"`）
>
> **验证（全量 Release，`-j 1` 串行）**
> · `BUILD_RC=0` / `error C*|LNK*` = **0** / 环境竞态 = **0** / `warning C*` = **0** / **392 目标**
> · `ctest -L ecs` **11/11**（新增 `TestFramePrepareOncePerFrame`）；`TestCSMIncrementalPass` **1/1**；
>   `TestTransformFlatStorage` / `TestRenderItemDataStorage` / `TestFramePrepareOncePerFrame` /
>   `TestSystemReplaceAndPoolLiveness` / `TestComponentMask` / `TestBoundingBoxStorage` 全 **rc=0**
>
> **反证（四组，各自精确变红 + sha256 还原复绿）**
> · **#2** 去掉 `IsAllocated` 判 ⇒ Test 63 rc=**63**：`重复释放返回了成功（应 no-op）`
> · **#9a** 世代发放回退成常量 1 ⇒ Test 19 rc=**21**：`复位后新行的世代与旧行相同（gen=1）⇒ 世代计数器被复位了`
> · **#9b** 去掉 `Shutdown` 里的行存储复位 ⇒ Test 19 rc=**21**：`Shutdown 后变换行存储未复位（rows=1）⇒ 新实体继承旧变换行`
> · **#11** 去掉每帧守卫 ⇒ Test 1 rc=**1**：`同帧 3 次 PrepareFrame 实际准备 3 次（期望 1 —— 缺每帧守卫）`
>
> **过程记录（一条值得进纪律的坑）**：本批一次性改了两个**公共头**（`TransformDataStorage.h` 加成员、
> `RenderItemDataStorage.h` 加成员）后只做增量构建 ⇒ `TestCSMIncrementalPass` 在 Test 9 附近 **SEGV**（rc=139）。
> 按既有纪律用 `purge-stale-deps.sh` 清 **20 个 include 者 / 30 obj** 后**同一份源码** rc=0（48 项全过）⇒
> 定性为**陈旧 TU 布局错位**（`RenderItemDataStorage` 里加了成员，只有部分 TU 用新头编），不是回归。
> 教训固定为：**改公共头成员集合后，先 purge 全部 include 者再构建**，别先怀疑逻辑。
>
> **文档面（同批）**：A 档 §7 第 8 项（`PrimitiveSortSystem`）措辞订正为"**保留**"，并注明
> "删它可省一次 `PrepareFrame`"的收益已因 #11 加真守卫而消失。
>
> **§2 剩余（未做，均属 backlog/低优先）**
> · **#5** `EntityManager::AddFreeIndex` 的 `lower_bound` 有序维护 vs 栈消费：记为**优化项**（挂 backlog，不急；
>   它会改变空闲序号复用次序，而 `TestTransformFlatStorage` Test 13 正钉着槽位契约）。
> · **#7** 整改顺序：已按 §0.6 执行（1.1 → 1.2 → 1.3 → §1 补充 → §2），无须另动。
> · **#2 的"与 B3 同批"建议**：B3 已在批 1.1 落地，本批按同一口径（账目非原子 ⇒ 显式状态）收口完成。
>
> **越界项（如实记录）**：全量 `ctest`（181 项）仍有 2 项失败 —— `OrderedMapBasicOperations` /
> `ObjectManagerTest`（CMCore 独立仓示例，SEGV，`#include` 无任何 ECS 头，且 `OrderedMapBasicOperations`
> 崩前自打印 `Added 3 items, map size: 0`）⇒ 与本批无关，留待 CMCore 侧处理。

## 3. 三方争议（含 B 自己 v1→v2 的翻转）

> 每条：争议点 → A / B / C 三方主张 → **我的实测裁决** → 对执行的影响。

| # | 争议点 | A 主张 | B 主张 | C 主张 | 实测裁决（依据） | 对执行的影响 |
|---|---|---|---|---|---|---|
| 1 | `TransformAccessor::MarkDirty()` 的性质与严重度 | 高（"标的是永不上传的活"） | v1：✅真、升格准死 API | 两方都说重了：动态段每帧全量写 | **两方各半**：静态行按 `last_uploaded_version` 上传（`TransformSystem.cpp:281-297`）⇒ 不 bump version 的 `MarkDirty`（`:529-533`）**静态行确实不上传**；动态行 `dynamic_force_full`（`:254,:305-309`）每帧全量写 ⇒ 动态行照样上传。唯一调用点 `ClockUse.cpp:350` 是**可动**手且上一行 `SetLocalRotation` 已 bump ⇒ C 对 | 归入 §1.2 A2 同级"语义陷阱"，**不占高优先级**；可选三选一（删公开 API / 让它等于 `MarkLocalChanged` / 改名）。**→ 已拍板走「删公开 API」并落地（2026-10-11）：**声明 + 定义 + 唯一调用点（`example/Transform/ClockUse.cpp`）三处删除，零行为变化；连带订正 2 处注释与 2 处文档。判据：访问器族 `MarkDirty` 全仓归零；反证：只恢复调用点 ⇒ `ClockUse` **LNK2019**（证明该调用点是唯一消费者、删除是承重的） |
| 2 | 相机组件"双真值" | 高 | v1：✅真 | 缺因果证据，属 API 收敛 | **采纳 C**：`GetCameraByEntity`（`Context.h:566-569`）经组件池，槽认领只在 `BindCameraResources`/`EnsureCameraSlot`（`CameraSystem.cpp:774-775`） | 降级为 §2-6 同类（API 收敛），不单独排批。**→ 已落地（2026-10-11）：**相机类型的 `Entity::GetComponent<T>`/`AddComponent<T>` **编译期禁死**（`static_assert` 指名世界级入口）；四条 `GetCameraByEntity` + 两条 `GetOrCreateCamera` 改走"行号→池行"直通。判据：唯一实体级实例化（测试里那处）已迁到 `GetOrCreateCamera`；反证 = 写一行 `e->GetComponent<CameraComponent>()` ⇒ **error C2338** 静态断言 |
| 3 | `RenderGraph` 抬高 pass Update 下界 | 判"名不符实"（漏执行） | v1：机制真、载体符号已删 | 是**有意为之**：`PrepareRenderPassSetup` 已跑前置相位 | **采纳 C**：`ContextRender.cpp:846-861` 先跑 Collect/Batch/Commit/Upload/FrameSync，`RenderGraph.cpp:107-112` 自 `RenderDrawSubmit` 起是避免重跑 | 只剩**文档语义**一项（写进头注即可），不排代码批。**→ 已落地（2026-10-11）：**`RenderGraph.h` 的 `Pass::startPhase` 头注写入契约口径（「pass 区间中 < `RenderDrawSubmit` 的部分由帧首前置段执行」），`RenderGraph.cpp` 截断处就地注释同一理由 |
| 4 | `GPUCullSystem::GetGPUVisibleCount()` 缺 fence | 高（"会读到残影"） | v1 起即降级（调用点都已 fence） | 同意降级，保留接口纪律 | **合流**：`GPUCullSystem.cpp:866-878` 无自守卫；两个调用点分别位于 `:758-763` 的 fence 内与 `CPUCullReferenceSystem.cpp:416` 的 `SyncForParityReadback()` 之后 | 记为**接口纪律**（补注释/加守卫皆可），不进批 1。**→ 已落地（2026-10-11）：**声明处补前置条件与两个调用点的满足方式；并加**自守卫**（fence 路径记账 `readback_ready_frame_` + `SyncForParityReadback` 的 `counters_sync_frame_` 二者取或；都不满足 ⇒ 一次性报错 + **返回 0 拒绝残影**） |
| 5 | `LineBuffer::AddSegment` 无回滚的定级 | P0（第一阶段） | v1："路径不可达" | 缺回滚属实，但**收回绝对表述** | **各让一步**：预分配 + 前置有效性 + 每帧 `Reset` 使常见路径不可达，但无法证明无其它入口 ⇒ P3"防御性回滚" | 进批 1.3 类（可选），不进最先批。**→ 已落地（2026-10-11，P3 防御性回滚）：**`AddSegment` 四个写入流改为**同进同退**（`&` 非短路），任一失败一律 `Seek` 回退到 `line_count * 2`，本次线段整体丢弃 + 周期性日志；成功路径有运行时证据（`LineRender.exe` 稳态运行） |
| 6 | `CameraSystem::GetActiveCameraComponent()` 隐式认领 | 高 | v1：✅真 | 源码已定义为惰性解析，属命名/拆分 | **合流**：行为属实（`CameraSystem.cpp:686-746`），定级降为 API 收敛 | 与 §2-6 同批（拆只读/认领两口）。**→ 已落地（2026-10-11，§2-6 那条已按"改名"收口，故本条独立成批）：**`GetActiveCameraComponent()` 改名 **`EnsureActiveCamera()`**（认领版）+ 新增纯读 **`ResolveActiveCamera()`** / `ECSContext::ResolveActiveCameraInfo()`；两口共用同一套解析（`SelectMainCameraPure`），gizmo ×2 + 拾取示例迁到纯读口。判据：新用例 `TestCameraResolveAndClaim`；反证 = 纯读退回认领版 ⇒ 用例 rc=1 |
| 7 | "多容器 = 多权威" | 倾向冗余收敛 | v1 同 A | 以**实际失步路径**定性 | **采纳 C**；唯一实证失步 = 系统替换未摘旧实例（`ContextSystems.cpp:155-177`：`*it == 系统新实例` 比较 ⇒ 旧实例留在元素索引，组开关仍能关它） | 该缺陷本身**无争议**，可做（见 §1 补充项 S1） |

> ### ✅ §3-1 执行记录（2026-10-11）—— 按选项① 删除 `TransformAccessor::MarkDirty()`，全绿 + 承重反证
>
> **改动（4 处代码/示例 + 2 处文档）**
> · `inc/hgl/ecs/support/TransformAccessor.h`：删声明；原处留一行"已删除 + 为什么"的注释，
>   并指引正确入口（`SetLocal*` 一族内部走 `MarkLocalChanged`）。
> · `src/ecs/support/TransformAccessor.cpp`：删定义；同文件那条历史注释里复述的旧符号一并改写
>   （仓库纪律：注释不复述被删声明原文）。
> · `example/Transform/ClockUse.cpp`：删唯一调用点（三根指针的 `MarkDirty()` 循环）——
>   三针是 `Mobility::Movable` 且上一行 `SetLocalRotation` 已 bump 版本 ⇒ **零行为变化**。
> · 文档面：`doc/ecs-quality-audit-2026-10.md`（旧账本该行补"已处置"）、
>   `doc/ecs/transform-data-management-and-transfer.md`（描述该示例行为的行同步订正）。
>
> **判据（"红的样子"）**
> · 访问器族符号全仓归零：`git grep -n "TransformAccessor::MarkDirty\|transform\.MarkDirty" -- inc src example`
>   ⇒ **0**（只剩"已删除"说明性注释；`DirtyRowSet`/`EntityGPUTable`/`RenderItemDataStorage`/`IGPUBuffer`
>   的同名函数是**别的类**，不受影响）。
> · **承重反证**：只把调用点恢复（API 保持删除）⇒ `ClockUse` 构建 **error C2039**
>   `'MarkDirty': is not a member of 'hgl::ecs::TransformAccessor'`（比 LNK2019 更强：编译期即证明
>   该 API 已无任何声明、该调用点是唯一消费者）；还原（sha256 一致）⇒ rc=0。
> · **示例实跑**：`ClockUse.exe` 运行 20s 仍在正常出帧（rc=124 = 我设的上限截断；日志
>   `[PipelineResolver] materialize(ok/fail)=1/0` 无错）⇒ 删除调用点后时钟示例行为不变。
>
> **验证（全量 Release，`-j 1` 串行）**
> · `BUILD_RC=0` / `error C*|LNK*` = **0** / 环境竞态 = **0** / `warning C*` = **0** / **392 目标**
> · `ctest -L ecs` **11/11**；`TestCSMIncrementalPass` **1/1**；清 `TransformAccessor.h` 的
>   **66 个依赖 TU / 113 obj** 后重编再验（防陈旧 TU 结论）。
>
> **为什么选①而不是②/③**：唯一调用点是冗余、删除零行为变化；而"只重算 CPU 矩阵"的口子目前**无真实使用者**
> （内部走 `UpdateIfDirty()`）。若将来确有该需求，应按③以**表实**的名字（如 `MarkWorldMatrixRecompute()`）
> 重新引入，**不要**恢复这个名字 —— 它的名字承诺"刷新"、实际只保证 CPU 侧重算，且批 1.1 B4 之后对静态行
> 还会产生"阴影缓存失效、矩阵仍旧"的**半效**状态，正是容易误导后来者的形态。

> ### ✅ §3-②/⑥ 执行记录（2026-10-11）—— 相机纯读/认领拆两口 + 实体级模板编译期禁死，全绿 + 双反证
>
> **⑥ 拆两口（纯读 / 认领）**
> · `CameraSystem::GetActiveCameraComponent()` → **改名 `EnsureActiveCamera()`**（名字暴露"会认领 0 号槽 /
>   必要时生成常驻 fallback"）。旧名名不符实：名字像 Getter、实际写世界。
> · 新增 **纯读** `CameraSystem::ResolveActiveCamera() const` / `ResolveActiveCameraInfo() const`
>   + 世界级 `ECSContext::ResolveActiveCameraInfo() const`（`Context.h`/`Context.cpp`）：
>   **不认领 0 号槽、不生成 fallback、不改世界状态**；解析不到 ⇒ nullptr。
> · 实现上把 `SelectMainCamera`（三级解析 + 认领）拆为 **`SelectMainCameraPure()`**（只选择，const）
>   与认领包装 `SelectMainCamera()`（补 ① 已是宿主 / ②③ 升格 / ④ 常驻 fallback）⇒ **两口共用同一套解析顺序**，
>   不会再造出"第二套解析"。`CollectCameras()` 加 `const`。
> · 只读调用点迁移到纯读口（3 处）：`SunDirectionControlSystem.cpp:185`、`TransformGizmoSystem.cpp:194`、
>   `example/Interaction/RayPicking.cpp:324`（三处本来就已经 `nullptr` 提前返回 ⇒ 世界无相机时行为天然正确）。
>   渲染侧（`ContextRender.cpp:501`、`LineRenderPipeline.cpp:416`）继续走认领版 `GetActiveCameraInfo()`。
>
> **② 实体级模板编译期禁死**
> · `Entity::GetComponent<T>()` / `Entity::AddComponent<T>()`（模板体在 `Context.h`）各加一条
>   `static_assert(!std::is_same_v<T, CameraComponent>, …)`，文案直接指名正确入口
>   （`ECSContext::GetCameraByEntity` / `GetOrCreateCamera`）。
> · 连带：`GetCameraByEntity` 四条重载 + `GetOrCreateCamera` 两条**改走"行号 → 池行"直通**
>   （`Entity::GetComponentRow` + `ECSContext::GetComponentByRow`）——因为它们原先经 `ComponentOfEntity<T>` /
>   `GetOrAddComponentOfEntity<T>`，那两个 helper 内部正是实例化被禁模板的地方（不改就会打到自己的实现上）。
> · 仓内唯一实体级实例化（`TestCSMIncrementalPass.cpp:1240` 的 `AddComponent<CameraComponent>`）已迁到
>   `GetOrCreateCamera`（A7c 的目录扫描针此前只扫 `example/`，测试侧是漏网的那一处）。
> · 注：`HasComponent<CameraComponent>` / `RemoveComponent<CameraComponent>` **未禁**（只是查/卸行号，不构成第二个读取/创建入口）。
>
> **判据用例**：新目标 **`TestCameraResolveAndClaim`**（`LABELS "ecs;camera"`）
> · 段 1 空世界：纯读 ⇒ nullptr **且世界不被改动**（默认相机空 / 相机数 0）；认领版 ⇒ 生成常驻 fallback 并占 0 号槽
> · 段 2 有相机（都没标主相机）：纯读 ⇒ 选出最小 EntityID 者但**不认领**（默认相机仍空）；认领版 ⇒ 升格 0 号槽
>
> **验证（全量 Release，`-j 1` 串行）**
> · `BUILD_RC=0` / `error C*|LNK*` = **0** / 环境竞态 = **0** / `warning C*` = **0** / **393 目标**
> · `ctest -L ecs` **12/12**（新增相机用例）；`TestCSMIncrementalPass` **1/1**；`TransformFlatStorage` /
>   `RenderItemDataStorage` / `SystemReplaceAndPoolLiveness` / `FramePrepareOncePerFrame` 全 rc=0
> · 改动落在 `Context.h`/`CameraSystem.h` 这类公共头上 ⇒ 先 purge **168 个依赖 TU / 278 obj** 再重编
>
> **反证（两组）**
> · **⑥（运行期）**：让 `ResolveActiveCamera()` 退回认领版 ⇒ 新用例 rc=**1**：
>   `空世界的**纯读**不该解析出相机（应 nullptr）`；还原（sha256 一致）⇒ rc=0
> · **②（编译期）**：在用例里写一行 `e1->GetComponent<CameraComponent>()` ⇒ **error C2338:**
>   `static assertion failed: '相机不能经 Entity::GetComponent 读取：走世界级入口 ECSContext::GetCameraByEntity'`；
>   还原 ⇒ rc=0
>
> **过程中两处必须记下的实情**
> 1. **纯读的 ④ 级语义由用例逼出来**：第一版纯读在"0 号槽是常驻 fallback（不经实体注册、`CollectCameras()`
>    看不到它）"时返回 nullptr（保留原 ① 的"默认相机必须在实体集合里"门），用例段 1 因此变红 —— 这暴露了
>    "问谁是当前生效相机却答 nullptr"的契约漏洞。修法：纯读在没有实体相机时返回 `GetDefaultCamera()`
>    （0 号槽宿主 = 权威），认领版的行为**逐字不变**（④ 仍是"没有就生成 fallback"）。
> 2. **测试 28 的"指针版直通核心"针**钉的是旧实现文本（`return ComponentOfEntity<CameraComponent>(owner);` ×2
>    与 `GetOrAddComponentOfEntity<CameraComponent>(owner, name)` ×1）⇒ 随②改名同步为新形态
>    （`owner->GetComponentRow(ComponentTypeOf<CameraComponent>::value)` ×2 + 判存在/世界侧创建各 1），
>    **意图逐字保留**（指针版不做 ID→Entity 往返）。

> ### ✅ §3-③④⑤ 执行记录（2026-10-11）—— 三条收尾（头注 / 接口纪律+自守卫 / P3 防御性回滚），§3 全案结清
>
> **③ `RenderGraph` 抬高 pass 更新下界 ⇒ 只做文档语义（审计指定的口径）**
> · `inc/hgl/ecs/core/RenderGraph.h`：`Pass::startPhase` 头注写入契约 ——
>   `ExecuteRenderGraphPasses` 从 `max(startPhase, RenderDrawSubmit)` 起跑**不是漏执行**，而是**避免重跑**：
>   帧首 `BeginManagedRenderFrame`（`ContextRender.cpp:838-862`）已按序跑过
>   `SubmitTransformUpdates → PublishCameraRows → RenderCollect → SyncWorldAddresses → RenderBatch →
>   RenderBufferCommit → RenderBufferUpload → RenderFrameSync`；契约 = 「pass 区间中 < `RenderDrawSubmit`
>   的部分由帧首前置段执行」。
> · `src/ecs/core/RenderGraph.cpp`：截断处就地注释同一理由（指向头注）。
>
> **④ `GetGPUVisibleCount()` 缺 fence ⇒ 接口纪律 + 自守卫**
> · `GPUCullSystem.h`：声明处补"前置条件 + 两个调用点如何满足"（对拍台架先 `SyncForParityReadback()`
>   `CPUCullReferenceSystem.cpp:419`；SSBO dump 在 `WaitLastSubmitFence()` 之后 `GPUCullSystem.cpp:757-758`）。
> · 新增自守卫（与同文件 `MapCurrentBatchCounters` 的 §9 M6 守卫同纪律）：fence 路径补一行记账
>   `readback_ready_frame_`，判据 = `counters_sync_frame_ != frame && readback_ready_frame_ != frame`
>   ⇒ 一次性报错（每实例闩，非进程级）+ **返回 0 拒绝返回残影**。
>   为什么不用单一 `counters_sync_frame_` 做判据：它只由 `SyncForParityReadback()` 记账，fence 路径不经过它 ⇒ 会误报。
> · 无行为影响面核查：`GetGPUVisibleCount` 全仓仅 2 个调用点，均已记账；对拍台架默认关（`ULRE_ENABLE_CPU_CULL_PARITY_TEST`）。
>
> **⑤ `LineBuffer::AddSegment` 无回滚 ⇒ P3 防御性回滚**
> · `src/ecs/support/line/LineRenderPipeline.cpp`：四个写入流（pos/color/transform/width）改为**同进同退** ——
>   一次 `&` 串起 8 次 `Write`（**非短路** ⇒ 四个流都被尝试，失败面判定完整），任一失败即
>   `Seek(line_count * 2)` 回退四个流（回退点 = 上次成功追加后的锁步游标）+ 周期性日志说明
>   「本次线段整体丢弃」；不再留下"位置流步进 2、其余流未动"的错位。
>   回退的可行性依据：`RawDataAccess<T>::Seek(offset)` 在 `offset < count` 时合法（`TypedArrayView.h:65`）。
>
> **验证（全量 Release）**
> · `BUILD_RC=0` / `error C*|LNK*` = **0** / `warning C*` = **0** / **393 目标**；改动落在
>   `GPUCullSystem.h`/`RenderGraph.h` ⇒ 先 purge 15 个依赖 TU 再重编
> · `ctest -L ecs` **12/12**；`TestCSMIncrementalPass` **1/1**
> · **⑤ 运行期冒烟**：`LineRender.exe` 稳态跑满 20s（rc=124 = 我的上限截断）：日志里
>   **0 条** `AddSegment 写入失败`，且每帧 `[LineCollectSystem] Collect summary: total=4 visible=4 …`
>   ⇒ 重写后的成功路径正常工作
>
> **判据的限度（如实标注）**
> · ③ = **文档级**（审计自身指定"写进头注即可"）：判据 = 头注/注释在位 + 全量构建与用例不回归。
> · ④ = **代码级 + 调用点核查**：运行时验证需要 GPU 设备（无 `draw_count_buffer_` 时函数提前返回，
>   自守卫分支在无设备环境不可达）；已核查唯一两个调用点都满足前置条件。
> · ⑤ = **成功路径有运行时证据**（上面 `LineRender.exe`）；**失败路径（容量满 / 访问器失效）不可在无设备环境构造**
>   ⇒ 回滚分支属防御性、判据为源码级（四流同进同退 + 回退点 = `line_count * 2`）。
>
> **§3 全案状态**：① 删除 `MarkDirty`（已落地）· ② 相机实体级模板编译期禁死（已落地）·
> ③ 头注（本批）· ④ 接口纪律 + 自守卫（本批）· ⑤ P3 防御性回滚（本批）· ⑥ 相机纯读/认领拆两口（已落地）·
> ⑦「多容器=多权威」随 §1-S1 落地。⇒ **§3 七条全部结案**；下一步进 §4（覆盖缺口与文档面回扫清单）。

**§3 的附带无争议项（C 发现、B 代码级复核成立、A 未涉及，故归"无争议"）**——建议并入 1.1 或紧随其后：

| # | 内容 | 判据 |
|---|---|---|
| S1 | 系统替换要摘旧实例：`ContextSystems.cpp:155-177` 用 `*it == system` 比较，替换后旧实例仍留在 `systems_by_element_type`（`SetSystemGroupEnabled` 从该列表重建快照，`:261-285`） | 注册两次同类型后列表只留新实例；组开关不影响旧实例（反证：现状可复现 C 的现象） |
| S2 | 裸池 `Row()` 不验活跃：`ComponentPool.h:235-246` 只判 `handle==0` 与 `next_index` ⇒ `DestroyRow` 后仍返回非空指针（与 `GetComponentByRow` 的"已释放返回 nullptr"承诺相悖）；`DestroyRowAt`（`:209-220`）在 `row->~T()` **之后** `SetOwner(nullptr)` | `DestroyRow` 后 `Row(h)==nullptr`；`SetOwner` 移到析构之前；两个池语义对齐 |

---

> ### ✅ §1 补充项 S1 / S2 执行记录（2026-10-11）—— 系统替换摘旧实例 + 裸池行存活校验，全绿 + 三组反证
>
> **改动（3 文件）**
> · **S1** `src/ecs/core/ContextSystems.cpp`：`AddOrUpdateSystem` 里 element-type 列表的摘除从
>   「按实例指针比较（`*it == system`）」改为「**按键/类型**摘（`remove_if` + `typeid(*candidate) == typeid(*system)`）」，
>   并补 `<typeinfo>`。原写法在 `RegisterSystem<T>()` 再注册（替换同类型系统）时**没有任何元素等于新实例**
>   ⇒ 旧实例永远留在列表里；`SetSystemGroupEnabled` 从该列表重建快照 ⇒ 快照同时挂着新旧两个实例，
>   组开关会去开关一个"已不在 `sys_map` 里"的僵尸实例；`RenderGraph.cpp:45` 取该列表算组的相位区间时
>   也会把僵尸算进去（这才是有实际后果的一侧）。
> · **S2** `inc/hgl/ecs/support/ComponentPool.h`：
>   ① `ComponentPool<T>::Row()` 增加**活跃校验**（`!ledger.IsActive(index) ⇒ nullptr`）——
>      原实现只判上界，`DestroyRow` 之后仍返回指向**已析构行**的非空指针（悬垂），
>      与 `SharedComponentPool<T>::Row`（reset 后自然 nullptr）语义不一致；
>   ② `DestroyRowAt(index)` 顺序改为**先摘宿主再析构**（`SetOwner(nullptr)` 移到 `row->~T()` 之前）——
>      原顺序是对生命周期已结束的对象写基类子对象（UB，只因行头是 POD 且内存未归还才"没炸"）。
>
> **新增判据用例**：`src/ecs/support/TestSystemReplaceAndPoolLiveness.cpp`
> （+ `src/ecs/CMakeLists.txt` 目标与 `add_test`，`LABELS "ecs;registry"`、`TIMEOUT 60`）
> · 段 1（S1）：`RegisterSystem<LineCollectSystem>()` 两次 ⇒ 列表只留新实例（size==1 且是新实例）；
>   `SetSystemGroupEnabled(Line,false)` 必须关到新实例、**且不能碰旧实例**
> · 段 2（S2-a）：裸池与 shared 池均「`DestroyRow` 后 `Row(h)==nullptr`」+ 重复释放 no-op + 活跃计数为 0
> · 段 3（S2-b）：源码针钉 `DestroyRowAt` 的顺序（`SetOwner` 的位置必须早于 `~T()`）
>
> **验证（全量 Release，`-j 1` 串行）**
> · `BUILD_RC=0` / `error C*|LNK*` = **0** / `MSB3073|MSB3075`（环境竞态）= **0** / `warning C*` = **0** / **391 目标**
> · `ctest -L ecs` **10/10**（新增用例后 9 → 10）；`TestCSMIncrementalPass` **1/1**；
>   `TestSystemReplaceAndPoolLiveness` **rc=0**（三段全过）
>
> **反证（三组，各自精确变红 + sha256 还原复绿）**
> · **S1** 打回「按实例比较」⇒ 用例 rc=**1**：`替换后 element-type 列表仍有 2 个实例（应为 1 —— 旧实例没被摘掉…）`
> · **S2-a** 去掉活跃校验 ⇒ 用例 rc=**2**：`DestroyRow 后 Row(1) 仍返回非空（指向已析构行的悬垂指针）`
> · **S2-b** 析构顺序打回 ⇒ 用例 rc=**3**：`DestroyRowAt 必须**先摘宿主再析构**（SetOwner 位置=9558 / ~T() 位置=9511）`
> · 头文件改动点（`ComponentPool.h`）每次破坏都先清依赖 TU 的 obj 再编（否则 MSBuild 复用旧头 ⇒ 假绿）
>
> **如实记录（越界项）**：全量 `ctest`（181 项）里 **2 项失败**——`OrderedMapBasicOperations` 与
> `ObjectManagerTest`（均 SEGV）。两者属 **CMCore 独立仓**的示例、`#include` 里没有任何 ECS 头、
> 且 `OrderedMapBasicOperations` 自身在崩溃前就打印 `Added 3 items, map size: 0`（容器在该仓当前状态下即为坏的）；
> 把 vcpkg bin 加进 `PATH` 后仍同样崩 ⇒ **与本批改动无交集**，非本次引入，留待 CMCore 侧处理。
>
> **§1 收尾状态**：1.1（4 项）/ 1.2（4 项）/ 1.3（6 项）/ S1 / S2 **全部落地**；下一步进 §2 不一致项。

## 4. 其它

### 4.1 覆盖缺口（不算异议，但要记账）
- **A 未覆盖 C 的 4 项 P1 与 4 项 P2**（CSR 搬迁、父环、段预算原子性、revision 信号链、系统替换、裸池查行、`Clear` 世代、Shutdown 半复位）⇒ A 若继续迭代审计，这是最该补的面。
- **A 未覆盖**旧账本 §4.1（台架/测试代码住生产目录：11 个 `main()`、~10.8k 行）与 §6（文档-代码脱钩横切）。
- **C 未覆盖** B 的静默项：`TransformDataStorage::Clear()` 零调用者、6 处进程级 static 闩、CMake 登记重复、`LinesComponent::version`。

### 4.2 文档面修正（本批已就地落地）
| 文件 | 状态 |
|---|---|
| `ecs-deep-quality-audit-and-evidence-2026-10.md`（A） | 已插「⚠ 交叉评审订正」块共 7 条（3 个已删符号、3 处计数错、`RenderItemHandle 0` 判错、9 处分类订正、`cl /Bv` 措辞） |
| `ecs-quality-audit-2026-10.md`（旧账本） | 已插「交叉评审入口」块 4 条（含 `const 版才是活的` 判断反转、`RenderItemHandle 0` 判错） |
| `ecs-deep-audit-second-crossreview-2026-10-11.md`（C） | 已插「综合结论指针」块（复核结论 + 3 处待订正：`MarkSynced` 措辞、探针入库、测试数口径 9+1） |
| `ecs-deep-audit-crossreview-2026-10.md`（B） | 已升 v2 综合版（含 v1→v2 变更记录 10 条）；v1 备份在会话 scratch |

> **待回扫的连带文档**（打勾式执行时同批订正，判据：旧术语命中必须落在带标注句中或命中 0）：`PrimitiveSortSystem`/`RunSorting`（4 档）、`change_mask`/`LineChange`（5 档）、`88B`（9 档）、`BoundingBoxAccessor`（5 档）、`MarkDirty`（6 档）——逐档清单见 B 档 v2 §5.3。

### 4.3 未复现 / 未验证（如实标注，勿当结论）
- **C 的探针数字未独立复现**（`expected_first=2 actual_first=4`、`a_movable=1/d_dynamic=0`、`overflow_valid=1/overflow_has_slot=0`、`before=1 after=1 world_x=4.0`、父环 >3s）：B 只做**代码级复核**（机制与代码逐字吻合）。建议把 `ecs_crossreview_probe.cpp` + 日志落进仓内（`src/ecs/support/` 或 PR 附件）。
- **GPU/视觉后果未目视**：B4 的"阴影残留"、B1 的错位、B2 的卡死均需示例（本会话无声，GUI 示例有已知早退现象）。
- **未跑全量构建**：本轮只跑 `ctest -N -L ecs`（=9）、`TestTransformFlatStorage`（rc=0）、`cl /Bv`、vcxproj 计数。

### 4.4 本档边界
只读清单：本档与 B 档均**未改引擎代码**（工作区改动面 = 4 个 `doc/*.md`）。行号属 `e8ee755fd`，开工前先 `git log --oneline -1` 核对。

---

> ### ✅ §4 连带文档回扫执行记录（2026-10-11）—— 五个旧术语全仓定性，无一条"未标注的当前状态错误"
>
> **判据口径（先说清限度）**：术语名在本仓是**重载**的（同名不同接收者），"grep 归零"不可能也不该是判据。故判据 =
> **逐条归类**：每个命中要么落在带标注句中，要么属于下列四类"不需标注"，**且四类的归类理由逐条成立**。
>
> **落地动作（12 处标注 / 7 档，分两轮）**
> · `doc/ecs-layer-architecture-and-frame-flow.md`：按本文既有约定加 **dated 订正块**（2026-10-11），点明
>   `Component`/`Component.h`/`Component::version`/`change_mask`（`:71`、`:332`、`:582`、`:586}）**组件基类整删**（T2-c）、
>   `TransformAccessor::MarkDirty`（`:519`）**已删**（§3-①，连带标脏一直由 `MarkLocalChanged` 承担）、
>   `PrimitiveSortSystem`/`RunSorting`（`:358`、`:394`、`:425`）**保留**（§1.4；`RunSorting()` 空体 = 占位无操作，
>   其帧前准备自 §2-#11 起为每帧真守卫）、`BoundingBoxAccessor` **现值 16 B**。
> · `doc/ecs/transform-data-management-and-transfer.md`：`:42` `MarkDirty`→`MarkLocalChanged`（版本 + 掩码 + 子孙传播）；
>   `:67` `TransformComponent::MarkDirty` 段加"组件已整删，记账现由 `TransformAccessor::MarkLocalChanged` →
>   `TransformDataStorage::TouchChange/SetDirty` 承担（语义仍成立）"。
> · `doc/backlog.md`：静态写入处 `MarkDirty` → 变更记账 `MarkLocalChanged`。
> · `doc/ecs-quality-audit-2026-10.md`（旧账本）：三条 finding 就地补处置 —— `PrimitiveSortSystem` 保留（§1.4）、
>   `LineChange` 随批 1.2 A2 已删（`TextChange` 族保留）、`MaterialRuntimeSlot` 88 B → **40 B**（批 1.3 C1）。
> · `doc/future/stage_B_execution_plan.md` / `stage_B_debt_list.md` / `ULRE_Transform_System_Completion_Design_and_Plan.md` /
>   `ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md`：**只改"会被后来者当基线引用的数字"** ——
>   `MaterialRuntimeSlot` 现值 40 B（2 处）、`BoundingBoxAccessor` 现值 16 B（3 处）。
>
> **残留归类（术语 / 命中 / 已标注 / 残留 = 同名不同接收者 + 不同量 + dated 快照 + 保留口径仍准）**
>
> | 术语 | 命中 | 已标注 | 残留归类（逐条理由） |
> |---|---|---|---|
> | `PrimitiveSortSystem` | 10 | 3 | **保留口径仍准**（7）：帧图/链路口径的 4 档叙述仍成立——系统保留、相位不变；2 条是旧账本 finding 行（处置在 `:202`/`:222`）；1 条 `.h`/`.cpp` 相位注释不一致（与本批改动无关） |
> | `RunSorting` | 9 | 5 | 同上（4） |
> | `change_mask` | 14 | 0* | **同名不同接收者**（6）：`TransformDataStorage` 的 `change_masks` 是**活字段**、含其真 bug 记录（`ULRE_Phase0-1:837`）；**dated 快照**（4）：`ULRE_Architecture_Technical_Review` / `entitygpu-and-ulrescene-design` 的 `sizeof(Component) 104 B` 等历史表；**已被 dated 订正块覆盖**（4）：`ecs-layer:72/333/583` + 块内点名（`*` 块首行含 `change_mask`，故逐行计数不体现） |
> | `LineChange` | 3 | 2 | **dated 快照**（1）：`ULRE_Phase0-1:1102` 是当时的计划项 |
> | `88 B` | 28 | 9 | **不同量**（19）：EntityGPU 88 B 载荷（正确）、BC7 `699088 B`、`TransformDataStorage 488 B`、`888 B`、terrain `524288 B`、MeshDrawParams 历史值（`gpu-driven-4id:373` 已 ✅ 已修） |
> | `88B` | 10 | 0 | **不同量**（10）：MeshDrawParams 88B→112B 历史/注释残留（已有 ✅ 已修 记录）、push constant、`GlobalAddresses` 64→88B、meshlet `24B+88B=112B` |
> | `BoundingBoxAccessor` | 15 | 10 | **不同量/上下文行**（5）：`Object` ID 行（顺带提及）、ABI 断言分析行（历史，且已写明该断言不成立）、债务账"已落地"段落（现值注已加在其下一行）、访问器裸指针行（未做 24 B 断言） |
>
> ⇒ **判定为"当前状态描述错误且未标注"的命中 = 0**。
>
> **验证**
> · `ctest -L ecs` **12/12**（其中 `TestCSMIncrementalPass` 会**运行时读取** `doc/future/stage_B_execution_plan.md`
>   等档做针校验 ⇒ 正是本批文档编辑的回归判据）· `ctest -R TestCSM*` **1/1**
> · 本批只动文档，不涉构建；术语残留分类见上表（可 grep 复现：`git grep -n "<术语>" -- doc` 后按下表归类）
>
> **§4 状态**：4.1 覆盖缺口记账 / 4.2 文档面修正（已就地落地）/ 4.3 未复现与未验证 / 4.4 本档边界 —— 均已在档；
> 4.4 的「待回扫连带文档」清单（`PrimitiveSortSystem`/`RunSorting`、`change_mask`/`LineChange`、`88 B`、
> `BoundingBoxAccessor`、`MarkDirty`）**本批执行完毕**。⇒ 共识档 §1–§4 全部结清。

## 5. 附：四档关系

| 档 | 角色 | 现在的位置 |
|---|---|---|
| `doc/ecs-quality-audit-2026-10.md` | 旧账本（含 chunk 1–4 处置记录、§2.2 六条待拍板） | 历史 + 处置依据；顶部有交叉评审入口 |
| `doc/analysis/ecs-audit-2026-10/`（6 份） | 旧账本 §2.2 六条待拍板的逐条详述 | 待拍板项的细节来源 |
| `doc/ecs-deep-quality-audit-and-evidence-2026-10.md`（A） | 第一次深度审计（30 条断言） | 事实面已复核；行号/符号有漂移，顶部有订正块 |
| `doc/ecs-deep-audit-crossreview-2026-10.md`（B，v2） | 对 A 的逐条复核 + 与 C 的合流（含争议裁决、批 0/1/2/3） | **技术细节真源**（每条结论的 path:line 与裁决都在这里） |
| `doc/ecs-deep-audit-second-crossreview-2026-10-11.md`（C） | 第二轮（带探针，发现 4 项正确性缺陷） | 正确性缺陷的发现方；顶部有综合结论指针 |
| **本档** | **按"有没有分歧"重排的执行清单** | **开工入口**：无争议项照 §1 做，分歧项照 §2/§3 拍板 |
