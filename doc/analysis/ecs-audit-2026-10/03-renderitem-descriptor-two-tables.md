# §2.2 问题 3 详述：同一份 16 B 4-ID 描述符由两条通道发布到两个 GPU 面

> **审计出处**：`doc/ecs-quality-audit-2026-10.md` §2.2「并行表家族」第 3 条（原文："同一份 16 B `RenderItemDescriptor` 存两张世界表……各自脏跟踪/缓冲/分配策略，**静默可失步**"）
> **结论一句话**：**值是真同源**（同一 call site 的四元组），但同一个值被**发布两次**，进两个不同索引空间、两套脏跟踪/上传通道、两个不同 GPU 消费者面，而**两块地方没有任何交叉校验** ⇒ 失步不会以"取不到值"的形式暴露，只会以"用了旧值"的形式静默存在。

> **定论（2026-10-14，用户拍板：以「删」结案，不补对拍门）**：
> ① 池的 **GPU 面零消费者** —— `ResolveRenderItem*` 全仓零调用，其启用开关字段 `uses_render_item_resolve` 已不存在（没有任何开关能打开它）；
> ② 池的 **CPU 面只剩一处**（`WriteBatchIndexRows` 取 `material_id` / `texture_id`）**且已有等价回退在位**（直读 `MaterialRuntimeRow`）；
> ③ 池的**索引粒度本身就是「每实体一行」**（`EnsureRenderItemHandle`：一实体槽一句柄、惰性分配、复用）⇒ 它**表达不了「一实体 N 实例」**，
> 对实例化 / 间接绘制**零增益**；而实例化真正依赖的「每实例 → 各自行」现由**每批 `l2w_index` / 每批材料行表 / 剔除侧输出行**承担（都不经过池）。
> ⇒ 池 + 二级索引表 + 压缩器 + 解析 GLSL **整族删除**；批行写入改读**实体 4-ID 表本 pass 那一半**（单一真源）。落地与验收见 §九。

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
   > **（2026-10-14 订正）本项随池整族删除而消灭**：不对称只存在于「池（单份）vs 实体表（按世界地址槽分份）」这对关系里；池没了，剩下的实体表本来就是每槽一份。见 §九。
2. **`geometry_id == 0` 之外，剔除侧是否还读可见性位** —— 已确认读 `flags & 2`（`entity_gpu_cull.glsl:230`）；但"停止 collect 时该位是否被清"未验（属问题 6 的权威链）。

---

## 七、处置选项（按成本递增）

> **（2026-10-14 订正）**：(a) 对拍门与 (b) 收敛写点**都不做**（无表可守）；(c)「真单源」以**整族删除**落地 —— 但落地形态是「批行写入读实体表本 pass 那一半」，
> 不是原文设想的「在 `RenderItemResolve.glsl` 里建 `instance_index → 实体` 映射」（那条通道本身已随本批整删）。见 §九。

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

---

## 九、落地记录（2026-10-14，以「删」结案）

**改动**（两批，各自可编译 + 判据绿）：

1. **批 A：删「二级索引族」（零行为变化）** —— `DrawItemIDStorage.h/.cpp`、`DrawItemCompaction.h/.cpp`、`TestRenderItemDataStorage.cpp`（867 行）整档删除；
   `RenderItemResolve.glsl` 只留「形状两件」（`RenderItemDescriptor` + `RenderItemBufferRef` —— 后者**活着**：剔除侧按 `EntityID.index` 用它寻址实体 4-ID 表），
   原有 BDA 寻址 / 直通+二级索引解析 / 两级缓冲视图 / 两个地址宏共 3136 字符整段切除；`WorldAddresses` 删 `addr_draw_item_ids`（9 → 8 字段）；
   连带：上传系统一行、`ContextGPU` 一处 pick、门归属表一条、`TestCSMIncrementalPass` 六条针、`PrimitiveBatchPipeline` 每帧空转 `Reset()`、三处 include。
2. **批 B：删「4-ID 池表」+ 批行写入切到单一真源** —— `RenderItemDataStorage.h/.cpp` 与随之而死的 `GpuMirrorBuffer.h` 删除；
   `RenderItem::GetRenderItemHandle()` 虚接口 + `PrimitiveRenderItem` 覆写、`graph::RenderItemHandle` / `INVALID_RENDER_ITEM_HANDLE`、
   `MaterialRuntimeSlot::render_item_handle`（行宽 **40 → 32 B**：删掉 4 B 句柄后还剩 32 B、天然 8 对齐，`static_assert` 与探针文案同步）、`PrimitiveState` 四个自由函数、
   `Context` 的成员 / Getter / 构造 / 销毁钩子、`WorldAddresses` 删 `addr_global_render_items`（8 → 7 字段）一并删除；
   `PrimitiveBatchPipeline::FinalizeBatch` 的行表取数改为 `world->GetEntityRenderIDTable().Row(本 pass, id.index)` ——
   **等价性由构造保证**：实体表写点就在 collect 的「几何行成立 ⇒ 本项进批」判据块内 ⇒ 能进批的项其本 pass 行必定本帧写过；
   同时订正了 9 处把「渲染项表 / DrawItemID 表」列为世界表内容的过期文档口径。

**验收**（清整棵 obj 树 + 挪开着色器产物缓存后全量重编，两配置）：Debug / Release 各 `rc=0`、`error C/LNK/MSB` = 0；
门 **45 PASS / 0 FAIL**（用例数不变：归属表删两条登记不减少用例）；`ctest -L ecs` **11/11**（12 → 11 = 被删的 `TestRenderItemDataStorage`）、
`TestCSMIncrementalPass` **1/1**、`-L shadergen` **13/13**（含 `ShaderCookSmoke`）；
**四个示例帧摘要逐位不变**（Debug 锚定值）：PBRSpheres `0x3DD88A0999CDC486`、AlphaModes `0x2FE87B15EE8B8A16`、
LoadScene `0xC9E38ED2F49A069B`、CascadeShadowMap `0x1CDAADEBF18B7CE4`。

**边界（本批不做）**：不保留任何「GPU 侧运行时解析 4-ID」通道（启用开关已不存在 ⇒ 将来要做就是新建）；
`receive_shadow` 幅度漂移与本条无关；实体表的可见性权威链仍属 #4。

**订正本报告两处**：§六 未证实项 1（「池单份缓冲的帧槽保护」）**随池消失而消灭** —— 该问题只存在于「池 vs 按槽分份的实体表」这对不对称里；
§七 的 (a) 对拍门与 (b) 收敛写点**都不做**（无表可守），(c)「真单源」以**删池**落地，但形态是「批行写入读实体表本 pass 那一半」，
**不是**原文设想的「在 `RenderItemResolve.glsl` 里建 `instance_index → 实体` 映射」（那条通道本身已整删）。

---

## 十、单轨化方案（独立计划档）的证据与取舍记录（2026-10-14 合并）

> 本节由 `doc/render-item-storage-decommission-plan.md`（2026-10-14 制定，未入库）**合并而来** —— 原档已删除，正文证据、取舍与验收矩阵全部落在本节（**保持一份**）。
> 合并时对原档做了三处事实订正（§10.3）。

### 10.1 原档的证据链（复核结论：全部成立）

1. **CPU 绘制批已物理删除**：`src/ecs/support/PipelineMaterialRenderer.cpp:207` 只在 `owner_batch->gpu_driven_override` 为真时 `ProcIndirectRender()`；该处原话"T4c4-3：CPU 绘制批已删 ⇒ 这里只剩接管批的 flush"。
2. **生产绘制全由 GPU-Driven 驱动**：绘制命令由 `entity_gpu_cull.glsl` 的 gather 写出（`cmds_out` / `mtl_out`），剔除只读 `addr_entity_render_ids`。
3. **Shader 端解析函数零调用**：`RenderItemResolve.glsl` 的直通 / 二级索引解析函数无活调用者；启用开关字段 `uses_render_item_resolve` 已不存在（与本报告 §一～§四 的独立复核同结论）。
4. **每帧的 CPU 计算与上传全属浪费**：collect 每帧为每个图元算 4-ID 并写池、`RenderBufferUploadSystem` 每帧 `SyncToGPU`，而该 SSBO 无任何着色器解引用。

**本轮补强（原档未覆盖）**：不只是"GPU 面没人读" —— 接管切换时 `src/ecs/support/GPUCullSystem.cpp:862-864` 会把 `l2w_index_buffer` **与** `material_data_index_rows_buffer` 两个指针**整体换成 GPU 输出表**
（`l2w_out` / `mtl_out`，并置 `own_l2w_index=false` / `own_material_data_rows=false`）；而 `PrimitiveBatchPipeline::WriteBatchIndexRows` 开头即
`if (batch.items.empty() || batch.gpu_driven_override) return;`（`src/ecs/support/PrimitiveBatchPipeline.cpp:529`）⇒
**这两列 CPU 行写入对任何"会画的批"都不生效**（非接管批按 T4c4 不画）⇒ 整块属死工作。

### 10.2 关键取舍：占位（A1）vs 物理删除（A2）—— 拍板：A2

| 维度 | A1 占位（原档推荐先行） | A2 物理删除（**本批落地**） |
|---|---|---|
| `MaterialRuntimeSlot::render_item_handle` | 改名 `_pad_render_item`，保住 40 B | 删字段 ⇒ **40 → 32 B**（8 字节对齐） |
| `WorldAddresses::addr_global_render_items` | 降级为占位字段、恒写 0 | 删字段 ⇒ 世界表 **9 → 7 字段** |
| GLSL / 产物缓存 | 不动 ⇒ 不必重编 ShaderLibrary | 结构变更 ⇒ 挪开着色器产物缓存重编（已做） |
| 契约 / 门 | 针与归属表可保留 | 四处同步（GLSL 结构、parity 用例自动逐字段提取、归属表、契约针）—— 已做 |
| 代价 | **永久留一个死字段与一个死地址** | 一次性同步；`static_assert` 由跳闸器改为 32 |

**拍板理由**：那条 `static_assert` 是**跳闸器**（头注原话"改字段必须同步改断言并回写计划文档"），不是"行宽必须 40 B"的契约；
A1 想省的（不动字段偏移、不重编 shader）在本仓按铁律本来就要清整树 obj + 挪缓存，实际省不到；而代价是一个永远为 0 的死字段 —— 与"零兼容层"口径冲突。

### 10.3 对原档的三处订正

1. 原 §3.1「删该 4 字节字段 ⇒ `sizeof` 降为 **36**」→ **实测 32**（8 字节对齐：删掉 4 B 后剩 32 B 且天然对齐；本批第一轮即按 36 写，被 `static_assert` 当场抓住）。
2. 原 §3.3 列的针行号（3588 / 3601 / 3624 / 6511）是**改动前**的行号 ⇒ 这些针已随本批删除；并按原档建议**升级为禁复活针**（`needle + true`）——**已落地 10 条**：
   `RenderItemDataStorage` / `DrawItemIDStorage` / `addr_global_render_items` / `addr_draw_item_ids` / `render_item_handle` / `GetRenderItemHandle` / `GetRenderItemStorage` /
   `SetRenderItem4ID` / `ResolveRenderItem`，覆盖 `Context.h` / `WorldAddresses.h` / `scene_ubo.glsl` / `MaterialRuntimeTable.h` / `RenderItem.h` /
   `PrimitiveBatchPipeline.cpp` / `RenderPrimitiveCollectSystem.cpp` / `entity_gpu_cull.glsl`。
   **同一轮被一条既有针当场抓住**：Test 38 报 `slot 行宽冻结断言缺失 … static_assert(sizeof(MaterialRuntimeSlot) == 40, not found`
   ⇒ 证明这条契约真在管（不是摆设）；已把该针与 `ComponentTypeTable.h` 的行宽注释一并改到 **32 B**，复跑通过。
3. 原 §Phase 1.4「简化 `WriteBatchIndexRows`，删除向池查询的回退分支，统一归口为从 `MaterialRuntimeRow` 提取」→ **升级为整块删除**（依据 §10.1 的补强）。
   本批的落地形态是"改读**实体 4-ID 表本 pass 那一半**"（等价、四示例摘要逐位不变）；若批 D 落地，则整块连同取数一起消失。

### 10.4 候选批 D：删 CPU 行表填充（待拍板）

- **前提**（已在代码核实为铁律）：非接管批**不画**（T4c4-3）⇒ `WriteBatchIndexRows` 只跑"不画的批"。
- **范围草案**：`WriteBatchIndexRows` 的 `l2w_index` + `MaterialInstanceAddresses` 两列写入与其 buffer 分配 / 释放分支；
  `MaterialBatch` 的 `own_material_data_rows` / `material_data_index_rows_buffer`（若接管切换已不需要"先有 CPU 表"）；`GPUCullSystem` 里的对应释放分支。
- **判据**：四示例摘要逐位不变 + 门 / 测试全绿（同本批口径）+ 一条"非接管批不画"的红的样子。
- 待拍板后立施工单。

### 10.5 原档的验收矩阵（收敛 + 本批实测）

| 判据 | 目标 | 本批实测 |
|---|---|---|
| 全量编译（清整树 obj + 挪缓存后） | 0 error（warning 以本批不新增为准） | Debug / Release `rc=0`、`error C/LNK/MSB` = 0 |
| `static_assert(sizeof(MaterialRuntimeSlot) == 32)` | 成立（A2 口径） | 成立 |
| `sizeof(WorldAddresses) <= kWorldAddressesSlotStride` | 成立 | 成立（7 字段） |
| `ctest -C Release -L ecs` | 全过 | 11/11（原 12：`TestRenderItemDataStorage` 已删） |
| `TestCSMIncrementalPass` | 全过 + 禁复活针命中 | 1/1（10 条禁复活针；行宽针 40 → 32 B） |
| `ShaderResourceSchemaRegressionGate` | 通过 | 45 PASS / 0 FAIL |
| `-L shadergen`（含 `ShaderCookSmoke`） | 通过 | 13/13 |
| 示例：0 VUID + 帧摘要 | 与基线逐位一致 | 四例逐位不变（PBRSpheres / AlphaModes / LoadScene / CascadeShadowMap） |
