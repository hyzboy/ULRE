# §2.2 问题 2 详述："实体有哪些组件"存 4 份（审计的 4–5 份里有一份要订正）

> **审计出处**：`doc/ecs-quality-audit-2026-10.md` §2.2「并行表家族」第 2 条
> **结论一句话**：这条**基本成立但最后一小截要订正** —— 真正的复制链是 **4 份**（权威 `component_rows` → CPU 行镜像 → GPU SSBO(按帧槽) → GLSL 镜像），审计列的"另有 `EntityRenderIDTable`"**不属于本链**（它是 4-ID 元组，见问题 3）。另：这条链上**已经写好一个逐字段对拍器 `EntityGPUTable::VerifyAgainst()`，但它全仓零调用**。

---

## 一、现状：这条链的 4 份

| # | 形态 | 位置 | 谁写 | 谁读 |
|---|---|---|---|---|
| 1 | **权威**：`Entity::component_rows[16]`（1 基行号，0 = 无） | `inc/hgl/ecs/core/Entity.h:42` | `Entity::SetComponentRow()`（`Entity.cpp:33-47`，唯一写者） | 挂载/卸载、`Refresh`、掩码对拍 |
| 2 | **CPU 行镜像**：`EntityGPUTable::rows[i]`（`EntityGPU` 128 B：`type[16]` + `row[16]`） | `inc/hgl/ecs/support/EntityGPUTable.h` | `Refresh(entity)`（`:169-185`，全量填 `type[]/row[]`）+ `SetTransformRow(id,l2w_row)`（`:188-212`，只动 Transform 槽，**值变才标脏**）+ `SetVisible(id,bool)`（`:132-141`） | 上传、对拍器、探针 |
| 3 | **GPU SSBO**（`EntityGPU` 行） | `src/ecs/core/ContextGPU.cpp:173-175`（`UploadDirtyRowsHelper<EntityGPUTable, EntityGPU>`），缓冲**按世界地址槽切 `kWorldAddressesSlotCount` 份**（`:128-140`） | 脏行上传（`DirtyRowSet`） | GPU 剔除着色器、形态 B gather |
| 4 | **GLSL 镜像** | `ShaderLibrary/ecs/entity_gpu_cull.glsl`：读 `entity.flags`（Active/Visible 位，`:226-232`）与 `entity.row[1]`（Transform 的 L2W 行，`:252-256`） | —— | 剔除判据 |

另有**一层行内冗余**（审计提到、确实存在且**有意**）：`EntityGPU::type[16]` —— 槽号本身就蕴含类型，作者自注（`inc/hgl/ecs/core/EntityGPU.h:46-52`）：

> 槽 i != 0 ⇒ 本实体有 `ComponentType(i)` 的组件（**槽号已蕴含类型 ⇒ 该数组是显式冗余**；按 v2 §3 保留，便于着色器判空写法直观：`if (e.type[CT] != 0)`）

**要订正的那半截**：审计把 `EntityRenderIDTable` 列进"同一份'实体有哪些组件'存 4–5 份"。实测不是 —— `EntityRenderIDTable::rows` 存的是 **4-ID 元组**（`transform/geometry/material/texture`，16 B `RenderItemDescriptor`），**不是组件行号表**；它与本链的关系只是"同属实体索引空间"，与"哪些组件"无关。这条应归入问题 3。

### 1.2 结构契约（不可擅改，有 static_assert 钉住）

`inc/hgl/ecs/core/EntityGPU.h`：
- 形状 = `flags(4) + type[16](16) + row[16](64) + work_flags(4)` = **88 B 载荷**，`alignas(64)` ⇒ **128 B**（`static_assert(sizeof==128)`、`alignof==64`、逐字段 `offsetof` 断言）；
- 槽位语义 = `ComponentType` 值、每类型最多一行（**P-3 拍板：与 `Entity::component_mask` 1:1**）；
- `row[i] == 0` = 无（全仓"行 0 哨兵"惯例）；
- `shadow_bias_multiplier` / `shadow_receive` 用 `_pad` 区 8 B（T3h，写者 = collect，与 4-ID 表同一判据点）；
- 归属：**世界私有**（落到设备级全局池会让多世界同帧互相覆盖 —— 相机行当年踩过的同型坑）。

---

## 二、问题：刷新点分散 + 对拍器没人调

### 2.1 刷新点（三个写入路径，缺一处即静默不一致）

| 写入者 | 触发 | 覆盖范围 |
|---|---|---|
| `Refresh(entity)` | 组件挂载（`Context.h:1111`）/ 卸载（`Context.h:757` `RefreshEntityRow`） | 全槽（除 Transform 由专门路径） |
| `SetTransformRow(id, row)` | `TransformSystem.cpp:47,57,64`（销毁/换边 ⇒ 写 0；正常 ⇒ 写 L2W 行） | 只 Transform 槽 |
| `SetVisible(id, bool)` | `Context.h:771`（`SetEntityVisible` 内） | 只 Visible 位 |

### 2.2 已经有对拍器，但**全仓零调用**

`inc/hgl/ecs/support/EntityGPUTable.h:222-255`：

```cpp
/// `type[]/row[]` 是否与 `Entity::component_rows[]` 逐字段一致
/// （`row[Transform]` 不参与：它由变换上传路径给，见 `SetTransformRow`）
bool VerifyAgainst(const Entity *entity, uint32_t *out_mismatch_type = nullptr,
                   uint32_t *out_mismatch_row = nullptr) const;
```

`grep -rn "VerifyAgainst" inc/ src/` ⇒ **只有这一处定义，没有任何调用者**（连测试都没调）。

⇒ 与问题 1 的 `Entity::VerifyComponentMaskAgainstComponents()` 完全同型：**两个对拍器都写了、都没接进生产**。"这条链不会漂移"目前靠的是"所有人恰好都走那三个写入者"，而不是门。

### 2.3 危险点具体化

- **漏刷新**：新增/改动挂载路径时忘记 `Refresh` ⇒ CPU 行停在旧状态；若该行已被标脏并上传，GPU 剔除会按**旧几何 / 旧 L2W 行**判可见性（后果与问题 3 的场景 (a) 同族：不画多半被 batch tag 兜住，但剔除账与 AABB 判据是错的）。
- **`row[Transform]` 的独立口径**：L2W 行号**不在** `Refresh` 的覆盖范围内（对拍器显式跳过它）⇒ 这条槽的"权威"其实是**变换上传路径**，不是 `component_rows[Transform]`。这是本链里唯一的"两个权威"点，值得写进头注（现在只在 `SetTransformRow` 的注释里）。
- **`work_flags` 恒 0**：字段存在、无人写（审计 §7 已判"只有 `work_flags` 死"）。它作为**保留的载荷位**在 §1.1 清理时被刻意留下（GPU 行布局合同），保持现状即可。

---

## 三、处置选项

### 选项 (a)：把对拍器接进生产（debug 期）——推荐先做

- 落点：`Refresh` / `SetTransformRow` / `SetVisible` 的**调用侧**（挂载/卸载、TransformSystem 的写入点），`#if ULRE_ECS_DEBUG_API` 下调用 `entity_gpu_table.VerifyAgainst(entity, &t, &r)`，不一致即 `GLogError` 点名 `type` 与 `row`（返回的正是这两项）。
- 成本：~10 行（三处各 2-3 行）+ 一个开关；生产零开销。
- 收益：把一个**已经写好的**对拍器用起来（现在它和不存在等价）；把"三个写入者覆盖全部槽"变成 debug 期可验证的断言。
- 验证：破坏验证 —— 临时从 `Refresh` 里去掉 `row->row[t] = handle;` ⇒ 期望 debug 点名 `out_mismatch_row=t`；还原复绿。
- 注意：`row[Transform]` 不在比对范围（对拍器已排除）⇒ 若要覆盖它，需要额外的"L2W 行号 vs 变换上传路径"的判定，那是另一条链的契约（建议单独一条）。

### 选项 (b)：把权威链写进头注（零风险，长期价值）

在 `EntityGPU`/`EntityGPUTable` 头注里明写**四份的权威顺序与刷新时机**：`component_rows`（权威）→ `Refresh`（挂载/卸载全量刷新）→ `SetTransformRow`（Transform 槽的**另一权威**：变换上传路径）→ 脏行上传（按帧槽）→ 着色器只读。防的是"下次审计把 `type[16]` 当冗余删掉"（它有意冗余，删了会破坏 shader 判空写法与 `offsetof` 断言）。

### 选项 (c)：收敛副本（成本高，不建议现在做）

把 CPU 行镜像改为"按需从 `component_rows` 生成、不常驻"—— 但它是**上传计划的载体**（`DirtyRowSet` + 逐行 `Row(idx)` 拷贝），去掉它等于重做上传通道；且 `EntityGPU` 的 128 B/64 B 对齐是 GPU 侧契约。**收益（省 128 B/实体）远小于风险**。

---

## 四、建议

1. **(a) 立即接对拍器**（和问题 1 的 (a) 同一批做，两个门一次写）；
2. **(b) 补头注**（同批，纯文字）；
3. **(c) 不做**：本链的"冗余"是 GPU 行载荷的既定形态，`EntityGPU.h` 自注已经说清，问题只在"没人守门"。

---

## 五、未证实 / 需要确认

- ~~`VerifyAgainst` 是否曾有过测试调用、后来被删~~ ⇒ **已查（2026-10-13）**：`git log --oneline -S "VerifyAgainst" -- inc/hgl/ecs/support/EntityGPUTable.h`
  只有一条 `1d877788a`（T3b/B8 引入），**从未有过测试调用**；唯一的调用者是探针 `ProbeTransformDiagnostics.cpp`（`[T0c]` 四项判据）。结论不变：它此前与"不存在"等价（本批已接进生产写入点，见 §六）。
- ~~GPU 剔除着色器读 `entity.row[1]` 之外的槽（如 `row[Geometry]`）？~~ ⇒ **已确认（2026-10-13）**：`ShaderLibrary/ecs/entity_gpu_cull.glsl` 里 `row[]` 只出现两处 ——
  `:254 const uint transform_id = entity.row[1u];`（Transform 槽）与注释行 `:17`；`:226/:230` 读 `entity.flags` 的 Active/Visible 位；`:235` 明写"不能用 `entity.row[2]` 当 geometry_id"。
  ⇒ **其余 14 槽当前没有任何 GPU 消费者**（CPU 侧：上传走脏行、对拍走 `VerifyAgainst`）。"`Refresh` 退化成只填会被读的槽"属于下一步优化，不在本批。

---

## 六、落地记录（2026-10-13；选项 (a) + (b) 均已完成并验证）

**对拍器接线 (a)**：`EntityGPUTable::VerifyAfterWrite(entity, what)`（`inc/hgl/ecs/support/EntityGPUTable.h`；函数体内
`#if ULRE_ECS_DEBUG_API` ⇒  Release 下为空 inline，**生产零开销**；开关在台帐 `hgl/common/RenderOptions.h` §2）。
接到**三个写入点**（含报告要求的 `Refresh` 调用侧、`SetVisible`、`SetTransformRow`）：

| 写入点 | 落点 |
|---|---|
| 组件卸载 | `ECSContext::RefreshEntityRow` |
| 组件挂载 | `ECSContext::CreateComponent`（`Refresh` 之后）|
| 可见性 | `ECSContext::SetEntityVisible` |
| 变换行 | `TransformSystem::WriteEntityTransformRows`（**三条路径各一处**：无变换 / 无槽 / 正常）|

失步即 `GLogError` 点名：**写入点 + entity 名 + id + mismatch_type / mismatch_row + 该槽两侧值**（`type[t]` / `row[t]` / `component_rows[t]`）。

**权威链头注 (b)**：`EntityGPUTable.h` 类头新增「权威链（四份副本 + 刷新时机）」一节 —— ①`Entity::component_rows` 权威 →
②本表 CPU 镜像（刷新点逐条列出）→ ③GPU SSBO（按世界地址槽切份、只传脏行）→ ④GLSL 只读镜像；
含两句防误删警告：`row[Transform]` 是链上唯一的**两个权威**点、`EntityGPU::type[16]` 是**有意冗余**。

**顺手修一处会让对拍假红的真缺口**：槽位复用（块池 + generation）下 `Deactivate` 刻意不清 `type[]/row[]`，
新实体在挂第一个组件**之前**与 `component_rows[]`（全新实体 = 全 0）不一致 ⇒ 现在 `Activate` 把这些槽清成「无组件」
（与权威同源；挂卸仍由 `Refresh` 全槽同步）。

**契约钉住**：`TestCSMIncrementalPass` **Test 49** —— 3 条存在性（对拍器本体 / 门在 `#if ULRE_ECS_DEBUG_API` / 头注含「权威链」）
+ 2 条写入点存在性 + 1 条**计数**（变换行三条路径各接一处）+ 1 条**禁复活**（不许在写入侧裸调 bool 版 `VerifyAgainst`：
返回值一丢就等于没有门）。

**红的样子（实测）**：把 `Refresh` 里的 `row->row[t] = handle;` 改成 `0`（模拟漏刷新）⇒ Debug 跑 `LoadScene` 打印
**899 条**失步，例：

```
[EntityGPUTable.h:312] [EntityGPUTable] 实体行表与 component_rows 失步（写入点=ECSContext::CreateComponent）：
entity=SceneNode_0 id=(0,0) mismatch_type=0 mismatch_row=2；该槽 type[2]=2 row[2]=0，权威 component_rows[2]=1
```

逐字节还原后 **0 条**。对拍**只报错不阻断**（示例照常跑完、帧摘要逐位不变），与"debug 面诊断"定位一致。

**验收**：两配置 `rc=0 / 0 error`；门 45 PASS/0 FAIL；`ctest -L ecs` 12/12 + `TestCSMIncrementalPass` 1/1；
`LoadScene` 自检摘要 `0xC9E38ED2F49A069B` **逐位不变**（⇒ 接线与 `Activate` 清槽对画面无影响）。

**未做**：选项 (c) 收敛副本（维持原判：收益远小于风险）。
