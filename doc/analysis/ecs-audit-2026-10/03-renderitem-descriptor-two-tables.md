# §2.2 问题 3 详述：同一份 16 B 4-ID 描述符由两条通道发布到两个 GPU 面

> **审计出处**：`doc/ecs-quality-audit-2026-10.md` §2.2「并行表家族」第 3 条（原文："同一份 16 B `RenderItemDescriptor` 存两张世界表……各自脏跟踪/缓冲/分配策略，**静默可失步**"）
> **结论一句话**：**值是真同源**（同一 call site 的四元组），但同一个值被**发布两次**，进两个不同索引空间、两套脏跟踪/上传通道、两个不同 GPU 消费者面，而**两块地方没有任何交叉校验** ⇒ 失步不会以"取不到值"的形式暴露，只会以"用了旧值"的形式静默存在。

---

## 一、两张表是什么

| | `RenderItemDataStorage::items`（"池"） | `EntityRenderIDTable::rows`（"实体 4-ID 表"） |
|---|---|---|
| 行 | `graph::RenderItemDescriptor`（16 B = transform/geometry/material/texture 四个 u32） | 同（头注：**不做第二个描述符形状**） |
| 索引语言 | `RenderItemHandle` + LIFO `free_list` 复用 | `flat = PassBase(pass) + EntityID.index`（**直索引**） |
| pass 维度 | **无**（一实体一行） | 有：按 `RenderPassSlot::Count` 分半（`EntityRenderIDTable.h:44-50`） |
| 唯一关联键 | 每实体槽 `MaterialRuntimeSlot::render_item_handle`（`PrimitiveState.cpp:62-77`） | EntityID.index |
| GPU 地址 | `WorldAddresses::addr_global_render_items`（`ContextGPU.cpp:504-506`） | `WorldAddresses::addr_entity_render_ids`（`ContextGPU.cpp:210-214`） |
| GPU 消费者 | **绘制/解析面**：`ShaderLibrary/common/RenderItemResolve.glsl:69-138`（按 `instance_index` / `item_id`） | **GPU 剔除面**：`ShaderLibrary/ecs/entity_gpu_cull.glsl:79,245,402`（按 `entity_index`；`geometry_id == 0` ⇒ 跳过本帧） |
| 脏跟踪 | **单段 range**（`dirty_min..dirty_max`，`RenderItemDataStorage.h:41-43`） | `DirtyRowSet` 逐行位 |
| 设备缓冲 | **单份** device buffer（无帧槽概念） | **每世界地址槽一份**：`kRowStride * kWorldAddressesSlotCount`，初始化全零（`ContextGPU.cpp:210-221`） |
| 上传者 | `RenderBufferUploadSystem::Update` → `SyncToGPU`（`RenderBufferUploadSystem.cpp:48`） | `UploadDirtyRowsHelper<EntityRenderIDTable, RenderItemDescriptor>`（`ContextGPU.cpp:184-186`） |
| 生命周期钩子 | 实体销毁 → `ReleaseRenderItemHandle` → `Release(handle)`（**行清成 0** + 标脏 + 进 free list，`RenderItemDataStorage.cpp:59-68`） | 实体销毁 → `DeactivateEntityRow` → `Clear(id)`（**两半都清零** + 标脏） |

> ⚠ 顺带一条名不符实：池的地址字段叫 `addr_global_render_items`（"global"），实际是**每世界私有**（`Context.h:188` 世界成员；`ContextGPU.cpp:504` 从本世界取地址）。属 §5 那一族，动手时别被名字带到"跨世界共享池"上。

---

## 二、值同源的确切证据（审计这部分是对的）

`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp` 同一段函数体：

1. `:2318-2327` **算一次**四元组：`transform_id = transform.GetID()` / 池口径 `geometry_id`（`geometry_comp->GetGeometryID()`）/ `material_id = material_for_item->data_index_row` / `texture_id = material_texture_configuration.row_index`；
2. `:2329` `SetRenderItem4ID(*world, entity_id, transform_id, geometry_id, material_id, texture_id)` → 写**池**；
3. `:2360` `world->GetEntityRenderIDTable().SetRenderIDs(pass, entity_id, render_ids)` → 写**实体表**（`render_ids` 由上面同 4 个局部量填）。

⇒ 不存在"第二次解析"，头注宣称的"四元组与 `SetRenderItem4ID` 逐字同源 ⇒ 无第二口径"**在值层面成立**。

---

## 三、四条不对称（失步的真正来源）

### 3.1 写条件不对称（最硬的一条）

- **池**：`:2329` **无条件**写 —— 只要本帧被 collect（走完前置判据），池里就有值。
- **实体表**：`:2360` 位于 `if (geometry_id != 0)` **且** `if (params_row)` 的**"本项进批"判据块之内** —— 行缺失 / 几何为 0 时**根本不写**，保留旧值。

### 3.2 索引空间不对称

句柄（可被 free list 复用、`Allocate/Release` 成对）vs 实体 index（直索引、随实体生死）。两者能对上**只靠每实体槽里那个 handle** ⇒ 任何"用实体 index 猜池行"的代码都是错的（这正是 `entity_gpu_cull.glsl:235-240` 那段注释血泪写的旧 bug）。

### 3.3 pass 维度不对称

实体表按 pass 分半，理由写在 `EntityRenderIDTable.h:44-50`："本表行内容是**按 pass 解析**的（阴影 pass 用阴影程序/材质变体）⇒ 一份表存不下两个 pass 的风味"。而**池没有 pass 概念** ⇒ 池里存的是"**最后一次 collect 的那个 pass** 的口径"（写点用 `world->IsCurrentPassShadow() ? 1u : 0u` 区分的是表，不是池）。绘制面读池时并不带 pass 语义。

### 3.4 脏/上传/帧槽不对称

| 维度 | 池 | 实体表 |
|---|---|---|
| 脏粒度 | 单段 range（`dirty_min..dirty_max`，一次上传覆盖整段） | 逐行位（`DirtyRowSet`） |
| 缓冲份数 | **1 份**（原地增量写） | `kWorldAddressesSlotCount` 份（每帧写自己那一槽，初始化全零） |
| 上传者 | `RenderBufferUploadSystem` | 世界行表族（`ContextGPU.cpp:184`） |

---

## 四、为什么失步是"静默"的（三条机制）

1. **没有对拍门**：两表互不校验；且两侧都做了"值变才脏"（池 `Set()`/`Set4ID` 判 `!= desc`；实体表 `if (*row == desc) return`，`EntityRenderIDTable.h:120-124`）⇒ 一致时零成本、**不一致时也零声音**。
2. **消费者有回退路径 ⇒ 陈旧值不会被"取不到"暴露**：`WriteBatchIndexRows`（`src/ecs/support/PrimitiveBatchPipeline.cpp:550-577`）先取 `storage->Get(handle)` 的 `material_id/texture_id`，**取不到**才回退 `MaterialRuntimeRow`。池里只要不是全零就永远"取到" ⇒ 陈旧值被当真值用掉，回退分支永不触发（也就永不报警）。
3. **剔除侧把 `geometry_id == 0` 当"跳过本帧"**（`entity_gpu_cull.glsl:247-249`）⇒ 实体表里"陈旧但非零"的行**看起来完全合法**。

---

## 五、具体后果（分场景，含被谁兜住）

| 场景 | 两表状态 | 后果 | 被谁兜住 |
|---|---|---|---|
| **(a) 本帧被 collect，但 `geometry_id == 0` 或 `params_row == nullptr`** | 池 = 新四元组；实体表 = **上一帧的几何**（或 0） | 剔除按**旧几何的 AABB** 判可见性；池与表口径分歧 | 该实体不进批 ⇒ 无 batch tag ⇒ gather 按 `tags[...] == 0xFFFFFFFF` 跳过（`entity_gpu_cull.glsl:333-334,402`）⇒ **不画**。危害落在"剔除账 / AABB 判据用了本帧不该存在的几何"，画面看不出、台架对拍数字会偏 |
| **(b) 本帧完全没被 collect**（组件被摘 / 提前 continue） | 两表都留旧值 | 陈旧行看似合法 | 剔除先判实体行的 `flags & 1`（Active）与 `flags & 2`（**Visible**）（`:226-232`）⇒ 被 Visible 位掩盖。⚠ 掩盖是否严密取决于"停止 collect 时 Visible 位是否被清" —— **与问题 6 绑在一起** |
| **(c) pass 交叉** | 池 = 最后 collect 的 pass 口径；实体表 = 两半各记各的 | 池的 pass 语义"随最后一次 collect 漂移" | 无（这是设计层面的语义缺失） |
| **(d) 实体销毁** | 两表**都清零** | —— | 一致。（我原怀疑池只还句柄不清内容，实测 `RenderItemDataStorage.cpp:59-68` 是 `items[handle] = {}` + `MarkDirty` ⇒ **这一档无问题**，写在这里避免下次翻案） |
| **(e) free-list 复用** | 池把该槽给**另一个实体**；实体表仍是原实体的行 | 池行的"主语"变了 | 正常路径读不混（唯一关联键是每实体 slot 的 handle），但**两个表没有共同身份键**这件事本身是理解成本 |

---

## 六、未证实 / 需要确认（两条，都必须你确认或实验）

1. **帧槽保护不对称**：实体表按世界地址槽切份（在飞帧各读各的副本，与"per-frame ring 深度必须 == 槽空间"同纪律）；池是**单份**缓冲、每帧**原地**增量写。若 N 帧在飞，帧 N 的上传是否可能覆盖帧 N-1 绘制仍在读的内容 —— **我没有证明它安全，也没证明有洞**。
   判据方法：读 `RenderBufferUploadSystem::Update` 到 draw 之间的屏障/同步顺序（谁保证"上传只在本帧消费前生效且不跨帧覆盖"），或 RenderDoc 连抓两帧看该缓冲的写入时机。
2. **`geometry_id == 0` 之外，剔除侧是否还读可见性位** —— 已确认读 `flags & 2`（`entity_gpu_cull.glsl:230`）；但"停止 collect 时该位是否被清"未验（属问题 6 的权威链）。

---

## 七、处置选项（按成本递增）

### (a) 加门：每帧同步点的 debug-only 对拍（建议先做，风险零）

- 形态：`ULRE_ECS_DEBUG_API` 下，对"本帧进批的实体"比较**池行（经 slot 的 handle）**与**实体表行（本 pass 那一半）**是否逐字段相等，不等即 `GLogError` 点名（entity + 哪个字段 + 两侧值）。
- 成本：~40 行 + 开关；默认关 ⇒ 生产零开销。
- **先决问题（只有你能回答）**：**"池无条件写、实体表只在进批判据内写"是有意的还是疏漏？**
  - 若**有意**（"池是每实体恒有的一行；实体表只记'真会画'的"），门就按"只在进批时比对"，且应在两侧头注写明这条差异；
  - 若**疏漏**，门一开就会点名 —— 那正是要修的东西（修法：把池的写入也挪进同一判据块，或让实体表在"不进批"时写 0）。

### (b) 收敛写点（低成本重构）

把两处写收进一个函数 `PublishRenderIDs(world, id, desc, pass, ...)`，让"池无条件 / 实体表有条件"变成**显式参数**，而不是相隔 30 行、缩进不同的两个 if 里的裸调用。收益：下一个改这块的人一眼看到两条通道的条件差异；也便于把 (a) 的对拍写在唯一出口。

### (c) 真单源（成本高）

删池，改绘制面读实体表 —— 需要在绘制侧建立 `instance_index → 实体` 的映射（`RenderItemResolve.glsl` 现在按 instance/item id 取池）。收益 = 省 16 B/实体 + 一条发布通道 + 一处上传；代价 = 动绘制解析链，须端到端验证（且与问题 6 的可见性权威链纠缠）。

---

## 八、建议

1. **先答"有意还是疏漏"** —— 这一个问题决定 (a) 的门怎么写、也决定 (b) 的方向。
2. 按答案做 **(a) 的门**（零风险、把"静默"变成"点名"），顺手 **(b)**（把条件显式化）。
3. **(c) 留到问题 6 的权威链定完再谈**：它们本质是同一件事的两面（"谁在什么条件下把一行发布给 GPU"）。
