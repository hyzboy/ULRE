# ECS 深度审计第二轮交叉评审（2026-10-11）

> **评审范围**：复核 `ecs-deep-quality-audit-and-evidence-2026-10.md` 与 `ecs-deep-audit-crossreview-2026-10.md`，并对照当前仓库代码做独立验证与补充。
>
> **代码基线**：`e8ee755fded763bc7b47846cc1cbeab9ab3acf65`（`ECS 审计 §2.2 六条待拍板项：逐条详述报告（6 份）+ 审计档索引`）。本轮审查时 HEAD 与两份被评文档所述基线相同；主要分歧来自调用链、失败路径和契约理解，而非提交漂移。
>
> **性质**：只读代码评审与最小复现。未修改 ECS 代码或原审计档。工作区已有的其他改动未触碰。
> 
> **综合结论指针（2026-10-11，由 `doc/ecs-deep-audit-crossreview-2026-10.md` v2 给出）**：本档的 4 项 P1 与 4 项 P2 已逐条**代码级**复核——**4/4 + 4/4 成立**（P1-1/P1-2/P1-3 逐字复核；P1-4 的信号链结论成立，且双方口径一致地把「GPU 阴影残留」留待示例目视）。另补两条同类发现：`TransformDataStorage::Clear()` 全仓**零调用者**（P2-3 的 Transform 侧隐患目前是「死 API + 陷阱」）、`LinesComponent::version` 同为零读者。  
> · **待订正 3 处**：① 「第二份称 `MarkSynced()` 零调用」——第二份写的是「零**读者**」，且其删除清单已含调用点 `LineRenderPipeline.cpp:595`（措辞易误读，v2 已统一改为「写者存在、效果无人读」）；② 4 个探针文件建议落进仓内（`src/ecs/support/` 或 PR 附件），否则数字不可复查；③ 「重新构建并运行 10 项测试」宜写成 **9（`-L ecs`）+ 1（`TestCSMIncrementalPass`）**，与 `ctest -N` 实测口径一致。  

## 结论摘要

第二份交叉评审比原始报告更准确，但仍不能直接作为整改清单：它纠正了部分旧结论，也引入了新的误判。两份文档共同漏掉了 **4 项应优先处理的正确性问题**，以及若干生命周期、句柄和登记一致性缺口。

本次重新构建并运行了 10 项相关测试，全部通过：`-L ecs` 对应 9 项，另加 `TestCSMIncrementalPass`。独立探针复现了以下状态错误：交错添加子节点会丢失旧子节点；双节点父环会卡在 `SetParent`；迁移到满段会挤掉原有变换；正常 `Update → Submit` 顺序会漏掉静态场景 revision。测试全绿说明这些边界尚未被当前测试覆盖，不表示相关问题不存在。

## 一、新发现：4 项优先正确性问题

### 1. P1：CSR 子表搬迁没有复制旧内容，普通层级装配即可损坏子表

**位置：** `inc\hgl\ecs\support\TransformDataStorage.h:571–590`

`AddChild()` 在当前节点的子区间不位于池尾时，只把 `child_offset[id]` 改到新池尾，然后追加新子节点，却仍将数量设置成 `旧 count + 1`。旧子节点没有搬过去。

最小触发顺序：

```text
P 添加子节点 A
Q 添加子节点 B
P 再添加子节点 C
```

本次实测：

```text
expected_first=2 actual_first=4 count=2
```

即 P 的第一个子节点应为 A，实际变成 C。此时 P 声称拥有两个子节点，但新位置只追加了一项；随后 `CopyChildren()` 的区间还可能超出 `child_pool.end()`。

**影响不只是查询错误：**`TransformAccessor::MarkLocalChanged()` 依赖这张表遍历后代、递增版本，因此会遗漏真实后代、访问错误区间，破坏变换更新。

**现有测试为什么没抓住：**`TestTransformFlatStorage.cpp:702–704` 只连续向同一父节点追加，始终走“区间贴着池尾”的分支，没有覆盖交错追加。

**修复方向：**搬迁时复制完整旧区间，再追加新节点；增加交错追加、删除后再追加的回归用例。

### 2. P1：访问器创建父环会先陷入无限遍历，存储层的环保护来不及执行

**位置：**

- `src\ecs\support\TransformAccessor.cpp:218–238`
- `src\ecs\support\TransformAccessor.cpp:73–99`

`SetParent()` 只处理“自己作为自己的父节点”，没有拒绝祖先环。它更新父链和子表后，立即调用 `MarkLocalChanged()`；后者遍历子表时没有 visited 集合或其他终止保障。

最小触发：

```text
A.SetParent(B)
B.SetParent(A)
```

实测第二次调用超过 3 秒未返回，已仅终止该探针进程。

这里不能用 `RebuildTopologyOrder()` 的环保护证明安全：程序在进入拓扑重建之前，已经卡在后代标脏循环里。

**现有测试为什么没抓住：**`TestTransformFlatStorage.cpp:149–151` 直接调用存储层 `SetParent()`，随后主动重建拓扑，绕过了有问题的访问器路径。

**修复方向：**在修改任何父链、子表之前验证新父节点有效且不会成环；防御性遍历也应能够终止。不能仅依赖之后的拓扑修复。

### 3. P1：段预算拒绝不是原子操作，迁移失败会破坏已有槽位

**位置：**

- `src\ecs\core\Context.cpp:862–905`
- `src\ecs\support\TransformAccessor.cpp:500–521`
- `src\ecs\core\Context.cpp:922–940`

`MigrateTransform()` 先释放原段槽位，再申请目标段。目标段满时，其“复位”代码：

```cpp
(toMovable ? movable_transforms : static_transforms)[old_slot] = id;
```

写入的仍是目标段，不是原段。

声明 `static=2、dynamic=1`，动态段已经有 D，再将静态 A 迁过去，实测：

```text
a_static=0 a_dynamic=1 d_dynamic=0 a_movable=1
```

结果不是“拒绝并留在原段”，而是 A 挤掉了 D。若原槽下标超出目标段长度，该写入还会越界。

这里至少有三个关联错误：

- 原段槽已进入 free list，失败时没有撤销这笔账。
- 访问器在申请成功之前已经修改 storage 的 mobility。
- 即使只改对上述三元表达式，仍无法完整回滚状态。

**初次创建也有同类契约缺口：**总行预算尚有空间、但所选段满时，`CreateTransform()` 返回有效行，而 `RegisterTransform()` 没能分配 L2W 槽。实测：

```text
overflow_handle=1 overflow_valid=1 overflow_has_slot=0
```

这与函数上方“不再造半个变换”的注释不一致。

**修复方向：**先取得目标槽，成功后再提交换段和 mobility；创建时必须传播登记失败并回收已分配行，不能返回“有效但无法渲染”的半成品。

### 4. P1：正常更新顺序会吞掉静态场景 revision，删除静态实体也不触发失效

**位置：**

- `src\ecs\systems\tick\TransformSystem.cpp:73–78、195–224、531–551`
- `inc\hgl\ecs\support\TransformDataStorage.h:858–860`
- `src\ecs\systems\render\MainLightShadowSystem.cpp:250` 起

当前链条是：

```text
修改静态 Transform
→ TransformSystem::Update()
→ UpdateDirtyWorldMatricesFlat() 清除 matrixDirty
→ SubmitTransformUpdates()
→ ShouldUpdateTransform() 因 IsDirty=false 返回 false
→ 不递增 static_scene_revision
```

实测先直接提交建立基线，再修改位置并执行正常 `Update → Submit`：

```text
before=1 after=1 world_x=4.0
```

CPU 世界矩阵确实改变了，但静态场景 revision 没变。相同问题也可能由其他提前读取世界矩阵的路径触发，不能仅靠调整某一个调用点的顺序解决。

此外，删除最后一个静态实体后再提交，实测：

```text
before=1 after=1 live_static=0
```

删除路径同样没有通知静态缓存失效。

**现有测试为什么没抓住：**`TestCSMIncrementalPass.cpp:1349–1368` 在写入后直接 `SubmitTransformUpdates()`，没有经过正常 tick 或提前求值；也没有覆盖删除。

**影响判断：**静态阴影缓存消费者依赖 revision，因此失效信号可能丢失。本次验证的是信号链断裂，没有将实际 GPU 阴影残留描述为已目视复现。

**修复方向：**不要用“CPU 矩阵缓存是否还脏”代表“静态场景变更是否已通知”。应按变更版本或明确的结构事件通知，并覆盖静态对象删除、换段等成员变化。

## 二、新发现的生命周期与登记缺口

| 优先级 | 问题与位置 | 本次证据及边界 |
|---|---|---|
| **P2** | **替换系统没有从元素类型索引中摘除旧实例。**`ContextSystems.cpp:155–177` | 主表和执行顺序表按类型替换，但元素列表比较的是 `*it == 新 system`，找不到旧对象，于是追加新对象。注册两次 `LineStatsSystem` 后，组禁用仍能禁用已替换的旧实例，证明旧实例继续留在组索引中。不是两次 tick 执行，而是登记和所有权不一致。 |
| **P2** | **裸组件池查行不检查活跃状态。**`ComponentPool.h:235–246`；`Context.h:1118` 起 | `DestroyRow()` 后活跃数为 0，`Row(旧行号)` 仍非空，违背 `GetComponentByRow()`“已释放返回 nullptr”的承诺。Shared 池则返回空，两个池实际语义不同。另外，裸池在 `row->~T()` 后调用 `SetOwner()`，也应调整到对象生命周期结束之前。 |
| **P2** | **`Clear()` 重置世代后，旧句柄重新有效。**`EntityManager.cpp:95–115`；`TransformDataStorage.h:869–893` | Entity 清空后重新创建，旧、新 EntityID 完全相同，旧 ID 校验通过。Transform 清空后重新分配，旧 accessor 的 `IsValid()` 又变成 true。世代保护没有覆盖 reset 边界。 |
| **P2，取决于重用契约** | **`Shutdown()` 清了实体，却没有同步清掉世界行存储。**`Context.cpp:264–294、318–326` | 对未初始化世界执行 Shutdown 后创建新实体，新实体重新取得旧 EntityID，并通过 `GetTransformID()` 查到了旧实体的变换。若世界支持重用，必须完整复位；若 Shutdown 是终态，后续 `CreateEntity/Initialize` 应明确拒绝，不能维持现在的半重置状态。 |

## 三、对两份文档的再次裁决

| 条目 | 本次裁决 |
|---|---|
| **`MarkDirty()`“永不上传”** | 两份都说重了。它没有 bump version，但 GPU 上传不只经过 `ShouldUpdateTransform()`：动态段每次提交全量写，静态段另有版本和布局判据。`ClockUse.cpp:335–350` 已先调用 `SetLocalRotation()`，后面的 `MarkDirty()` 是冗余，不是当前时钟不上传的证据。真正应优先追的是上述静态 revision 缺陷。 |
| **相机组件读取“双真值”** | 两份都缺少因果证据。`Context.cpp:569–600` 的世界 getter、GetOrCreate 最终仍调用同一个实体组件池，也没有在这里认领槽。两个入口是 API 收敛问题，不能仅凭入口数量认定存在两份状态真源。 |
| **RenderGraph 抬高 Update 下界** | 不是已证明的漏执行。`PrepareRenderPassSetup()` 已在 `ContextRender.cpp:846–860` 执行 Collect、Batch、Commit、Upload、FrameSync；之后从 DrawSubmit 开始更新，是避免在绘制段重跑前置工作。第二份只修正了已删除字段名，没有补齐这条调用链。 |
| **`IsCurrentPassShadow()` 九处分类** | 第二份分类仍错。`:2372` 是按 pass 写实体 4-ID 表，`:2391` 是成功后重置实例重试计数，都不是日志。九处应分为 **1 处策略事实注入、6 处程序选择/重试/状态落盘、2 处诊断**。不能按 grep 次数直接定性为策略重复。 |
| **Lines 变更字段** | `dirty/change_mask` 无业务读者的结论成立，但第二份称 `MarkSynced()` 零调用是错的，`LineRenderPipeline.cpp:595` 正在调用。`version` 同样只写不读，两份都漏了。可清理的是不再被消费的整套记账，而不是“所有函数零调用”。 |
| **RenderItem 重复释放** | 问题成立，而且后果比计数失真更重要：同一槽被两个分配者取得。实测两次释放都成功，后续两次 Allocate 都返回 0。计数是少算或钳零，不是原报告的下溢，也不是第二份的“虚高”。 |
| **第二份建议用零 descriptor 判断已释放** | 不能采用。`Allocate()` 和 `AllocateContiguous()` 都合法创建全零 descriptor。应使用明确的分配状态或已有槽账本机制，不能把载荷值当生命周期状态。 |
| **RenderItemHandle 0 是空句柄** | 原报告及旧账本均错。0 是第一条合法分配；真正无效值是 `UINT32_MAX`，见 `RenderItemDescriptor.h:44`。第二份未纠正。 |
| **`GetGPUVisibleCount()` 缺 Fence** | 第二份对当前调用链的降级合理：两个读者分别位于 fence 等待和 `SyncForParityReadback()` 之后。保留接口纪律风险即可，没有当前读到旧帧的复现证据。 |
| **`AddSegment()` 无回滚** | 缺回滚属实，但正常预分配、统一容量和前置有效性检查路径下，没有证明中途失败入口，不足以列为 P0。第二份“任何情况下不可达”的绝对表述也应收回。 |
| **`GetActiveCameraComponent()` 隐式认领** | 行为属实，但源码明确将它定义为惰性主相机解析。可讨论命名或拆分只读查询，不能仅凭 Getter 有副作用就定为高严重度故障。 |
| **有序 free list 却弹最大索引** | 是 O(N) 维护换取确定性复用次序的实现选择，不构成逻辑自相矛盾。改 LIFO 可作为优化，但应说明复用顺序变化。 |
| **CMake 双登记** | 源列表确实重复，但当前生成的 `ULRE.ECS.vcxproj` 中，`RenderSystemCore.cpp` 只有一个编译条目。属于登记清理，不是实际重复编译。 |
| **`cl /Bv` 无此开关** | 第二份错误。本机 MSVC 实际接受并输出各编译阶段版本。原报告用它检查 `static_assert` 也不准确——断言要通过编译相关 TU 验证。 |

其余主要事实仍可保留：`GetAllEntities/GetAllEntityPointers` 无生产消费者、R/F 动作无消费者、`uint32_t > UINT32_MAX` 恒假、五个场景模式为空壳、三项未使用形参、材质键字段应按第二份订正、40 B/88 B 注释冲突、空线段统计混入可见性剔除，以及 BoundingBox accessor 的瘦身候选。

**`PrimitiveSortSystem` 和材质分表应维持既有拍板。**其空排序体属实，但系统实际位于 `RenderCollect`；第二份方案中的 `RenderSort` 并不是当前枚举值。多个索引容器也不天然等于多份权威状态，应以实际失步路径判断——本次系统替换问题就是这样的具体证据。

## 四、验证范围与整改重点

本次重新构建并运行了 10 项相关测试，全部通过：`-L ecs` 对应 9 项，另加 `TestCSMIncrementalPass`。另外使用链接当前 ECS 库的独立探针，复现了上述状态错误；父环探针进行了限时终止。未运行 GUI/GPU 场景，也未宣称全仓所有示例均已构建。

复现源和输出保存在本会话 artifacts 目录：

- `ecs_crossreview_probe.cpp`
- `ecs_crossreview_probe_run.log`
- `ecs_crossreview_cycle.log`
- `ecs_crossreview_probe_build.log`

整改清单应首先补入 CSR 搬迁、父环拒绝、预算事务性、静态缓存失效这四项，并将已有重复释放问题按槽位别名风险处理。现有绿色测试不足以关闭这些问题；零调用删减、命名调整和结构收敛应与正确性缺陷分开记账，避免再次把“代码存在”“契约成立”和“测试覆盖”混为一谈。
