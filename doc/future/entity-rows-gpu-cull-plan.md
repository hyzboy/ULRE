# 实体行驱动 GPU 剔除（T3 消费者先行）—— 设计 + 拍板点 + 任务分解

> **⚠ 历史记录档**：本文写于对应日期，文中提到的已删符号（`PrimitiveComponent` / `MaterialComponent` / `RenderableComponent` / `TransformComponent` / `VisibilityComponent`·`VisibilitySystem` / `component_mask` / `RenderItemDataStorage` / `AssignTransformIndices` / `PositionSourceSpec`·`TransformPolicySpec` 等）按**历史口径**读。
> 现行权威：`doc/ecs-layer-architecture-and-frame-flow.md`（ECS 层主档；§2.1 对象模型 / §5 组件面）＋ `doc/visibility-authority-chain.md`（可见性）＋ `doc/backlog.md` §2（逐条落地记录）。

> 版次：v1（2026-10-03）。**本稿只做设计与拍板，不含代码改动。**
> 起因：用户拍板「**先把消费者定下来并同批做**」——`EntityGPU` 派生表（设计稿 T3）不再单独建，
> 而是与它的第一个真消费者「**剔除 shader 读实体行决定画谁**」一起落地。
> 关联真源：`doc/future/entitygpu-and-ulrescene-design.md`（B8 设计稿：T1 已落结构体 / T3 派生表 /
> P-B8-2 落位 = 世界私有 / P-B8-6 容量 4096）、`doc/future/ULRE_FINAL_TARGET_v2_设计约束.md` §1/§3、
> `doc/world-addresses-and-camera-model-plan.md` §2（地址三档）、`doc/gpu-driven-4id-draw-item-plan-2026-09.md`。

---

## 0. 现状实测（本次核对，非估算）

### 0.1 `EntityGPU` 是死结构：全仓零读者零写者 —— **（前提已过期，2026-10-07 注：T3d..T3i 落地后实体行表已是 GPU 剔除的实际输入，本节保留作立项背景）**

```
grep -rn 'EntityGPU' inc/ src/ example/ ShaderLibrary/ src/Tools/
```
只命中：`inc/hgl/ecs/core/EntityGPU.h`（T1 的结构体 + 断言）、若干**注释**（`Entity.h` / `ComponentTypeTable.h` /
`MaterialAuthoringSideTable.h`）、探针的 sizeof/offsetof 打印、测试里的枚举追加注释。
⇒ 设计稿「0 消费者 = 投机 API」的前提**成立**，所以本稿的价值在于：**先把消费者定形，再让表随它落地**。

### 0.2 现有 GPU-Driven 通路（消费者要复用/替换的就是它）

**引擎侧（已就位）**：批次可被标为「GPU-Driven 托管」，托管后**跳过 CPU 侧 ICB 与行表生成**、改用 GPU 写的计数：

| 落点 | 事实 |
|---|---|
| `inc/hgl/ecs/core/MaterialBatch.h:71` | `bool gpu_driven_override`（**跳过 CPU 侧 ICB 和行表生成**） |
| `MaterialBatch.h:53-54` | `icb_count_buffer` + `icb_count_buffer_offset`（**GPU-Driven 动态计数**，可选） |
| `inc/hgl/ecs/core/InstancedPrimitiveRenderItem.h:32` | `bool IsGPUDriven() const`（逐项声明自己是托管的） |
| `src/ecs/support/PrimitiveBatchPipeline.cpp:744-765` | 任一项 `IsGPUDriven()` ⇒ 该批 `gpu_driven_override = true`，并把 `icb_count_buffer` 挂上 |
| 同上 `:350/453/475/604/657/795/842/927` | 托管批在行表生成 / 4-ID 写入 / 命令表等**八处**被跳过 |

**示例侧原型（`example/ApiValidation/ComputeFrustumCull.cpp`，829 行）** —— 它已经把「GPU 剔除」的
数据面与着色器跑通了，但**候选表仍是 CPU 建的**：

- `:94-99` `DrawItem4ID { transform_id, geometry_id, material_id, texture_id }`（4-ID 项）
- `:88-185` 内联 GLSL：`local_size_x=64`；pc = `vec4 frustum_planes[6] + uint candidate_count + uint mesh_group_count_x + uint64 addr_table`（`sizeof == 112` 有断言）
- `:121-138` **7 张表**（BDA 两级寻址，无描述符集）：candidates / world_matrices / geometry_aabbs /
  visible_items / draw_count / indirect_commands / visible_l2w_indices
- `:140-184` 主体：`candidates.items[g_idx]` → 读 `world_matrices.mats[item.transform_id]` 与
  `geometry_aabbs.boxes[item.geometry_id]` → 6 平面投影半径剔除（`r = dot(extents, abs(n*mat3(l2w)))`）→
  `atomicAdd(draw_count)` 紧凑输出 + `DrawMeshTasksIndirectCommand(mesh_group_count_x,1,1)` + `visible_l2w_indices`
- `:196-226` `GPUIndirectCountHookSystem`：把 compute 写的 `count_buffer` 挂到批次的 `icb_count_buffer`

⇒ **替换点唯一**：`candidates[]` 这一份「CPU 逐帧建的候选 4-ID 表」换成 **`EntityGPU` 实体行**
（实体自己说"我有哪些组件、各自在哪一行"），剔除 shader 从实体行**自己组装** draw item。

### 0.3 关键事实：`Transform` 的行号不是 `component_rows[Transform]` 能直接给的

`Transform` 没有组件对象（T2-c 后也没有池行）：它的 GPU 侧行号是 **L2W 行**，由「稳定槽位 + 段预算 + 帧槽」
算出（`静态: 1 + slot` / `动态: 1 + 预算static + frame × 预算dynamic + slot`，见 `ulre-ecs-layer` 记的 B1b 口径），
现有 CPU 侧算法在 `PrimitiveBatchPipeline` / `LineRenderPipeline` 里用
`TransformAccessor::GetID()` + `TransformSystem::TryGetTransformSlot()` 复现。
⇒ 实体行里的 `row[Transform]` 存哪个量，是本稿 **P-1**。

---

## 1. 目标与非目标

**目标**：让世界里的实体行（`EntityGPU`）成为 GPU 剔除的**唯一枚举源**：

1. 剔除计算**不再需要 CPU 建候选表**（无逐帧 per-item CPU 写）；
2. 剔除判定与 draw item 组装**都读实体行**：`type[CT]/row[CT]` → 几何行 / 材质行 /（L2W 行）；
3. 结果经**既有** `icb_count_buffer` + 间接命令机制喂给绘制（不新开绘制通路）；
4. 判据可实测：**GPU 计数 == CPU 参考剔除计数**（同帧同相机对拍），且像素与现状一致。

**非目标**：

- 不做 GPU-Driven 的**批次合并/排序**（批次仍由 CPU 收集产出，B3-iii 的「逐项自成簇」纪律不变）；
- 不做 `.ulrescene` 直载（设计稿 T4，前置 (b) 仍未满足）；
- 不改 Lines/Text 的收集与绘制语义（T2-c 定界不变）；
- 不做 material 定义级行空间（欠账 §2.9）。

---

## 2. 数据面：`EntityGPU` 世界私有表

### 2.1 布局与索引（沿用 T1 已冻结的结构体，不改形状）

`EntityGPU` = 128 B（`flags(4) + type[16](16) + row[16](64) + work_flags(4)` + 40 B 填充，`alignas(64)`）。

**索引方式（P-2）**：建议 **按 `EntityID.index` 直接索引**（表 = 槽位表）：
`addr = base + index * 128`；`flags & Active` 判在用。理由：
① 实体槽位本来就稳定（T2-a 块池 + `generation`），增删实体**不重排**；
② shader 端一次位运算即可定位，不需要 CPU 维护「紧凑活跃集」；
③ 与 `TransformDataStorage`、`MaterialRuntimeTable` 的"槽位表 + 空洞"口径同族。

### 2.2 每个槽填什么

| 槽 | 来源（CPU 权威） | 备注 |
|---|---|---|
| `flags.Active` | 实体在槽在用 | 实体销毁 ⇒ 清位（该行的其余字段不清理，靠 Active 判） |
| `flags.Visible` | `ECSContext::IsEntityVisible(entity)` | 与 CPU 同源（含祖先链上溯的结论） |
| `type[CT]` / `row[CT]` | `Entity::GetComponentMask()` / `GetComponentRow(CT)` | **已有**（T2-c：实体侧就是按类型行号表）⇒ 写路径是"抄一份"，不是新算 |
| `row[Transform]` | L2W 行号（**见 P-1**） | 唯一需要额外计算的槽 |
| `work_flags` | 留 0 | 等消费者定义（不造没人读的位） |

### 2.3 归属与下发（设计稿 P-B8-2 已拍：世界私有）

- 落位：**`WorldAddresses` 加一字段 `addr_entity_gpu`**（随 `pc_root.addr_world_addresses` 下发）。
- 连带面**五处**（改地址表的固定动作，漏一处就红，见技能 `ulre-ecs-layer`）：
  ① `inc/hgl/graph/ubo/WorldAddresses.h`（C++ 真源 struct）
  ② `ShaderLibrary/ubo/scene_ubo.glsl`（`buffer WorldAddressesRef` 镜像）
  ③ 门的两条 parity 用例（`W.world-addresses-struct-parity` / `W.world-addresses-field-ownership`）
  ④ `src/ecs/tests/TestRenderItemDataStorage.cpp` 的**三份内嵌 GLSL 夹具** + 逐字段 `static_assert`
  ⑤ `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` 的 `kAddressOwnershipTable`（登记"世界私有"）

### 2.4 容量与失败模式

- 容量 **4096**（设计稿 P-B8-6 已拍，与 Transform static 同口径）；超限 = **fail-fast**（项目 bug，不扩容）。
- 实体表一旦冻结即存档语义（行号/槽位不得重排）。

---

## 3. 消费面：剔除计算（消费者）

### 3.1 输入（全部 BDA，无描述符集）

| 输入 | 来源 |
|---|---|
| `EntityGPU[]` | **新增**（本稿的主角；地址经世界地址表） |
| `GeometryAABB[]` | 引擎 `GlobalSSBOType::GeometryAABB` 池基址（示例原型已用 `GetGPUBase(GeometryAABB)`） |
| L2W `mats[]` | 现有变换矩阵表（`TransformAssignmentBuffer` / 静态段 + 动态 ring；`row[Transform]` 即索引） |
| 视锥 6 平面 | 相机行（`CameraInfo`）已在世界表 ⇒ shader 可直接解出 6 平面，**不必 CPU 传**（P-3） |
| 材质/纹理 id | `MaterialRuntimeTable` / 每实例 slot（4-ID 的后两位；**P-5**） |

### 3.2 输出（复用既有机制）

`visible_items[]`（紧凑 4-ID）+ `draw_count` + `DrawMeshTasksIndirectCommand[]` + `visible_l2w_indices[]`
—— **与示例原型逐字段同形**，只把"输入候选"换成实体行。计数缓冲直接接到批次的 `icb_count_buffer`
（`MaterialBatch.h:53`）⇒ 绘制侧**零改动**。

### 3.3 主体逻辑（按实体行组装，取代 CPU 候选表）

```
for each entity slot i where (entity.flags & Active) && (entity.flags & Visible):
    geoid = entity.row[Geometry];         if (!geoid) continue;      // 无几何 ⇒ 不是绘制项
    trid  = entity.row[Transform];        if (!trid)  continue;      // 无变换行 ⇒ 无法放置
    l2w   = mats[trid];  aabb = boxes[geoid];
    <6 平面投影半径剔除，与示例原型同一算式>
    if (visible): slot = atomicAdd(draw_count); visible_items[slot] = {trid, geoid, mat, tex};
                  indirect_commands[slot] = {mesh_group_count_x,1,1}; visible_l2w_indices[slot] = trid;
```

---

## 4. 拍板点

| # | 议题 | 选项 | **推荐** | 影响 |
|---|---|---|---|---|
| **P-1** | `row[Transform]` 存什么 | ① **L2W 行号**（GPU 侧直接可用） ② `TransformDataStorage` 句柄（CPU 语义） | **①** | ②shader 还要再换算一次（且换算式含帧槽 ⇒ shader 得知道 mobility/预算）；①与现有 `MeshDrawParams.addr_transform_id` 同源 |
| **P-2** | 表索引 | ① **按 `EntityID.index` 直索引（槽位表 + Active 位）** ② 紧凑活跃集（CPU 维护） | **①** | ②给 CPU 增加一份需同步的映射，且增删要重排 |
| **P-3** | 视锥平面来源 | ① **shader 从相机行现解** ② CPU 每帧传 push constant | **①** | ②每帧多一次 CPU 计算与上传；①与"相机行已在世界表"的既有分层一致 |
| **P-4** | 表的**帧槽**语义 | ① 与相机行同款「世界私有 × 帧槽」多份 ② 单份 + 脏集合一次性写 | **已实测定案：选 ①**（份数 = `kWorldAddressesSlotCount` = 8 ⇒ 8 × 512 KB = **4 MB**，世界私有） | 剔除在**提交前**读表：若 GPU 侧读到的是"下一帧写完的同一份"，就会用错帧的行 ⇒ 需要按帧轮转或多份；相机行当年正是为此做成 16 槽 × 8 帧槽。<br>**T3c-3 实测（真世界 PBRSpheres，钉住一个可动实体）**：`row[Transform]` = `4097 → 8193 → 12289 → …`，**步长恒 4096 行（= 动态段预算）、周期 3 帧**。⇒ 行号**随帧槽变化** ⇒ **单份 = 跨帧别名**（帧 N 的行号被帧 N+1 的写入覆盖，GPU 读到的是别帧的 L2W 段 ⇒ 取错变换矩阵）；与相机行同款多份是正解。⚠ **不跟随**变换侧的环深度 3：那是**待查的既有条件**（若主帧只用槽 0..2 则 3 合理；若主帧会用到 3..7，则变换缓冲自身在部分槽上就是别名的），实体表不该去跟随一个可疑值 |
| **P-5** | 材质/纹理 id 的来源 | ① 复用 `MaterialRuntimeTable`（每实例 slot） ② 暂用 0（先只验证几何/变换剔除） | **①**，若改动过大则退 ② | 决定这个特性一次落地多少 |
| **P-6** | 消费者落位 | ① **引擎侧**（世界驱动系统 + `ShaderLibrary` 里的 cull 计算着色器） ② 示例侧（照抄 `ComputeFrustumCull`） | **①** | ②等于把"引擎能力"留在示例里，用户明确反对（示例只做装配） |

---

## 5. 任务分解（每项：内容 / 判据 / 回滚点）

| 任务 | 内容 | 判据（可自动判定） | tag |
|---|---|---|---|
| **T3a** | `WorldAddresses` 加 `addr_entity_gpu`（**五处连带面**全做） | 门 **42 PASS / 0 FAIL**（含两条 parity/ownership 用例）；`static_assert` 逐字段对齐 | `b8-t3a` |
| **T3b** | 世界侧实体表存储：容量 4096、按 `index` 索引、`Active/Visible` + type/row 抄写（**挂卸/可见性/行变更三处写入**） | 探针：建 N 实体 ⇒ 表内 `Active` 位数 == N、`row[Geometry]` 与 `Entity::GetComponentRow(Geometry)` 逐字节相等；`Entity` 销毁 ⇒ 位清零 | `b8-t3b` |
| **T3c** ✅ | 每帧下发：`addr_entity_gpu` 写世界地址表（**按帧槽偏移** = 基址 + 帧槽 × 每份字节）+ **脏行上传**（`UploadEntityGPUDirtyRows(frame_slot)`，脏行写进每一份；`GetDirtyCount()==0` 即一行不传） | 已达成：① 地址非 0 且 16B 对齐（真世界日志实证）；② 脏行计划三条（无脏 0 / 改 1 行恰好 1 行且槽号正确 / 清脏后 0）；③ 静态段机理（首写脏 1 / 同值重下发脏 **0** / 值变后脏 1）；④ 表切 8 份（4 MB）；⑤ 跨帧行号的帧槽相关性已实测坐实（见 P-4） | `b8-t3c` |
| **T3d** | 剔除计算着色器（引擎侧，`ShaderLibrary/`）：实体表 → visible/count/indirect | 门编译过；对拍：**GPU count == CPU 参考剔除计数**（同帧同相机，示例内双面打印） | `b8-t3d` |
| **T3e** | 世界侧驱动系统（dispatch + barrier/fence + 把 count buffer 交给批次） | 示例跑满帧、无 VUID、无 error；`策略判定对拍` 不一致 0 | `b8-t3e` |
| **T3f** | 示例装配 + **像素验收** | 像素与现状一致（同相机同场景）；剔除计数对拍不一致 0；GPU-Driven 批**逐项自成簇**纪律不破 | `b8-t3f` |

> 排序理由：T3a/T3b 与 P-4/P-5 无关（纯地址 + 纯 CPU 侧抄写），可立即开工；
> T3c 依赖 P-4，T3d/T3e 依赖 P-5。**每项自身可编译 + 判据绿**才进下一项。

---

## 6. 风险

| # | 风险 | 默认 |
|---|---|---|
| R1 | 表是**每帧可变**（可见性/行迁移）而剔除在提交前读 ⇒ 读到别的帧的行 | **实测坐实**（T3c-3：可动实体的 `row[Transform]` 周期 3、步长 4096 随帧槽轮转）⇒ 已按**帧槽多份**收口（8 份 × 512 KB = 4 MB） |
| R2 | `row[Transform]`（L2W 行）随 mobility/帧槽变化而变 ⇒ 写路径必须与 `SubmitTransformUpdates` 同帧同源 | 复用现有 `TryGetTransformSlot` 算法，不另造一份换算 |
| R3 | 4096 槽 × 128 B = 512 KB/世界；多世界 = 线性叠加 | 已拍不扩容；超限 fail-fast |
| R4 | 剔除结果与 CPU 收集的批次划分不一致（批次仍 CPU 收集）⇒ 可能"GPU 说可见但批次里没这项" | 首版把消费者限制在**同一批**（批内实体行驱动），批次划分保持 CPU 权威 |
| R5 | 改地址表漏一处连带面 ⇒ 门红/示例编译失败 | 五处清单逐条打勾（技能 `ulre-ecs-layer` 已列） |

---

## 7. T3d-1 落点（**已定稿，勿再重复侦察**）

### 7.1 引擎着色器落点（P-6 = ① 引擎侧）
- 引擎着色器库 = 仓库根 **`ShaderLibrary/`**（`2d/ common/ vertex/ shadow/ material/ ubo/` + `sampler.toml`）；
  生成器 = `src/ShaderGen/`（`glsl_module/`），编译器 = `src/Tools/GLSLCompiler`，门 = `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp`。
- 新的剔除着色器落 **`ShaderLibrary/<新主题目录>/…glsl`**（引擎资产），**不**照抄示例内联 GLSL。

### 7.2 着色器如何拿到实体表（**零 schema 改动**）
`ShaderLibrary/ubo/scene_ubo.glsl:124-134` 已有：
```glsl
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer WorldAddressesRef
{
    uint64_t addr_camera_info;          // 相机行表（行号 = 相机槽 × 帧槽总数 + 帧槽）
    uint64_t addr_global_render_items;  // 4-ID 渲染项表（世界私有）
    uint64_t addr_draw_item_ids;        // DrawItemID 压缩索引表（世界私有）
    // （2026-10-07：addr_sky 已随 sky 段整删 —— 本片段是当时形态）
    uint64_t addr_shadow;               // 世界 profile 的 ShadowInfo（每帧槽一份 ring）
    uint64_t addr_entity_gpu;           // 实体行表 EntityGPU（世界私有；GPU 剔除的枚举源）
};
#define world_addresses WorldAddressesRef(pc_root.addr_world_addresses)
```
- ⇒ 着色器直接 `world_addresses.addr_entity_gpu` 即得**本帧那一槽**的实体表地址（`pc_root` 本就指向本帧槽）；
- C++ 端同序同型：`inc/hgl/graph/ubo/WorldAddresses.h`（`addr_entity_gpu` 偏移 40）；槽常量
  `kWorldAddressesSlotCount = HGL_FRAME_SLOT_TOTAL`、`kWorldAddressesSlotStride = 128`（两份 `static_assert` 兜底）。

### 7.3 门的约束（写新结构前必读）
| 用例 | 位置 | 要求 |
|---|---|---|
| `W.world-addresses-struct-parity` | 门 `:2805` | 读 `WorldAddresses.h` 与 `scene_ubo.glsl`，`struct WorldAddresses` vs `buffer WorldAddressesRef` **逐字段同序同型**；漏加 = 静默回退默认材质 / 解引用错地址 |
| `S.global-addresses-struct-parity` | 门 `:2742` | 全局地址同款比对（跨世界池 vs 世界私有是**两张表**，别混） |
| 归属表 | 门 `:2880` | `{ "addr_entity_gpu", true }`：实体行表**世界私有** ⇒ **不得**提升到设备级表（否则重演"跨世界同行冲突"） |

描述符纪律：`ShaderLibrary/common/descriptor_macros.glsl` 是**生成物**（`DescriptorMacroGen --emit`，`BINDLESS_SET 0`），
新着色器 `#include` 它即可，**不新增 set/binding**（push constant + BDA 两级寻址，照 `example/ApiValidation/ComputeFrustumCull.cpp:120-228`）。

### 7.4 唯一尚未存在的结构对（T3d-1 必须先补）
- C++ 端已有：`inc/hgl/ecs/core/EntityGPU.h`（128 B 行：`flags` + `type[16]` + `row[16]`）；
- GLSL 端**尚无** `buffer EntityGPURef` ⇒ 必须新增，**两端逐字段同序同型**，并在门里**有对拍用例覆盖**
  （否则门绿也证明不了它没漂 —— 这就是"S.global-addresses-struct-parity 兜底"那条坑的同族）。

### 7.5 实施清单（按序，每步有判据）
1. **GLSL `EntityGPURef` 结构对**（与 `EntityGPU.h` 逐字段对齐）+ 门覆盖 ⇒ 判据 **门 42 PASS / 0 FAIL**；
2. **剔除着色器**（`ShaderLibrary/<cull 主题>/`）：读本帧实体表 + 相机行（解 6 平面，RH_ZO）+ 几何 AABB 池
   （`GlobalSSBOBufferRegistry::GetGPUBase(GeometryAABB)`）⇒ 写 `visible[]/count/indirect` ⇒ 判据 门编译过 + 42 PASS；
3. **宿主侧引擎管道/模块登记**（从世界取址，**不手填地址**）⇒ 判据 示例编译链接、跑起来无 VUID/无 error；
4. **（T3d-2）** CPU 参考剔除（同一套规则）+ 示例内双面打印 ⇒ 判据 **GPU count == CPU 参考计数**，不一致 0。

> 本文到此为止的落点信息**已足以直接开写**：不需要再侦察着色器库位置、地址通路、门的约束或缺失结构。
### 7.6 实现时踩到的引擎约定（2026-10-03 实测——T3d-1 首次尝试，改动已回滚，约定留存）

1. **新增 GLSL 必须带 `@ulre` 元数据块**（文件头，缺则门三条用例同时红）：
   ```
   // @ulre begin
   // @ulre name <name>
   // @ulre kind Utility        ← 库文件主流写法（44 个文件）
   // @ulre priority 0
   // @ulre end
   ```
   缺失时门只报笼统的 "provider registry directory scan reported parse errors" /
   "LoadDirectory reported 1 errors"，**不指出是文件头缺块**：
   受影响用例 = `H1.provider-resource-manifest` / `I.glsl-code-module-file-parse` / `MI.module-invariants`。

2. **门里钉了 shader 模块总数**（`src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp`）：
   `if (file_count != 69)`、诊断串 `"LoadDirectory expected 69 file modules, got "`、
   `const int expected_count = 69;` ⇒ **每新增一个 shader 模块，这三处必须同步 +1**，
   否则 `I.glsl-code-module-file-parse` 报 `expected 69 file modules, got 70`。

3. **C++ 结构取块不能用花括号配对**：成员初始化式（`= {}` / `= 0`）里也含 `}` 与 `=`，
   `find('}')` 会把块尾**提前**截断 —— 实测表现是"标记找得到、字段数 0"（`C++=0 GLSL=0`）。
   正确做法 = **逐行**判定：跳过以 `{` 开头行、遇以 `}` 开头行终止；每行去 `//` 注释、去空白、
   去 `=...` 初始化式、去结尾 `;`，最后才拆 `type name[array]`。

4. **结构对拍提取器必须做类型/长度归一**：C++ `uint32_t`/`uint8_t` ↔ GLSL `uint`/`uint8`；
   数组长度宏 `kEntityGPUSlotCount` ↔ `ENTITY_GPU_SLOT_COUNT` ⇒ 归一到同一符号再比较，
   否则两端恒不相等（假红）。

5. **门里既有的 `ExtractU64Fields` 不能照抄到 `EntityGPU` 上**：它先 `StripCommentTokens(text)`，
   再用**花括号配对**取块（`text.find('}', brace)`）+ 按 `"uint64_t"` 令牌扫描后续标识符 ——
   这套只对"成员都是 `= 0`"的结构成立（`WorldAddresses`）。
   `EntityGPU` 是 `uint8_t type[16] = {}`（初始化式里含 `}`）+ `row [16]`（含空格）⇒ 配对被**提前截断**；
   实测两种自写取块（花括号配对版 / 索引式逐行版，后者用 `(char)10` 无转义）在
   **标记确实能找到**（`cpp_marker_at=2451` / `glsl_marker_at=2002`，文件 4030 / 2627 字节，CRLF）的
   前提下**都产出 0 字段**（`C++=0 GLSL=0`）。
   ⇒ 重做时的正确顺序：**先加一行诊断**（打印取块后扫描到的前 3 行原文与首个 `find` 结果），
   拿到那一个数据再按"先归一去掉落 `= {...}` / `= 0` 初始化式、再定位 `};` 边界"的路子写；
   **不要**继续在逐行判定的细节上试探（本轮在此烧掉两轮）。

> 重做 T3d-1 时这四件**同一批次**做完（新 GLSL + 模块计数 + 门用例 + 提取器），
> 判据 **门 43 PASS / 0 FAIL** 才提交 —— 避免中途留下红树。

6. **`uint8_t` 数组在 glslang 可用**（2026-10-03 T3d-1 落地时实测，Vulkan SDK 1.4.363 glslangValidator）：
   前提是 `#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable`（缺它 = `syntax error,
   unexpected IDENTIFIER`）。`s1_luminance.glsl` 的旧注"glslang 不支持 uint8_t 数组"只对
   **无该扩展**的路径成立 —— 带扩展即可直写 `uint8_t type[16]`，不必 uint 打包绕行。
7. **scalar layout 的 GLSL 结构不会隐式补 alignas 填充**：C++ `alignas(64)` 的 128 B 行在
   GLSL 侧必须把 40 B 填充**写成显式成员**（`uint8_t _pad[40]`），否则 `rows[]` 的 ArrayStride
   = 88（载荷和）⇒ 第 2 行起错位。对拍方法：`glslangValidator -V -g` 编探针，
   `spirv-dis` 读 `OpMemberDecorate` 偏移 + `OpDecorate %_runtimearr_… ArrayStride`，
   与头文件 `offsetof` 断言族逐字节核。
8. **逐行取块器（§7.6 第 3 条路子）实测一次通过**：命中 `struct <marker>` 行进块、`{`/`}`
   行首判进出、每行去 `=…` 初始化式 → 拆 `type name[dim]` → 类型/长度归一。数组长度写宏名时
   值从同文件 `#define` 现读（门内不复制数字）；GLSL 侧 `#include` 探针须
   `#extension GL_GOOGLE_include_directive : enable` + `-I ShaderLibrary`。


---

## 8. 进度与交接（2026-10-03 收口；**新会话从这一节开始**）

### 8.1 已完成并提交（仅本地，领先上游 7，未推送）

| 任务 | 提交 | 判据（已跑过） |
|---|---|---|
| T2-c 主体（B8 前置：组件改「类型化对象池 + 实体按类型行号」） | `9b1084baa` | `[T0b]−[T0]` +195.4 B/+3.000 块 → **+33.8 B/+0.015 块** |
| T3a：`WorldAddresses` 加 `addr_entity_gpu`（五处连带面） | `a7d242174` | 门 **42 PASS / 0 FAIL** |
| T3b：世界私有实体行表 `EntityGPUTable`（128 B 行、4096 槽、三处写入） | `1d877788a` | 探针 `[T0c]` 四项全绿 |
| T3c-1/2a：GPU 侧 SSBO + 地址下发 + 脏行上传 | `f0095ef9e` | 地址实机非 0 且 16B 对齐；脏行计划三条 |
| T3c-2b（**B1**）：`row[Transform]` 写者归**变换侧** | `ae1a4e64c` | 门 42/0；示例 0 ERROR |
| T3c-3：表**按帧槽切份**（8 × 512 KB = 4 MB）+ 撤临时自检 | `4b4d071cd` | 门 42/0；就绪日志「512 KB／份 × 8 份」 |
| T3c-3 收尾：静态段机理判据 + 示例动静对照 + P-4 实测定案 | `af550ff49` | 探针 首写脏 1 / 同值重下发脏 **0** / 值变脏 1；像素目视一致 |
| T3d-1 落点与约定（纯文档） | `56cb90c32` / `fbab027bc` / `7ca4e6c64` | §7 落点定稿 + §7.6 五条实现约定 |
| **T3d-1（结构对）**：GLSL 镜像 + 门对拍用例 | `d66f54fe9` | 门 **43 PASS / 0 FAIL**；`E.entity-gpu-struct-parity` 绿；glslang 实测偏移 0/4/20/84 + `_pad`@88 ⇒ **ArrayStride 128**；探针 `[T0c]` 全绿；PBRSpheres `[ERROR]`=0 |
| **T3d-1a**：GPU 剔除着色器 + L2W 地址加到 WorldAddresses | `1d7db5a74` | 门 43/0；glslang 编译通过 |
| **T3d-1b**：引擎接入（GPUCullSystem） | `f0c7eb1a4` | 门 43/0；ULRE.ECS 编译通过 |
| **T3d-2**：CPU 参考剔除对拍系统 | `1425e43d4` | **判据达成（经 `5ec033ea8` 返工后）**：PBRSpheres 对拍 CPU=100 GPU=100 match=100 mismatch=0 |
| **T3e**：GPU 剔除结果驱动绘制系统 | `f8dc8a0f4` → **返工 `5ec033ea8`** | 首版无条件接管 ⇒ 全示例画空；返工 = 双重 opt-in + 接线四处断点（详见 §8.2b）；判据：门 43/0、[ERROR]=0、VUID=0 |
| **T3f-1（形态 A）**：GPU 幸存集回灌 `RenderItem::isVisible` | `220da961d`（主体）+ `ea73a0bad`（注释）+ `4b4990d84`（验收通路） | **像素验收达成**：GPU 剔除 vs CPU 基线帧 8 **逐字节全等**（3,686,400 B sha256 一致）；对拍 mismatch=0；门 43/0；详见 §8.2c |
| **T3d-3a（方案 A）**：实体 4-ID 表——剔除改读池口径 geometry_id | `9e0aa0ad1` | **判据修正**：旧 mismatch=0 系两端读同一错下标的假绿；修后 Parity 100/100/0 + 像素逐字节全等（hash 不变）+ Test 断言 48→64 纳入构建；详见 §8.2d |
| **T3f-2（形态 B）**：gather 输出直连接管绘制（含删除 GPUCullResultHookSystem） | `f2ed4fcd6` | 离屏对照（visible=14/100 真剔 86）像素 **三方逐字节全等**（CPU=A=B）；[T3f2] ENGAGED 正向判据；门 **44 PASS**（新用例 E.mesh-draw-params-head-parity，抓力实测）；详见 §8.2e |
| **T3g**：派发异步化 + 资源生命周期一次化（修句柄泄漏/stall；**形态 A 退役**） | 本批 | Release `gpu_dispatch` 1114→**~150us/帧（-86%）**；泄漏判据 `tracked_objects` 960 帧**恒 165**（旧实现随帧上涨）；VUID=0（交接沿排空修掉 vkFreeMemory-00677）；门 44/0；像素 CPU==形态B；详见 §8.2f |
| **T3h**：阴影接收事实的 GPU 通路（欠账①清；guard 阴影前提删除） | 本批 | 值链三段 **100/100**（非默认实体注入实测：idx=23 NoReceive、idx=74 bias=2.0）；门 44/0；ENGAGED reject=0；像素 CPU==B 全等；详见 §8.2g |
| **T3i**：③ 多批/混材质（单次派发 + 批标签/切片/每批计数；▸§8.6） | `6b6baa39f` | 多批实测 `ENGAGED batches=2/2 items=100 arena_rows=104`、`Σcount=100=global overflow=0 值链 100/100`、Parity 100/100/0、VUID=0、像素三方逐字节全等 |
| 测试床示例：`CoverageVariants`（同 def 两覆盖变体）/ `AlphaModes`（4 行 alpha 方式 × 7 档比例） | `4c0fd9c40` / `9a5505246` | 前者 2 批、后者 4 批；均带 `CV_*` / `AM_*` opt-in 与像素 dump |
| alpha 呈现链三修：dither 覆盖判定被 `#ifdef HGL_ALPHA_TEST` 吃掉 → 修守卫；曲面 alpha 写进输出（`HGL_ALPHA_OUTPUT`，仅 `alpha_blend` 用；A2C 特性已整体删除）；有效 alpha = `base_color.a × opacity_mask.r` | `6d3b2c95a` / `d7fb7b524` / `9dca1071e`（+`bc2a80ae0`/`2c9c7b81d` 示例侧） | 门 44/0；四排形态量化可分（空洞 0 / 0.14+中间调 28% / 0.73 / 0.66）；梯子实测 ≈ 理论；像素三方全等（混合排偶发顺序差异，见 §8.7 B1） |

### 8.2 落地件清单（T3d/T3e/T3f-1/T3f-2 全部达成；各节 = §8.2b/§8.2c/§8.2d/§8.2e）

- T3d 落地件（同批次四件）：
  ① `ShaderLibrary/ecs/entity_gpu.glsl`（`@ulre` Utility 模块；`EntityGPUData` 逐字段镜像
  `EntityGPU.h` + **显式 `_pad[40]`**，`EntityGPURef(rows[])` 经 `world_addresses.addr_entity_gpu`
  直索引；宏 `entity_gpu_rows`）；
  ② 门模块总数 69→**70**（三处同步）；
  ③ 门新用例 `E.entity-gpu-struct-parity`（逐行取块 + 类型/长度归一 + 步长 ==128 对拍；
  宏值从同文件 `#define` 现读、门内不复制数字）；
  ④ 取块器按 §7.6 第 3/5 条（`ExtractStructFieldsLineWise`，不用花括号配对）——
  §8.2 旧述"失败点收窄到一个待测数据"已消解：**逐行式路子实测一次通过**，未再试探。
- T3d-1a 落地件：
  ① `ShaderLibrary/ecs/entity_gpu_cull.glsl`（GPU 视锥剔除着色器，输出 `VkDrawMeshTasksIndirectCommandEXT` 格式）；
  ② `WorldAddresses` 加 `addr_l2w` 字段（世界私有，按帧槽切份）。
- T3d-1b 落地件：
  ① `GPUCullSystem`（每帧 dispatch 剔除着色器，管理输出 buffer）；
  ② 在 `DefaultSystems` 中注册。
- T3d-2 落地件：
  ① `CPUCullReferenceSystem`（CPU 侧参考剔除，每 60 帧打印对拍统计）。
- T3e 落地件：
  ① `GPUCullResultHookSystem`（在 `RenderFrameSync` 阶段将 GPU 剔除结果挂到 MaterialBatch）；
  ② 着色器输出格式改为 `VkDrawMeshTasksIndirectCommandEXT`（`groupCountX/Y/Z`）；
  ③ `GPUCullSystem` 使用 `IndirectMeshTaskBuffer` 替代 `DeviceBuffer`。
- 新实测约定（追加到 §7.6）：**`uint8_t` 数组在 glslang 可用**，前提是
  `#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable`（缺它 = 语法错误；
  `s1_luminance.glsl` 的"不支持 uint8_t 数组"旧注只对无扩展路径成立）；scalar layout 下
  GLSL 结构**不会**隐式补 alignas 填充 ⇒ 128 B 步长必须 `_pad` 写死（SPIR-V 对拍坐实）。

### 8.2b T3e 返工记录（2026-10-03，提交 `5ec033ea8`）—— **默认全局接管 = 画面全黑回归**

T3e 首版（`f8dc8a0f4`）把 `GPUCullResultHookSystem` 做成**无条件**把所有示例的批次
改挂到 GPU 剔除的 ICB/count buffer 上；而当时剔除管线本身还没跑通（`count` 恒 0）
⇒ `vkCmdDrawMeshTasksIndirectCountEXT(drawCount=0)` ⇒ **所有示例画面全空**
（用户用 RenderDoc 截帧定位）。返工要点：

1. **双重 opt-in（默认全关）**：`GPUCullSystem::SetEnabled`（dispatch + 对拍）与
   `SetDrawOverrideEnabled`（接管绘制）分离 —— 对拍只需要 dispatch 正确；接管绘制还要求
   per-batch 行表（l2w_index / mesh_draw_params / material rows）与剔除输出**同序**，
   那是 T3f 的设计题，不能绑在一个开关里。示例（PBRSpheres）只开 `SetEnabled`。
2. **`#version` 必须在 GLSL 文件第 1 行**：引擎 GLSLCompiler 插件按首行判断是否注入
   版本头；`@ulre` 注释块把 `#version` 挤到第 16 行 ⇒ 注入后出现第二个 `#version`
   ⇒ `'#version' : must occur first in shader`，且失败只表现为 `pipeline is null`。
3. **剔除着色器的 4 处静默断点**（修完 GPU count 才从 0 变 100）：
   ① `addr_l2w` **只有字段没有写者**（`SyncWorldAddresses` 没接）⇒ shader 解引用 0 基址；
   ② camera 行号写死 0 ⇒ 读到别的相机的视锥，须与 `pc_root.camera_row` 同式（`GetActiveCameraRow()`）；
   ③ push constant 字段顺序须与 GLSL `layout(push_constant)` **逐字段同序同型**（u64 前置）；
   ④ `row[Transform]` 槽号 = `ComponentType::Transform` = **1**，不是 0（`None = 0`）。
4. **`EntityGPUTable::Activate` 必须同时置 `Visible`**：派生表若不镜像
   `VisibilityDataStorage` 的默认可见态，就出现「真值可见 / 行表不可见」漂移 ⇒ 全剔空。
5. **`shaderInt8` 设备特性要显式启用**：`storageBuffer8BitAccess` 只管存储层，
   `uint8_t` 着色器算术声明 SPIR-V `Int8` capability ⇒ 缺 feature 时
   `vkCreateShaderModule` 报 `VUID-...-pCode-08740`（`VKDeviceCreater` 已接）。

判据现状：**PBRSpheres 剔除计数对拍 `CPU=100 GPU=100 match=100 mismatch=0`**（= **T3d-2 达成**）；
`[ERROR]=0`、`VUID=0`、策略判定对拍不一致=0；门 **43 PASS / 0 FAIL**；探针 `[T0c]` 全绿；
其余示例不受影响（管线默认关）。

### 8.2c T3f-1 落地记录（2026-10-04，提交 `220da961d` / `ea73a0bad` / `4b4990d84`）—— **接管绘制的落法被推翻重定：回灌 `isVisible`，不碰 ICB**

设计题（§8.3 旧稿）的答案与预想**不同**，两条被否的路线都是实际侦察结果：

1. **「ICB 归零式」被否**：幸存 item 经簇合并进**同一条** ICB 命令 ⇒ 归一条杀一片，
   归零粒度与剔除粒度不同阶。
2. **「直连 GPU 序接管」被否**（形态 B 的原设想）：`atomicAdd` 紧凑序**每帧不定**，
   而批行表（l2w_index / mesh_draw_params / material rows）是**固定 CPU 序** ⇒
   两者对齐需要新数据契约 + 改动批消费侧，风险面覆盖所有 GPU-Driven 示例。
3. **落定 = 回灌**：`GPUCullSystem::ApplyVisibleSetToRenderItems` 把 GPU 幸存实体
   index 表（Map 读回）展开成覆盖位图，逐项写 `RenderItem::isVisible`。
   **下游 batch 构建 / 行表 / ICB / 材质解析一行未动** ⇒ 无新契约、无序对齐问题、
   GPU-Driven 逐项自成簇纪律天然不破。代价：CPU 的 `PerformFrustumCulling` 仍跑
   （其结果被回灌**覆盖**，不是被替代）⇒ 本形态**不省 CPU 剔除**，省的是"判据归 GPU"
   这一步；真省提交是形态 B 的目标。

**执行序靠依赖钉死，不靠插入顺序碰运气**：构造函数里
`AddDependency<PrimitiveCullSystem>()` ⇒ 回灌恒晚于 CPU 剔除（本帧**最终**判据）、
恒早于 `PrimitiveBuildSystem`（跨 phase：`RenderCollect` < `RenderBatch`，天然满足）。

**首帧护栏（实测踩出）**：实体行表脏行上传与首次 dispatch **同帧** ⇒ 首帧
`survivors=0`，直接回灌 = **一帧黑屏**（实测首帧 `off=100`）。护栏：计数为 0 且
存在实体驱动 item ⇒ **本帧不动 isVisible**（CPU/GPU 同式 ⇒ 最坏按 CPU 判据画一帧，
不画错不画空）。

**opt-in 三档（默认全关，逐示例显式开）**：
`SetEnabled`（dispatch + 对拍）→ `SetApplyVisibleSetEnabled`（形态 A 回灌）→
`SetDrawOverrideEnabled`（形态 B 直连，**仍无人开**，留作占位）。

**像素验收通路**（示例内环境自门控，沿用 `AlphaTestShadow` 先例）：
`PBR_DUMP=<tag>` = t 冻结（几何逐帧确定）+ 第 8 帧装一次性
`SwapchainRenderTarget::SetInFrameReadbackHook`（帧内窗口 = 提交后、present 前；
帧外读 presentable image 会触发真 VUID）→ 落 A2B10G10R10 **原始 packed 字节**
（零转换，保住 10bit）→ `std::exit`；`PBR_DISABLE_CULL=1` = 基线对照组。
结果：**两次运行 3,686,400 B sha256 逐字节全等**（`changed_pixels=0/921600`）。

### 8.2d T3d-3a 判据修正（2026-10-04，提交 `9e0aa0ad1`，方案 A）—— **剔除读错池：旧 mismatch=0 是两端同源的假绿**

形态 B 侦察（子代理契约清单）揪出比接管更根本的错：剔除拿 `EntityGPU.row[Geometry]`
（= `Entity::component_rows[Geometry]`，**组件池行号**，每实体一行）当 AABB /
MeshDrawParams 全局池的 **asset geometry_id**（每 asset 一行，"一号双表"）用。
PBRSpheres 实测规模：组件行 1..100 vs 池 id 1..10 ⇒ 越界/错借他人 AABB ⇒
**剔除判据从根上是错的**；GPU 与 CPU 参考读同一个错下标 ⇒ 错得同源 ⇒
旧 `mismatch=0` 与像素全等都成立但**不证明剔除正确**（假绿）。

修法（拍板 A：新表，不改 128B 冻结设计、不动 handle 分配器）：
- **EntityRenderIDTable**（世界私有、容量同 4096、按 `EntityID.index` 直索引）：
  行型复用 `graph::RenderItemDescriptor`（16 B uvec4，4-ID 终态的既有形状 ⇒
  不造同类结构）；GLSL 复用 `RenderItemResolve.glsl` 的 struct/`RenderItemBufferRef`
  ⇒ 门模块计数 71 不变。脏行上传、帧槽切份（8 份）、生死钩（`DeactivateEntityRow`
  同入口清两表）与实体行表逐字同纪律。
- **写者 = collect 现算点**：挂在「几何行成立 ⇒ 本项进批」的同一判据块内
  （`RenderPrimitiveCollectSystem.cpp` draw_key 填充处），四元组与喂给
  `SetRenderItem4ID` 的值**逐字同源** ⇒ 无第二口径；表内只留"真会画"的实体。
- `WorldAddresses.addr_entity_render_ids`（偏移 56；表 48→**64 B**，步长 128 不变）。
- **顺带纠史**：`TestRenderItemDataStorage` 的 `sizeof(WorldAddresses)==48` 断言在
  加 `addr_l2w`（T3d-1a）时就该红——它没红是因为该 target 不在本线构建里。
  本次改 64 并纳入验证（test 编译+全绿 = 断言真的在跑）。

新踩两条坑（都进注释/§7.6 口径）：
1. **include 顺序 = 宏展开时机**：`RenderItemResolve.glsl` 的**函数体**引用
   `world_addresses` 宏 ⇒ include 必须在 cull 文件的 pc 宏覆盖**之后**；写前面 =
   宏仍指未声明的 `pc_root` ⇒ `'pc_root': undeclared` ⇒ pipeline null ⇒ GPU 恒 0。
2. **改 `Context.h` 成员布局 ⇒ 全量 purge**：部分 purge（只清 ECS/示例目录）时
   首帧段错误（`SwapchainRenderTarget` 崩在旧布局 TU 上）——记忆里的布局错位坑
   重演；`find build -type d -path "*.dir/Debug" | xargs -0 rm -rf` 全清后一次通过。

判据（修后）：Parity CPU=100 GPU=100 mismatch=0（两端读**正确**池 id）；
`PBR_DUMP` GPU 剔除 vs CPU 基线**逐字节全等**（hash 与 T3f-1 相同 ⇒ 真值路径
未动）；[ERROR]=0、VUID=0；门 43/0；探针绿；`TestRenderItemDataStorage` 绿；
默认关路径示例正常。

### 8.2e T3f-2 形态 B 落地记录（2026-10-04）—— **真 GPU-Driven：gather 四表直连 + CPU 剔除跳过**

设计被用户纠正一次后定稿：**零复制字段**（我曾提议把 vertex_count/index_count 等
抄进实体 4-ID 表——被否：MeshDrawParams 池行本来就有，抄 = 第二真源 + 漂移面）。
最终 gather 每字段读真源：X 组数从**池行头部 9 字段镜像**现算（`_pad[92]` 保 128B
步长；门 `E.mesh-draw-params-head-parity` 对拍 X 列表 ↔ 镜像，漂移即红，抓力实测过）；
材质两位 = 实体 4-ID 表（T3d-3a 已上线 ⇒ gap #1/#2 正式拆掉）；阴影两位写 0 =
消费端语义（`pcf_shadow.glsl:180`：`<=0 ⇒ 1.0`、`flags0 ⇒ 受影`）与 CPU 缺省行**语义
等价**（字节不同，别写成"逐字节一致"）。

接线定稿（关键=时序与生命周期，两处与 §8.3 旧案不同）：
1. **接管点不在 hook 系统**：原 `GPUCullResultHookSystem`（RenderFrameSync）与批的
   跨帧生命周期/`Clear()` 复位不匹配 ⇒ **删除**。改为
   `PrimitiveBatchPipeline::TryApplyGPUCullOverride()`——`FinalizeBatches()` 开头、
   `FinalizeBatch` 之前逐帧 guard（此时批已含本帧全项）；通过后
   `GPUCullSystem::ApplyGPUCullOverrideHook()` 只做"指向+置标志"（校验与接线分离）。
   交接帧（CPU→GPU）先按 `~MaterialBatch` 同口径释放批自有 buffer ⇒ 无泄漏。
2. **CPU 剔除跳不跳 = 上一帧 guard 结果**：批成员资格在 build 按 `isVisible` 过滤、
   guard 在批建成之后 ⇒ `RunCulling` 读 `IsGPUCullOverrideActive()`（帧号记账）。
   稳态批=全项 ⇒ renderer 的 `maxDrawCount`（=CPU DrawBatch 数）是真上界不截绘制；
   交接帧最坏=一帧过画（CPU 账继续），不画错。
3. **guard 前提清单**（一条不满足 ⇒ 不接管 + 一次性报错，绝不静默）：opt-in/主帧
   pass（实测本场景每帧仅 1 批但 shadow 批共用 materialBatches ⇒ 显式挡）/恰好单批
   （R4）/全 mesh/非 lines/非实例化/批容量≤4096/全有 owner 实体/4-ID 行在/池行在/
   **非 meshlet**（`addr_meshlets!=0` 拒——X 与 CPU `GetMeshletCount()` 分支不同源）/
   **阴影全默认**（`CanReceiveShadow && bias==1.0`——ShadowProxy 是 CPU-only 组件，
   GPU 不可推 ⇒ 挡在接管外）/首帧 survivors>0 护栏。

判据实据（相机覆盖 env：`PBR_PITCH/PBR_YAW/PBR_DIST/PBR_TX/PBR_TZ`，dump 模式生效）：
离屏角度 `visible=14/100`（真剔 86）下 `[T3f2] ENGAGED items=27 survivors=14` +
CPU 基线 == 形态 A == 形态 B **3,686,400 B 逐字节全等**（三方同 sha256）；Parity
100/100/0；[ERROR]=0 VUID=0 reject=0；策略对拍 0；门 44/0；探针绿。
三档语义再分家：形态 A=回灌 isVisible（CPU 建表）；形态 B=gather 直连（跳 CPU 剔除）。

### 8.2f T3g 重构记录（2026-10-04）—— **派发异步化 + 资源一次化；形态 A 退役**

性能对比（§8.3 第 1 条）测出的第一批数据同时暴露了派发路径的三个致命实现问题
（用户评审点名，全部核实成立）：每帧 `CreateComputeCommandBuffer`/`CreateQueue`
= **句柄泄漏**（引擎纪律是初始化一次 + `SAFE_CLEAR`，先例 `SwapchainModule.cpp:259-268`
/`TextureManager.cpp:29-30`）；每帧 `WaitLastSubmitFence` = **CPU-GPU 流水线自杀**
（旧数据 gpu_dispatch ≈1.1ms 的大头是 fence 等待与每帧资源创建，不是 GPU 剔除本身）；
加上台架每帧 Map 读回。另按用户合规清单：`<chrono>/<fstream>/<sstream>/<vector>/<set>`
换引擎设施（`hgl/time/Time.h::GetUptimeUs`、`io::OpenFileInputStream`），着色器
绝对路径改 `graph::mtl::GetShaderLibraryPath()` 单源。

重构四件：
1. **资源一次化**：`CreateDispatchResources()` 建一份队列（fence 环深 = 帧槽数）
   + 8 槽 cmd 环（`frame_index % HGL_FRAME_SLOT_TOTAL` 取用，同槽重录隔 8 帧 ≥
   在飞深度 ⇒ 必已执行完，与 swapchain per-image cmd 同假设；pool 带
   `RESET_COMMAND_BUFFER_BIT` 已查实 `VKDeviceCreater.cpp:146`）；析构显式释放。
2. **计数清零进 cmd**：`FillBuffer(draw_count,0,4,0)` 替代 CPU `Write`。
   ⚠ 两个连带的坑（评审点名，都写进注释）：① **批内无隐式序**——`vkCmdFillBuffer`
   属 TRANSFER，紧随的 compute 原子读写必须显式 TRANSFER→COMPUTE barrier（同队列
   submission-order 只救**跨批**，不救批内）；② `draw_count_buffer_` 的 usage
   **必须补 `TRANSFER_DST`**（VUID-vkCmdFillBuffer-dstBuffer-00031，建缓冲时定死）。
3. **提交即返回**（删每帧 fence）。正确性依据：cull 与主帧**同一条 VkQueue**
   （`CreateQueue` 包 `attr->graphics_queue`）⇒ 同队列 submission order 规范条款
   （§6.4 "execute in order … all previously submitted batches have occurred"）
   保证 [本帧 cull → 主帧 draw] 的可见性，**不需要**跨队列原语。
   派生判据：`guard` 的"派发已定帧"改纯 CPU `dispatch_count_ >= 2`（场景切换/
   `SetEnabled(false)` ⇒ 计数归零，重开必须重新攒满——防坑 3），稳态生产路径零同步。
4. **形态 A 整体退役**（评审第 3 条"倒退设计"的直接结论）：`SetApplyVisibleSetEnabled`
   /`ApplyVisibleSetToRenderItems`/每帧 Map 读回全删——回灌的存在理由就是 CPU 读
   GPU 结果，与异步化不相容；其验证价值由对拍台架覆盖。对拍台架改**每 60 帧一个
   采样窗口**且窗口内先 `SyncForParityReadback()`（fence 成本只在采样帧付出），
   比对用**定长位图**（2×4096 B，替代 std::set/algorithm）。

**新踩坑（异步化引出，实测 VUID-vkFreeMemory-memory-00677）**：CPU→GPU 交接沿
guard 要 `delete` 批自有 ICB/行表，而它们仍被**上一帧主帧 cmd** 引用——自家 fence
只覆盖自家提交（各批"按序开始、可乱序完成"）⇒ 等自家不够；修法 =
`DrainForHandover()`（`WaitQueue` = vkQueueWaitIdle，同一条 graphics 队列 ⇒
覆盖主帧引用），且只在这次交接**真有自有 buffer 要释放**时付出，稳态零成本。

判据实据（Release，离屏场景）：`gpu_dispatch` **1114→~150us/帧（-86%）**；
**`tracked_objects` 960 帧恒 165**（泄漏的直接观测面，旧实现随帧上涨）；
VUID=0；像素 CPU 基线 == 形态 B **逐字节全等**（离屏真剔场景）；Parity 100/100/0；
门 **44/0**；探针/测试/默认关示例全绿。

**收益的正确表述（别再写错）**：100 球规模 GPU 路径（record+submit ~150us）仍贵于
CPU 剔除（~7us）——这条线在**当前场景规模**不省 CPU。它的收益是**规模无关性**：
派发成本不随实体数增长，CPU 剔除 O(n) 线性（7us/100 ⇒ 万级实体 ≈ 0.7ms+），
盈亏平衡在数千实体；且接管的前提价值（提交数与剔除数解耦、gather 数据面就绪）
已验证。**形态 B = 唯一 GPU 路径**；两档 opt-in（`SetEnabled` / `SetDrawOverrideEnabled`）
默认关纪律不变。

### 8.2g T3h 落账：阴影接收事实的 GPU 通路（欠账①清）—— 2026-10-04

**目标**：形态 B 的 guard 原挡"实体有非默认阴影参数（ShadowProxy GPU 行未接线）"
⇒ 非默认阴影实体不进接管范围。本批接线并**删除该前提**。

**落点：为什么住 `EntityGPU` 行**（备选逐个否掉）：
- 事实归属 = **实体级**（同 asset 的多个实体可各配 receive/bias），与 `Active/Visible`
  同类——那两个也住在行里；
- **4-ID 表不行**：它复用 `graph::RenderItemDescriptor`（16B uvec4），与**全局图元
  描述符池共用同一 GLSL 形状**（T3d-3a"不制造第二个描述符形状"契约）⇒ 塞实体事实
  会形状分叉，波及池的全部消费者；
- **材质池 / MeshDrawParams 池不行**：池行按 asset 索引（100 实体共享 10 行），粒度
  装不下 per-entity 值（§8.2d"一号双表"坑的反向）；
- **ShadowProxy 无 GPU 行**（T2-c：CPU-only）⇒ 必须"值进行"，塞池行号 GPU 无处可用；
- ⇒ `EntityGPU._pad` 划 8B：`shadow_bias_multiplier`@88 / `shadow_receive`@92，
  `_pad` 40→32，**128B 步长与 `alignas(64)` 不变**（填充是布局契约，划用不破契约）。
  SPIR-V 实测成员偏移 0/4/20/84/88/92/96 + ArrayStride=128，与 `offsetof` 断言族一致。

**写者 / 归一 / 消费**：
- 写者 = collect 的"本项进批"判据点（与四元组 `SetRenderIDs` **同一判据、同一行**）；
  `SetShadowFacts` **值变才标脏**（每帧都调——照抄 `SetVisible` 的无条件标脏会让静态
  实体永远脏行，违背「值变才标脏」纪律）；
- 归一 = `NormalizeShadowBiasToRow`（ShadowProxy.h **唯一出处**）：`bias<=0 / 非有限
  ⇒ 0`（消费侧视 0 为默认 1.0，pcf_shadow.glsl:180）；**CPU FinalizeBatch D3 / collect
  上行 / 台架判据三处共用**（原三处手抄分支已收敛）；
- 消费 = gather 写 mtl 行阴影两位：`{material, texture, entity.shadow_bias_multiplier,
  entity.shadow_receive==0 ? NoReceive : 0}`（**逐字节照抄** CPU 归一值）。

**判据：值链，不是像素**：
- 本示例无投影光 ⇒ 像素对 receive/bias **不敏感**——像素全等只是回归保护，
  **不构成接线证据**；
- 真判据 = 台架 `CheckShadowFacts` **三段值链**：CPU 访问器 → `EntityGPU` 行 →
  gather mtl 行，逐字节；
- 实测：`Shadow facts value-chain: 100/100 实体三段一致`；`PBR_SHADOW_FACTS=1` 注入
  两个非默认实体——idx=23（NoReceive：flags 1/0/1）、idx=74（bias=2.0：
  2.000/2.000/2.000）；
- 全套：门 44/0；探针/测试 RC=0；ENGAGED items=100 survivors=100（**reject=0**：
  非默认实体不再挡接管）；Parity 100/100/0；像素 CPU==形态 B（3,686,400 B，与 T3g
  基线同 sha256）；默认关运行 0 错 0 VUID。

**踩坑**：非默认实体日志首版拿 `bias != 0` 当判据，把 98 个缺省实体（accessor 返
1.0）全报 non-default——**对照物必须是"缺省约定"（receive / bias 1.0），不是零值**；
已修正为 `flags!=0 || bias!=1.0`。

### 8.3 下一步顺序（不要重做侦察：落点看 §7，坑看 §7.6/§8.2b–§8.2g；**T3d…T3i 全达成**（③ 多批/混材质见 §8.6））

1. ~~性能对比~~ **已完成（T3g，结论见 §8.2f"收益的正确表述"）**：
   `gpu_dispatch` 异步化后 ~150us/帧（旧实现 1.1ms 里大头是每帧 fence + 每帧
   CreateQueue/CreateCmd 泄漏，不是 GPU 剔除）；小场景（100 实体）GPU 路径仍贵于
   CPU 剔除（7us）——收益在规模无关性（派发成本不随实体数增长），盈亏平衡数千实体。
   ~~形态 A 的去留~~ **已裁决（T3g）**：整体退役；形态 B = 唯一 GPU 路径。
2. **放开接管范围的既有欠账**（形态 B 首版 guard 挡在外的，按欠账逐个补）：
   ① ~~ShadowProxy GPU 行~~ **已完成（T3h，见 §8.2g）**；
   ② ~~**meshlet 几何**（X 组数 = GetMeshletCount 分支）~~ **won't-do（2026-10-05 关闭）**：
      **本引擎不支持 meshlet**（2026-10-05 拍板）⇒ 引擎侧 meshlet 代码**整体删除**：
      池行 3 个 `addr_meshlet*` 字段（**行宽 128→104 B**）、GLSL 镜像 `_pad[92]→[68]`、门步长断言、
      `MeshShaderVertexAdapter` 的 `MeshletDescriptor`+3 个 `sbo_meshlet*` 宏、`VKGeometry` 的持有与
      访问器、`PrimitiveBatchPipeline` 的 X 组数分支与 guard 代理；loader 改**建对象前显式拒绝**。
      GLTFConvert（独立子模块，用户拍板保留产出能力）⇒ 终态是「工具能产出、引擎**响亮拒绝**」，
      不再是静默错渲（旧路径把 meshlet 数当 X 顶点组数 ⇒ 错渲且零 VUID、门全绿）。台账 §8.7 A1 同闭。
   ③ ~~多批/混合材质场景~~ **已完成（T3i `6b6baa39f`，见 §8.6；测试床 CoverageVariants/AlphaModes）**；
   ④ ~~实例化/多实例 item~~ **won't-do（2026-10-05 关闭）**：查实该能力**无真实使用者**——
      组件多实例的活调用方只有 A3 台架（它为「被拒」而存在）；`SetInstance4ID` 只有测试在调；
      collector 对多实例写的是「全部同 4-ID」；上游没有「每实例不同」的生产者 ⇒ 就算把 gather 的
      Y 写成实例数，画的也只是同一项原地叠 N 遍，**判据立不起来**。故按零兼容**整体删除**：
      `InstancedPrimitiveRenderItem` + 8 个自由函数 + slot 实例三字段（**行宽 48→40 B**）+ guard 该条
      + A3 台架的④类项（保留其「独立 program」半）。**CPU 侧自动合批（`DrawItemRange::instance_count`
      聚类）不受影响**——那才是活的实例化。存储侧 `AllocateContiguous/ReleaseContiguous` 保留
      （低层存储原语，Test 6 覆盖），只删掉 `PrimitiveState` 对它的包装。
3. **架构决策（2026-10-04 拍板）**：
   - **DGC 不在本线计划内**：开发机实测无 `VK_EXT_device_generated_commands`
     （Intel Arc 140T / 驱动 2025-01，`vulkaninfo` 查无）。形态 B **不依赖任何扩展**
     （compute + mesh shader + indirect count；引擎已按 `GetDeviceProc` 动态加载 +
     空检查 + fallback 的既有模式接入，先例 `VKDeviceCreater.cpp:690-693`）。
     将来若真要用，只需插"执行层"（见下），决策层/校验层零改动。
   - **CPU 渲染路径的处置**：在 ②③④ 覆盖完真实场景之前，它是形态 B 覆盖不到的场景的
     **唯一出路** ⇒ 保留；等形态 B 在真实场景全集（多批/混材质/meshlet/实例化/阴影）
     判据全绿后再删，或按 RELEASE 宏屏蔽（CPU 参考判据不受影响）。
   - **数据取回校验常驻**：CPU 参考实现（纯计算、不发命令）+ 台架采样对拍（fence +
     Map，每 60 帧一个采样窗口，生产路径零同步）是"画面错了能查"的唯一手段 ⇒ 不受
     渲染路径增删影响。覆盖：剔除判据 / 批次行表 / 命令流三类；管线·描述符·像素错
     仍需 RenderDoc 做最终视觉确认。
   - **分层契约（换任何执行器都不白做）**：决策层（compute 产可见集 / 批次分配 /
     行表）→ 执行层（现 = CPU 按决策表录 indirect count 绘制；将来可换 token 流）→
     校验层（读决策层对拍）。

### 8.4 已知的既有待查项（与本线无关但影响未来 GPU 剔除正确性）

- ~~变换侧 L2W 环深度 = 3，而帧槽空间 = 8~~ **已闭**：`HGL_L2W_RING_FRAMES` 现定义为 `HGL_FRAME_SLOT_TOTAL`
  （`inc/hgl/common/RenderOptions.h:33`）⇒ 环深度 ≡ 帧槽空间，不存在别名槽。实体表同按帧槽空间切份。

- ~~主帧车道（A1 timeline 信号量）**从未生效**~~ **已闭（2026-10-05）**：车道在 `SwapchainRenderTarget`
  构造里创建，而构造期 `ecs_context` 尚未绑定 GPU 设备——`ECSContext::Initialize(device, target)` 的
  **入参正是本 RT**（RT 必须先存在）⇒ 那一刻 `IRenderTarget::GetDevice()` 恒 null ⇒ 车道恒 null
  ⇒ `src/ecs/core/Context.cpp`（离屏 `RenderTo`）的「prepass await 上一次主帧车道值」被**静默跳过**，
  即「prepass 覆写阴影前，在途主帧已读完」这条保证**自 A1 落地（`2029ab516`）起就没生效过**。
  - **反证**：同一 API 在 `RenderTargetManager` 给 4 个级联 RT 建车道**全部成功**（跑 `CascadeShadowMap`
    实测，零失败日志）⇒ 不是驱动问题、也不是换机带来的；差别只在设备指针来源
    （RT manager 用自存 device，swapchain RT 用 `ecs_context->GetGPUDevice()`）。
  - **修法**：改为首次使用时 `EnsureMainLane()` **延迟创建**（`GetMainLane()` 亦经它），并加两条一次性
    正向判据——`[SwapchainRenderTarget] 主帧车道（A1 timeline 信号量）就位（延迟创建）` 与
    `[A1] 离屏提交挂上主帧车道等待 v=N`（缺后者即「锁没挂上」，此前正是如此）。
  - **语义依据（与「阴影要不要分帧槽」绑定）**：车道是**单调递增值**，等「最新值」即可覆盖所有更早的在途
    主帧（同队列提交序 ⇒ 值 N 完成蕴含 N-1… 已完成）⇒ **单个 timeline 车道足以保护跨帧共享的阴影纹理**
    ⇒ 阴影**不需要**按帧槽分份（分份还要为 CSM1..3 的稀疏写付每帧显存 + 按份维护静态缓存状态机，
    且离屏槽预算仅 4 槽、4 级联×2 槽装不下，会牵动 `HGL_FRAME_SLOT_TOTAL` 与 L2W 环深度不变式）。
### 8.5 ③ 多批/混材质设计（T3i —— **已实施，定稿与落地见 §8.6**；本节保留拍板前的方案推演）

**现状缺口**：guard 的"恰好一个含几何批（R4）"前提把形态 B 锁死在单批场景——
真实场景几乎必然多批 ⇒ 不解决则形态 B 在真场景一律 reject，CPU 渲染路径也永远删不掉。

**侦察结论（关键结构事实，实施前不必重查）**：
- 批 = `ShaderProgramPipelineKey`（`cache.materialBatches` 的 map 键；shadow 批与主帧批**共用**该 map）；
- 每批自带：`icb_mesh_tasks`（multi-draw ICB）、`icb_count_buffer(+offset)`、`mesh_draw_params_buffer`
  （per-draw 行：`{geometry_id, first_instance}`）、`l2w_index_buffer`、`material_data_index_rows_buffer`；
- mesh 侧消费链（发射器生成，`MeshTemplateEmitter`）：`cmds[gl_DrawID].geometry_id/first_instance`
  → `#define gl_InstanceIndex (draw_params.first_instance + gl_WorkGroupID.y)`
  → `ResolveTransformID(gl_InstanceIndex)` 查 `addr_l2w_index`；`MTL_ROW(dataIndex)` 查 `addr_mtl_data_addrs`；
- 批内 draw = 一次 `DrawMeshTasksIndirectCount(icb, first*sizeof(cmd), countBuf, countOff, maxCount)`
  （`PipelineMaterialRenderer::ProcIndirectRender`）；**段偏移支持现成** ⇒ 多批只需给每批不同的
  `first`/`countBuf+off`/`maxCount`；
- 接管 hook 现在把批的五个指针整体指向系统单套表（`ApplyGPUCullOverrideHook`）。

**设计定稿（2026-10-04 拍板：1A + 2β + 3A）—— 共享竞技场 + 按批切片**

⚠ 两处"落地形式"修正（查实现所得，架构不变）：
1. **批标签不进实体行**：实体行是 **8 份镜像 + 帧起点才刷新**（`SetFrameIndex →
   SyncWorldAddresses → UploadEntityGPUDirtyRows`）⇒ 帧内（批装配）写入要等**下一帧**起点
   才进镜像；而 `BatchInfo` 是 CPU 直写的新值 ⇒ 两者不同源 = 正是本设计要消灭的错配。
   ⇒ 标签走**系统自有的 per-entity u32 数组** `batch_tag`（CPU 批装配时直写当前帧槽、
   cull 直读，地址经 push constant 下发）。**不动 `EntityGPU._pad`**。
   原理性区分（记牢）：**内容类**值（阴影 receive/bias）滞后一帧无害；**结构类**值
   （批归属）必须与**本帧绘制**同源，滞后 = 切片错位。
2. **新增缓冲按帧槽分份、只写当前份**：否则本帧 CPU 直写与**上一帧仍在飞行的 cull**
   竞争（同队列提交序只保证"新提交批次读到旧批次写"，不覆盖"CPU 直写 vs 在飞 GPU 读"）。
   与引擎 UBO/实体表分份同理由。

**（下述原始设计条目按此两条修正后生效）**
1. **竞技场**：系统仍持 4 张表（icb 12B/行、cmds 8B/行、l2w 4B/行、mtl 16B/行），容量仍
   `kMaxVisibleEntities`——因为**每项只属一批**，全场景 item 总数 ≤ 4096 ⇒ 一块地够全部批分摊；
2. **切片**：批 b 得行区间 `[base_b, base_b + cap_b)`，`base_b` = 前序批 item 数累计、`cap_b` =
   本批 item 数（`base` 顺序 = CPU 遍历 `cache.materialBatches` 的稳定顺序，须与写 `batch_index`
   同一顺序）；
3. **批身份上 GPU**：`EntityGPU._pad` 再划 4B 放 `batch_index`（@96，`_pad` 32→28；沿用 T3h
   先例，零新表零新地址）；写者 = 批装配（与批行表同判据点）；消费 = 剔除着色器分流；
4. **每批输入 + 批标签**：`BatchTag[帧槽][4096]` u32（`0xFFFFFFFF` = 不属任何批 ⇒ 跳过；
   见修正 1）+ `BatchInfo[]`（`{u32 row_base, u32 capacity}`）+ `BatchCounters[]`（含末位
   overflow 槽；`FillBuffer` 在 cull cmd 内清全部，照 T3g 批内 barrier 纪律）；
   三个地址 + `batch_count` 进 cull push constant（现 88B → ~116B，仍在 128B 保底内）；
   **按批粒度接管**：未接管的批其实体标签留无效 ⇒ cull 不为其写行、CPU 路径照旧绘制
   （单次派发即得按批粒度，无需每批一次 dispatch）；
5. **着色器分流**：`b = entity.batch_index`；`b >= batch_count || batch_index 无效` ⇒ 跳过；
   `slot = atomicAdd(counter[b], 1)`；`slot >= cap_b` ⇒ 溢出（跳过 + 计数上报，见决策 3）；
   行号 `r = base_b + slot`，四张表写 `r`；
6. **绘制接线**：每批的 `first` = `base_b`（命令序号）、`maxCount` = `cap_b`、
   `countBuf/off` = 共享计数器 `+ b*4` ⇒ **GPU 计数直接当每批绘制数**（形态 B 的既有机制）；
7. **push 地址切片偏移**：批 b 的 `addr_batch_mesh_draw_params/addr_l2w_index/addr_mtl_data_addrs`
   各加 `base_b × 行字节` ⇒ 着色器里 `gl_DrawID`/`gl_InstanceIndex` 语义**不变**（切片内 0 起），
   故**材质与 mesh 发射器零改动**。

**一致性（本设计的关键约束，必须写进实现注释）**
- 事实分三类（查实现所得，勿凭直觉推广）：
  ① **实体行表 / 4-ID 表** = CPU **直写 host-visible 镜像**（`StructView::Data()`，非
  `vkCmdCopyBuffer`），刷新点在**帧起点**（`SetFrameIndex → SyncWorldAddresses →
  UploadEntityGPUDirtyRows`）⇒ cull 读到**本帧**值；
  ② **L2W** 走 staged → `RenderBufferUpload`（`vkCmdCopyBuffer`，在主帧 cmd 里、提交于
  cull 之后）⇒ cull 读到**上一帧**矩阵（既有语义：运动物体剔除滞后 1 帧）；
  ③ 新增的 `batch_index` 由**批装配**（RenderBatch，**晚于** cull 的提交）写 ⇒ cull 读到的是
  **上一帧**值；
- ⇒ 为让「批划分」与「batch_index」同源，`BatchInfo` / 切片基址也必须取**上一帧**布局；
- 接管前提加一条：**本帧批布局 == cull 所用（上一帧）布局**（比较有序的 `(pipeline_key,
  item_count)` 列表）；不等 ⇒ 本帧不接管（CPU 账），本帧布局记为下一帧 cull 用 —— 
  牺牲布局变化那一帧，换"切片与行内容恒定同源"；
- 好处：**无需移动任何系统相位**（cull 仍在 RenderCollect，hook 仍在 FinalizeBatches）。

**判据**
- 多批测试场景：`PBRSpheres` 加 env 门控的第二材质变体（管线键不同 ⇒ 第二批），默认关；
- 台架扩展：竞技场行归属校验——批 b 的 `[base_b, base_b+count_b)` 行内实体的 `batch_index`
  必须 == b（CPU 侧可查）⇒ 把"分流正确"钉在值链上，而不是只看像素；
- 既有判据全保留：门 44/0、Parity、值链三段、像素 CPU==B、VUID=0、reject=0。

**改动面（文件级）**：`EntityGPU.h`（+batch_index）、`entity_gpu.glsl`（镜像）、`EntityGPUTable.h`
（写者）、批装配点（写 batch_index）、`entity_gpu_cull.glsl`（分流）、`GPUCullSystem.h/.cpp`
（BatchInfo/Counters/切片接线/布局稳态）、`PrimitiveBatchPipeline.cpp`（guard 多批 + 布局比较）、
`MaterialBatch.h` + `PipelineMaterialRenderer.cpp`（切片 first/maxCount/地址偏移）、
`CPUCullReferenceSystem.cpp`（归属校验）、`PBRSpheres.cpp`（第二变体）。

**待拍板**（→ 已于 §8.6 拍板并落地：1A 改走系统自有标签数组 / 2β 相位后移 / 3A 跳过+计数）
1. `batch_index` 住 `EntityGPU._pad`（推荐，零新表）还是新建每实体批表？
2. 布局变化那一帧退回 CPU 账（推荐：简单且判据干净）还是强行同帧重建切片？
3. 溢出策略：`slot >= cap_b` ⇒ 跳过 + 计数器上报（推荐：可见且不崩）还是 fail-fast 报错？

---

## §8.6 T3i 落地：③ 多批/混材质 GPU 剔除（定稿 1A + 2β + 3A）

**状态**：实现完成（2026-10-04）。批定义 = `ShaderProgramPipelineKey`（`{ShaderProgram*, Pipeline*}`，
只比两个指针；材质**实例**不改变批号）。

### 定稿（用户拍板 1A + 2β + 3A，含两处落地修正）

| # | 拍板项 | 定稿 | 落地修正（查实现后） |
|---|--------|------|----------------------|
| 1A | 批归属标签 | 单次 cull 派发 + 每实体批标签 | **不进 `EntityGPU._pad`**：实体行有 8 份帧槽镜像且帧起点才刷新 ⇒ 帧内写入滞后一帧，与 `BatchInfo`（CPU 直写）不同源。改走系统自有 `batch_tag[帧槽][4096]`（CPU 批装配直写当前槽、cull 直读，地址经 push constant 下发） |
| 2β | cull 相位 | `RenderCollect` → `RenderFrameSync` | 使**标签 / 切片 / 绘制**三者本帧同源，无"布局稳态"前提；把隐式注册顺序依赖变成显式 `AddDependency`。**不解决** L2W 一帧滞后（内容类值滞后无害） |
| 3A | 切片溢出 | `slot >= capacity` ⇒ 跳过 + `overflow` 计数 | 台架把 `overflow != 0` 当**硬判据**（非零即红）+ guard 前置容量检查（超限不接管） |

### 原理分界

- **结构类值**（批归属/切片/计数）必须与本帧绘制**同源** ⇒ 帧槽分份 + 相位后移；
- **内容类值**（阴影 bias/receive、材质两位）滞后一帧无害 ⇒ 沿用实体行通道（T3h）。

### 数据通路

```
批装配（RenderBatch = 系统相位 RenderBatch）
  ├─ guard：逐批评估前提 ⇒ 分配批号 b、切片 [row_base, row_base+capacity)
  │     · 写 batch_tag[帧槽][entity_index] = b（整表先填 0xFFFFFFFF = 不属任何批）
  │     · SetBatchLayout(b, row_base, capacity) → EndBatchLayout(batch_count) 落设备
  ├─（未接管批：标签留无效 ⇒ cull 不写行、CPU 路径照旧绘制；按批粒度）
  ↓
cull 派发（RenderFrameSync，本帧）
  ├─ 幸存（6 平面 AABB）→ 全局可见表 + 全局计数（对拍口径不变）
  └─ enable_gather：读 batch_tag → binfo → atomicAdd(counter[b]) → slot
        · slot < capacity：row = row_base + slot
              · icb[row]      = {X = ceil(池行元素数 / mesh_group_size), Y=1, Z=1}
              · l2w[row]      = EntityGPU.row[Transform]
              · cmds[row]     = {geometry_id, first_instance = **slot**（切片内序号）}
              · mtl[row]      = 材质两位 + 阴影两位（逐字节照抄，T3h 口径）
        · slot >= capacity：跳过 + atomicAdd(counter[batch_count])（overflow 槽）
  ↓
主帧绘制（RenderDrawSubmit）
  └─ 每批：DrawMeshTasksIndirectCount(icb, first = row_base*12B,
                                      countBuf = counter[b] 偏移, maxCount = capacity)
      push 三表地址各加 row_base × 行字节（cmds 8B / l2w 4B / mtl 16B）
```

### 实现映射

| 文件 | 改动 |
|------|------|
| `inc/hgl/ecs/support/GPUCullSystem.h` | 相位注释（RenderFrameSync/2β）；`kMaxBatches=256`、`kInvalidBatchTag`；`BeginBatchTagWrite/SetBatchLayout/EndBatchLayout`、`GetBatchCounterByteOffset/MapCurrentBatchCounters`、台架视图 `GetBatchCount/GetBatchLayout` |
| `src/ecs/support/GPUCullSystem.cpp` | 相位改 `RenderFrameSync`；三缓冲创建（标签/分区/计数，**按帧槽分份**，`counter` 带 `TRANSFER_DST`）；PC 加三地址 + `batch_count`（112B）；`FillBuffer` 在 cull cmd 内清本帧槽计数 + `TRANSFER→COMPUTE` barrier；hook 签名 + 切片接线（`icb_count_buffer` = 本批计数槽、`gpu_row_base/gpu_capacity`） |
| `ShaderLibrary/ecs/entity_gpu_cull.glsl` | PC 镜像三地址 + `batch_count`；`BatchTagRef/BatchInfoRef/BatchCountersRef`；gather 段改"标签 → 切片 → atomicAdd → 越界计数"；`cmds.first_instance` = 切片内序号 |
| `src/ecs/support/PrimitiveBatchPipeline.cpp` | guard 重写：**逐批**评估 + 批号分配 + 标签直写 + 切片装配（起点 4 行对齐）；`row_cursor` 容量前置检查；全量/部分接管分别记账；`RunCulling` 仅在**全量**接管时跳过 CPU 剔除 |
| `inc/hgl/ecs/support/PrimitiveBatchPipeline.h` | `IsGPUCullFullTakeoverActive()` + `gpu_cull_full_frame_`（按批粒度接管下，部分接管时 CPU-driven 批仍需 CPU 剔除，否则退化成"全项提交"） |
| `inc/hgl/ecs/core/MaterialBatch.h(.cpp)` | `gpu_row_base / gpu_capacity`（切片；`Clear()` 复位） |
| `inc/hgl/graph/RootAddressPush.h` | push 末尾追加三个字节偏移（追加式 ⇒ 既有位置调用不受影响） |
| `src/ecs/support/PipelineMaterialRenderer.cpp` | 绘制段偏移/段长 = 切片（`first = row_base`、`maxCount = capacity`）；push 三表地址加 `row_base × 行字节` |
| `src/ecs/support/CPUCullReferenceSystem.cpp` | 相位随 cull 迁 `RenderFrameSync` + `AddDependency<GPUCullSystem>()`；`CheckShadowFacts` 改**逐批切片走查**（行→实体走 l2w 行号反查，多批交错下保序表作废）+ 两本账交叉核对 + overflow 硬判据 |
| `example/Material/PBRSpheres.cpp` | `PBR_MULTIBATCH=1`：网格后半走第二套配方（同 def / cull off）⇒ 期望 2 个批 |

### 契约要点（实现中被抓住的硬约束）

1. **帧槽分份**：标签/分区/计数三者按帧槽分份、CPU 只写**当前份**——否则本帧直写会与上一帧仍在飞行的
   cull 竞争（同队列提交序只覆盖"新提交读旧写"，不覆盖 CPU 直写）。
2. **切片起点 4 行对齐**：绘制侧 push 地址加 `row_base × 行字节` 后仍须满足引用声明的 16B 对齐
   （cmds 8B/行、l2w 4B/行、mtl 16B/行）⇒ `row_base % 4 == 0` 是同时满足三者的唯一条件（每批至多浪费 3 行）。
3. **`cmds.first_instance` = 切片内序号**（不是竞技场行号）：mesh 侧 `gl_InstanceIndex = first_instance + wg.y`，
   而 push 的 `addr_l2w/addr_mtl` 已带切片偏移 ⇒ 切片内 0 起，材质/mesh 发射器**零改动**。
4. **按批粒度接管**：一批前提不满足只影响该批（标签留无效）；但只有**全量**接管才跳过 CPU 剔除。
5. **竞技场是共享的**：全场景接管项数 ≤ `kMaxVisibleEntities(4096)`；批内切片容量 = 该批 item 数。

### 判据

- **多批测试床 = `example/Material/CoverageVariants`**（2026-10-04 新增）：一种 MaterialRecipe 的
  两个**覆盖变体**——Opaque（coverage None）与 DitherMask（`render_state_overrides.dither=true`
  ⇒ 生成 GLSL 带 `#define HGL_ALPHA_DITHER`）⇒ **不同 shader program ⇒ 2 个批**；同一几何、
  同一 mtl_def、同一纹理，差别只在覆盖模式（与"只改渲染状态"成不了批形成对照——EDS 后 Pipeline
  只留 shader 部分）。env：`CV_CULL=1`（dispatch+对拍）/ `CV_FORM_B=1`（接管）/ `CV_FRAMES=N` /
  `CV_DUMP=<tag>` / `CV_YAW|CV_PITCH|CV_DIST|CV_TX|CV_TY|CV_TZ`（取景）。
  实测：`ENGAGED batches=2/2 items=20 arena_rows=24`、Parity 20/20/0、
  `Σcount=20=global overflow=0 值链=20/20 bad=0`、三方像素（CPU 账 / 仅 dispatch / 接管）逐字节全等、VUID=0。
- **第二个测试床 = `example/Material/AlphaModes`**（2026-10-04 新增）：同一份内容用三种 alpha 呈现方式
  （Opaque / **AlphaBlend** / Dither）贴同一 CUBE、alpha 取自 `res/image/gradient` 的单通道 BC4 纹理。
  它把"批从哪来"又钉了一遍：**3 个 recipe 只出 2 个批**——Opaque 与 AlphaBlend 只差 `pipeline_config`
  （`alpha_blend` 在 EDS 下走 `cmd_set_color_blend_enable` **动态状态**）⇒ 同 program 同 pipeline ⇒ **同批**；
  Dither 改生成 GLSL ⇒ 不同 program ⇒ 单独一批。实测 `ENGAGED batches=2/2 items=18 arena_rows=20`、
  Parity 18/18/0、`Σcount=18=global overflow=0 值链=18/18`、三方像素逐字节全等、VUID=0。
- **正向**：`[T3i] GPU cull draw override ENGAGED: batches=x/y items=N arena_rows=M`（一次性）——
  静默退回 CPU 账也会像素全等 ⇒ 没有本日志不能宣称接管成立。
- **台架（值链 + 两本账）**：`Σ counters[b] == 全局幸存计数`；`overflow == 0`（硬判据）；
  逐批行 → 实体（l2w 行号反查）→ 三段阴影值链逐字节相等。
- **既有判据保留**：门 44 PASS / 0 FAIL、Parity、像素 CPU==形态 B 全等、VUID=0、reject=0。

### 欠账更新（§8.5 清单）

- ③ **多批/混材质** —— 本节点完成（无 DGC 依赖：切片 + 批标签 + 每批计数即"无 DGC 的 GPU 驱动多批"）。
- ~~② meshlet（补 GPU 可读 meshlet_count 源）~~ / ~~④ 实例化（per-item instance_count 源）~~ **均 won't-do（2026-10-05，代码整体删除）** ⇒ **本线 A 系列欠账已清空**，只剩 §8.7 B 系列与 A6；
  DGC 按拍板**不处理**（开发机无支持；形态 B 不需要任何扩展，只依赖 mesh shader）。

---

## §8.7 未完成清单（2026-10-05 整理；**新会话照此接手**）

> **订正（2026-10-07）**：A1–A5 已全部结案（下表的划删即凭证）。**仍未做**：`A6`（CPU 渲染路径去留——其前置「②③④ 覆盖真实场景」已定：② meshlet 与 ④ 实例化均 won't-do、③ 已落地 ⇒ 可重新拍板）、`B1`（混合排绘制顺序自由度）、`B2`（unlit/sky 的 alpha 语义统一）、`B4`（dither 白噪声 → Bayer）、~~`B5`~~（**已了结，2026-10-07**：该重载先补实现并做往返验证，随后按「0 消费者 = 投机 API，不做」纪律**连声明一起整删**（详见本表 B5 行）；`VKTextureReadback.h` 的 usage 前置注保留）。另：本表「门 44/0」是当时口径，现门为 **45 PASS / 0 FAIL**。

### A. 本线（GPU 剔除）欠账

| # | 内容 | 现状 / 落点 | 接手判据 |
|---|------|-------------|----------|
| ~~**A1**~~ | ~~**② meshlet 几何**~~ **won't-do（2026-10-05 关闭）**（X 组数走 CPU `GetMeshletCount()` 分支） | guard 仍把这类批挡在接管外（池行 `addr_meshlets != 0` ⇒ 该批不接管）。**已改为整体删除**（不再补源）：池行 3 个 `addr_meshlet*` 删 ⇒ **128→104 B**、GLSL 镜像、门步长断言、`VKGeometry`/`MeshShaderVertexAdapter`/`PrimitiveBatchPipeline` 全删、loader 建对象前显式拒绝 | 门 44/0（含 `E.mesh-draw-params-head-parity` 步长 104）+ loader 拒绝 8/8 + 0 `[DESTROY-WARNING]` + 回归示例全绿 |
| ~~**A2**~~ | ~~**④ 实例化/多实例 item**~~ **won't-do（2026-10-05 关闭）** | **查实无真实使用者**：唯一活调用方是 A3 台架（它为「被拒」而存在）、`SetInstance4ID` 只有测试在调、collector 对多实例写「全部同 4-ID」、上游无「每实例不同」的生产者 ⇒ 就算把 Y 写成实例数也只是同一项原地叠 N 遍，判据立不起来 | 按零兼容**整体删除**：`InstancedPrimitiveRenderItem` + 8 个自由函数 + slot 实例三字段（**行宽 48→40 B**）+ guard 该条 + A3 的④类项（其「异 program 自成一批」半保留 ⇒ A3 现为 5/5 全量）。存储侧 `AllocateContiguous/ReleaseContiguous` 保留（低层存储原语，Test 6 覆盖）；**CPU 侧自动合批不受影响** |
| ~~**A3**~~ | ~~**部分接管路径从未被真实触发**~~ **已完成（`c96cac8cd`，AlphaModes `AM_PARTIAL=1`）** | 台架 = Opaque 行最后一格改"同 def + billboard 顶点节点"（⇒ 独立批）+ `AllocateContiguousInstances(…,1)`（⇒ `InstancedPrimitiveRenderItem`，④ 类项）。实测：reject 日志（④ 理由）+ `ENGAGED batches=4/5 items=27` + `Parity 28/28/0` + `Σcount=27 < global=28 overflow=0 值链=27/27`（台架判据本就 Σ≤global，未误报）+ 未接管批确实被 CPU 账画出（Opaque 行 7 块）+ 像素三方逐字节全等 + 默认关时仍 4/4。**2026-10-05 补记**：④ 类项已整体删除 ⇒ 本台架的 `AllocateContiguousInstances(…,1)` 半消失，实测变为 `ENGAGED batches=5/5 items=28 arena_rows=36`（全量接管；异 program 自成一批照常被正确处理）⇒ 本行原「4/5 部分接管」判据改由 A4 批数超限（`CV_STRESS=260` ⇒ 256/260）覆盖 | 已达成（判据已迁移） |
| ~~**A4**~~ | ~~**容量边界未验**~~ **已完成（`CoverageVariants` 加 `CV_STRESS=N` 台架）** | N 个「同 def(Lit) + 仅 `alpha_cutoff` 不同」的配方（阈值进 `#define HGL_ALPHA_CUTOFF <v>` ⇒ program/批各不同）。实测 `CV_STRESS=260 CV_DIST=60`：**告警 1 次**（`接管批数超 kMaxBatches(256)`）+ `ENGAGED batches=256/260 items=256 arena_rows=1024`（=256×4，4 行对齐）+ `Parity 260/260/0` + `batch slices 256 rows=256 Σcount=256 global=260 overflow=0 值链=256/256`（**Σcount < global 合法**、无假红）+ 像素（CPU 账 vs 部分接管）逐字节全等 ⇒ 未接管的 4 批仍由 CPU 账画出。**负控**（临时改码，验完即撤）已证两条硬判据非空转：竞技场容量临时改 256 ⇒ `批次累计项数超共享竞技场` 精确在 64 批×4 行=256 顶格触发；批容量少一行 ⇒ `[T3i] 切片溢出！overflow=N`。**新发现**：竞技场那条在**现容量下不可达**——`EntityGPUTable::kCapacity(4096) == GetGatherCapacity()(4096)` 且**每实体至多 1 个批 item**（多批床台实测 items == 实体数）⇒ Σitems ≤ 4096 ⇒ 对齐后恒 ≤ 容量。**拍板 ①**：该分支已删除，随之删除只为它存在的 `GPUCullSystem::GetGatherCapacity()`（零残留）；删后判据复验不变（`batches=256/260`、`Σcount=256<global=260`、门 44/0）。⚠ 残留风险：若将来 `EntityGPUTable::kCapacity` 被抬到超过竞技场行数，切片会越过 gather 缓冲（静默越界写）——挡这个要的是 `static_assert(kCapacity <= kMaxVisibleEntities)`（机会性手段，②被否） | 已达成 |
| ~~**A5**~~ | ~~**L2W 环深度 = 3 vs 帧槽空间 = 8**~~ **已闭（代码层已按不变式写死）** | `inc/hgl/common/RenderOptions.h:33`：`#define HGL_L2W_RING_FRAMES HGL_FRAME_SLOT_TOTAL` ⇒ **环深度 ≡ 帧槽空间**，不可能再出现"离屏槽与主帧槽别名"（该别名曾导致 prepass 覆写主帧矩阵、整帧只剩清屏色）。§8.4 的旧"待查"已过时 | 已闭 |
| **A6** | **CPU 渲染路径的去留** —— **已拍板：删（用户 2026-10-07）** | 不再取「RELEASE 宏屏蔽」那条备选（代码里本就没有宏残留，已核）。**执行线 = §11 T4c4**：T4c1/T4c2/T4c3 ✅、T4c4-1 ✅、T4c4-2 ✅、T4c4-3 ✅（`49d9e2430`，CPU 绘制批对象链已删）；**T4c4-4 ✅（`82e488103`）** —— 逐字段取证只删真死件（`debug_blocks_logged` / `icb_count_buffer_offset`），其余字段仍有真实读者 ⇒ 不动。判据沿用 A6-d：九例 `ENGAGED x==y` + guard 拒绝 0 + 六台架摘要逐位不变 + 阴影两例逐位不变 | ✅ **已结案（`82e488103`）** |

### B. 本次会话（alpha 呈现链）带出的新欠账

| # | 内容 | 现状 | 建议 |
|---|------|------|------|
| **B1** | **混合排的绘制顺序自由度** | `alpha_blend` 真生效后混合排结果与**绘制顺序**相关（引擎无透明排序）；GPU 接管的行序 = atomicAdd 幸存序 ⇒ 与 CPU 账可不同 ⇒ **像素全等判据对混合排不保证**（实测：多次复跑中偶发一次差异，且只可能落在混合排；序无关的 opaque/alpha_test/dither 恒可用于硬判据） | 要硬判据：让接管路径的混合批沿用 CPU 序，或给混合批加固定排序键 |
| **B2** | **unlit / 其它材质源的 alpha 语义未统一** | 只有 Lit 源改成 `alpha = base_color.a × opacity_mask.r`；unlit/sky 的 alpha 仍来自各自源模块（`forward_flat` 的 `lighting.alpha`） | 若要同一"比例 × 图案"语义 ⇒ 逐个材质源改对应 alpha 求值 |
| ~~**B3**~~ | ~~`alpha_to_coverage`~~ **已按拍板整体删除（2026-10-05）** | 该特性**永不支持** ⇒ 清掉全部残留：`MaterialPipelineConfig.alpha_to_coverage`、TOML 键、`MaterialCoverageMode::AlphaToCoverage`、`BlendMode/PassType` 的 A2C 枚举项、Vulkan 侧（EDS3 的 `extendedDynamicState3AlphaToCoverageEnable` 启用 + `vkCmdSetAlphaToCoverageEnableEXT` proc/命令录制 + 动态状态表项 + 名字表项）、门与文档 | 全仓 grep 零残留（`alpha_to_coverage`/`alphaToCoverage`/`AlphaToCoverage`/`ALPHA_TO_COVERAGE`/`ForwardA2C`，`backup/` 与第三方目录除外） |
| **B4** | **dither 阈值是位置哈希（白噪声）** | `ShaderLibrary/common/alpha_compositor.glsl:HGLDitherThreshold` = `fract(sin(dot(pixel,...))*43758.5453)` | 想要**有序抖动点阵** ⇒ 换 Bayer 4×4（一处改动） |
| ~~**B5**~~ | ~~**引擎 API 缺口**：`ChangeTexture2DArray(tex, const void*, …)` **只有声明没有实现**（LNK2019；raw 版 `ChangeTexture2D` 有实现）~~ **已了结（2026-10-07）：先补实现、再整删**。① 补实现（照 2D 范式 `CreateTransferSourceBuffer` → 委托 `DeviceBuffer` 重载 → `delete`；去掉历史那处无符号恒假判据 `base_layer<0`）并用临时往返探针验证（上传已知字节 → GPU → `ReadbackTexture` 逐字节比对，三项全过：A 回读==源 / B 重传全零生效 / C 只写层 1 而层 0 不动；VUID 归零）；② 但**全仓 0 调用者**，按用户纪律「0 消费者 = 投机 API，不做」⇒ **声明与实现一并删除**（`TextureManager.h` + `VKDeviceTexture2DArray.cpp`），不给不存在的消费者留 API。**顺带保留**：探针暴露的 `ReadbackTexture` 隐含前置（目标纹理必须带 `VK_IMAGE_USAGE_TRANSFER_SRC_BIT`；`TextureCreateInfo` 默认只有 `DST\|SAMPLED`）已补进 `VKTextureReadback.h` 的接口注释。 | 已了结（删） |

### C. 文档/测试床现状（本次会话后）

- 测试床两个：`example/Material/CoverageVariants`（同 def 两覆盖变体 ⇒ 2 批）、
  `example/Material/AlphaModes`（4 行 alpha 方式 × 7 档比例 ⇒ 4 批）；
  env 门控 `CV_*` / `AM_*`（`*_CULL` / `*_FORM_B` / `*_FRAMES` / `*_DUMP` / 相机覆盖），默认全关。
- 判据口径：门 44 PASS / 0 FAIL；`ENGAGED batches=x/y`（正向）；台架 Parity + `Σcount` + `overflow` + 值链；
  像素三方（CPU 账 / 仅 dispatch / 接管）逐字节全等（**混合排见 B1**）。
- 本节若与 §8.3/§8.6 冲突，以本节（最新）为准。

---

## §8.8 A1（② meshlet 接管）侦察结论与任务切分（2026-10-05）

> **⚠ 本节整体作废（2026-10-05 晚）**：用户拍板**本轮不支持 meshlet** ⇒ T2..T5 不再实施，
> 改为**删除引擎侧 meshlet 代码**（清单见 §8.3 第 2 条②）。本节保留作**被否方案**的记录，
> 其中的坑仍然有效（「X 组数 = 集群数」在着色器侧**没有语义**；G1–G5 接线缺口）。

**结论：引擎侧改动很小（1 个池行字段 + 1 处写入 + GLSL 镜像 + 门用例 9→10 + 放开 guard）；
真正的成本全在"测试床"——`res/` 里没有任何带 meshlets 的资产（33 个 `.mesh/.scene` 全查过，零命中），
但 `VKGeometry::SetMeshlets(count, mb, mvb, mtb, mbb)` 允许**在代码里造 meshlet 几何** ⇒ 床走"代码造"，
不走内容管线（除非将来要走 GLTFConvert/meshoptimizer 产资产，那是另一条内容线）。**

### 侦察事实（逐条带落点）

| # | 事实 | 落点 |
|---|---|---|
| 1 | 池行是**单源表**：CPU struct / GLSL 字段名 / 字段类型 / 偏移断言全部由一张 X 列表生成 | `inc/hgl/graph/ShaderBufferSources.h`（`HGL_MESH_DRAW_PARAMS_FIELD_LIST`） |
| 2 | 行布局 = 头 9×4B + **4B 对齐填充** + 11×uint64 = **128B**；`static_assert(MeshDrawParamsLayoutValid())` 抓漂移 | 同上（`MeshDrawParamsLayoutValid` / `static_assert`） |
| 3 | ⇒ 新增一个 `uint` 头字段**正好吃掉那 4B 填充**：总长仍 128B、uint64 尾偏移不动 | —— |
| 4 | GLSL 侧镜像 `MeshDrawParamsHeadData`：9 字段 + `uint8_t _pad[92]` ⇒ 加字段须同步改 `_pad[92] → _pad[88]` | `ShaderLibrary/ecs/entity_gpu_cull.glsl` |
| 5 | **门有对表用例**：`E.mesh-draw-params-head-parity` 解析 X 列表**头字段**并与镜像逐字段比 + 断言步长 128 | `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp`（`RunMeshDrawParamsHeadParityCase`） |
| 6 | 池行作者（唯一写 `addr_meshlets` 的地方）就在 meshlet 分支里，`meshlet_count` 成员近在咫尺 | `src/SceneGraph/VKGeometry.cpp`（`if (HasMeshlets())`）+ `inc/hgl/graph/geo/VKGeometry.h`（`meshlet_count` / `SetMeshlets`） |
| 7 | CPU 账的 X 口径：`mesh_cmd->groupCountX = db.geometry->GetMeshletCount()` | `src/ecs/support/PrimitiveBatchPipeline.cpp`（`WriteMeshDrawCommands`） |
| 8 | GPU gather 侧**已能读池行**（`#define mesh_draw_params_pool …rows`）⇒ 加字段后可直接算 X | `ShaderLibrary/ecs/entity_gpu_cull.glsl` |
| 9 | 网格着色器侧无需新变体：镜像声明**无条件**发射 `#define sbo_meshlets MeshletDescriptorRef(draw_params.addr_meshlets)`；meshlet 与经典路径由 `draw_params` 决定 | `src/ShaderGen/meshgen/MeshShaderVertexAdapter.h` |
| 10 | guard 第 6 条（`addr_meshlets != 0` ⇒ 不接管）是这一件要放开的那条 | `src/ecs/support/PrimitiveBatchPipeline.cpp`（`TryApplyGPUCullOverride`） |
| 11 | 阴影批同路：caster 走 `ResolveShadowCasterProgram` 形成独立批 ⇒ 同一 guard/同一池行 ⇒ 放开口径对 forward/shadow 一致 | `RenderPrimitiveCollectSystem.cpp`（`ResolveShadowCasterProgram`） |

### 任务切分（每步编译 + 运行验证后再进下一步）

| 任务 | 内容 | 判据（可自动判定） |
|---|---|---|
| **T1 床（代码造 meshlet 几何）** | 参照 ApiValidation 的 meshlet 布缓冲方式，用 `SetMeshlets` 在一个示例里造几何（如 1~K 个 meshlet 的方块/球）；**先不开 GPU cull** | CPU 账下正常出图（X=`GetMeshletCount` 已是 CPU 口径）；`[IndirectMeshDraw]` 日志 x>1 的段；`[ERROR]`=0、VUID=0 |
| **T2 池行加字段** | X 列表加 `meshlet_group_count`(uint) + `VKGeometry.cpp` 在 meshlet 分支写值 + GLSL 镜像加字段并把 `_pad[92]→[88]` + 门用例头字段 9→10（含诊断文案） | 门 44/0；`static_assert` 通过；非 meshlet 示例像素**逐字节不变**（新字段=0 ⇒ X 仍 1） |
| **T3 gather 的 X + 放开 guard #6** | `X = (meshlet_group_count == 0) ? 1 : meshlet_group_count`（与 CPU 同口径）；删除 guard 第 6 条 | meshlet 床：`ENGAGED batches=k/n`；Parity mismatch=0；`Σcount ≤ global`、`overflow=0`、值链满；**像素三方逐字节全等**（cluster 数错会画漏/画重） |
| **T4 阴影侧** | 同一 meshlet 几何 + caster ⇒ 阴影批也接管 | 阴影图 dump 对拍（沿 CSM 判据口径）；VUID=0 |
| **T5 文档收口** | §8.7 A1 关闭 + 本档 §8.5/§8.6 口径 + `doc/render-flow-cull-sort-drawcall.md` §2.3 guard 表 | 全仓 grep 无残留旧口径 |

**风险**：① 池行是共享结构 ⇒ 改后**必须清整棵 Debug obj 树 + 清 shader 缓存**（`布局没变 ≠ 安全`：陈旧 TU 不会初始化新字段）；
② 门用例的头字段数写死 ⇒ 忘改就是"用例红"，别误判成渲染回归；③ 若 T1 床立不住（mesh 着色器对 meshlet 数据布局另有要求），
回退到"产出带 meshlets 的资产"这条内容线（更重，需单独拍板）。

### §8.8 补记（2026-10-05 晚，含工具链与渲染路径侦察）

**结论：② 的真正工作量不在"给 GPU 一个可读的 meshlet 组数源"，而在"meshlet 的集群展开渲染路径根本不存在"。**
`MeshShaderVertexAdapter` 只**声明**了 `MeshletDescriptorRef` / `MeshletVertexRef` / `MeshletTriangleRef` 与
`sbo_meshlets` 等宏，**全仓没有任何着色器读它们**（`sbo_meshlets` 全仓仅出现在那三行声明里）；现有网格顶点节点模板只有
`vertex_passthrough.glsl.tmpl` / `char_quad` / `line_quad`。因此：

- CPU 侧 `meshcmd->groupCountX = GetMeshletCount()`（meshlet 几何时 X = 集群数）**没有对应的着色器语义**
  —— 现在的 mesh shader 把 X 当"顶点组数"，于是 meshlet 资产会画错（实测：meshlet 版 vs `--no-meshlet` 版
  像素差 138 px / 0.015%，集中在可见小片区域）。**这也是 guard 第 6 条存在的真正原因**（不是"不同源"这么轻）。
- ⇒ ② 的正确切分（重排：先有渲染路径，再谈接管）：

| 任务 | 内容 |
|---|---|
| **T1 ✅（本轮完成）** | 资产与台架：`GLTFConvert` 转出 meshlet 资产 + `LoadScene` 加 `LS_*` 门控（含像素回读） |
| **T2** | **新增 meshlet 顶点节点**（`ShaderLibrary/mesh/meshlet_*.glsl.tmpl` + `VertexShaderNodeConfig` 一档 + 发射端接线）：`gl_WorkGroupID.x` = 集群号，读 descriptor → `meshlet_vertices` 取全局顶点号 → `meshlet_triangles` 取局部三角 → `SetMeshOutputsEXT` + `gl_MeshVerticesEXT`/`gl_PrimitiveTriangleIndicesEXT`。**判据**：meshlet 资产 vs `--no-meshlet` 资产像素**逐字节全等** |
| **T3** | 池行加 `meshlet_group_count`（单源表 + `VKGeometry` 写入 + GLSL 镜像 `_pad[92]→[88]` + 门 `E.mesh-draw-params-head-parity` 头字段 9→10） |
| **T4** | gather 的 X 改读该字段 + **放开 guard 第 6 条** |
| **T5** | 阴影侧 + 文档收口 |

### 工具链用法（本轮实测，写下来免得再摸）

```bat
:: 带 meshlet（默认就是开）
GLTFConvert.exe --no-images <input.gltf> <out_dir>
:: 不带 meshlet（对照）
GLTFConvert.exe --no-images --no-meshlet <input.gltf> <out_dir>
```
- exe：`build/out/Windows_64_Debug/GLTFConvert.exe`（`cmake --build build --target GLTFConvert --config Debug`）；
- **meshlet 数据写在 `.geometry`（minipack）里，条目名 `meshlets` / `meshlet_vertices` / `meshlet_triangles` / `meshlet_bounds`**，
  `.mesh` 只是小引用包（192 B）⇒ 用 `grep -a meshlets` 查的时候要查 `.geometry`；
- **不要省 `--no-images`**：纹理导出（mipmap 生成）是分钟级；台架只需要几何；
- 实测（Khronos `ABeautifulGame`）：8 个几何全部产出 meshlet（`meshlets` 条目 8704 B ÷ 16 = **544 集群**（40,480 tris）、
  `meshlet_triangles` = 40,480×3 B 全覆盖；索引被升成 U32 ⇒ 工具侧的 `indexType == U32` 前置不是障碍）；
- 本机落位：meshlet 版 `res/model/ChessMeshlet/`、对照版 `res/model/ChessClassic/`（与既有 `res/ABeautifulGame.StaticMesh/`
  一样**不入库**，按上面两行命令可再生）；
- 台架：`LoadScene` + `LS_SCENE=<.scene>` / `LS_DIST|LS_YAW|LS_PITCH` / `LS_FRAMES=N` / `LS_DUMP=<tag>`（像素回读）/
  `LS_CULL` / `LS_FORM_B`；loader 会打 `Loaded N meshlets for ...`（**注意**：该日志原缺 vararg ⇒ 打的是垃圾，
  已顺手修好——见下）。

### 本轮顺手修的真 bug

`example/Geometry/Shared/GeometryLoader.cpp` 的 meshlet 装载日志：
`MLogInfo(..., OS_TEXT("Loaded %u meshlets for ") + debug_name)` **没有传 vararg** ⇒ UB，实测打印垃圾常量 64
（8 个不同几何都显示 64，差点被当成"工具截断"）。已改为把 `meshlet_count` 作为实参传入。

### 另外两件待查（本轮暴露，非本线阻塞）

- **相机没框住模型**：`LoadScene` 默认（`ViewModel` + `distance=8`）下 ABeautifulGame 只占约 2.3 k 前景像素
  （dist 12→1036、40→84、150→6 ⇒ 模型很小/相机没对准）。T2 的"逐字节全等"判据需要**先把模型框满**，
  建议给 `LoadScene` 再补 `LS_TX/TY/TZ`（target 覆盖）并实测一组取景参数。
- `LoadScene` 的资产路径是硬编码单例；本轮已加 `LS_SCENE` 覆盖，未改默认行为。

---

## §8.9 B5-a 外部 GPU-Driven 注入入口整体退役（2026-10-05）

**背景**：`example/ApiValidation/ComputeFrustumCull` / `ComputeAsteroidBelt` 是**外部自己驱动 GPU
绘制路径**的探针（自建 compute 管线 + 7 张 BDA 表 + 间接命令/计数/L2W 索引表，再用 `SetGPUDriven`
+ 5 个 `Set*Buffer` 把整套塞进引擎）。T3 线把这条路径**整体接管**（引擎 `GPUCullSystem` 成为唯一
owner）之后，这两个示例在 **T3i `6b6baa39f` 静默黑屏**。

### 根因（实测，可复现）

T3i 在 `src/ecs/support/PipelineMaterialRenderer.cpp` 引入
`max_count = gpu_driven_override ? gpu_capacity : indirect_draw_count`，而 **`gpu_capacity` 全仓只有
`GPUCullSystem.cpp:763` 写**（引擎剔除接管时）；外部注入的批从不 engaged 它 ⇒ `maxDrawCount = 0`
⇒ `vkCmdDrawMeshTasksIndirectCountEXT` 画 0 条。实测日志：
`[IndirectMeshDraw] mesh indirect flush engaged: first=0 count=0（gpu_driven=1 row_base=0 capacity=0）`；
**0 VUID、0 `[ERROR]`**（`maxDrawCount=0` 合法）⇒ 只有画面看得出黑屏。
（不是迁移问题：源码逐字节相同、全新构建、由提交历史可确定复现。）

### 处置（拍板「深删」，不补 fallback）

删掉整个外部注入入口系：7 个 setter（`SetGPUDriven` / `SetL2WBuffer` / `SetL2WIndexBuffer` /
`SetMeshDrawParamsBuffer` / `SetMaterialDataRowsBuffer` / `SetIndirectMeshTaskBuffer` /
`SetIndirectCountBuffer`）、`MaterialRuntimeSlot` 的 6 个 `*_handle` + `indirect_count_offset` +
`is_gpu_driven` / `is_indirect`、`GetBufferHandles`、**世界级 `MaterialRuntimeBufferTable` 句柄表整体**
（B5-a 的唯一承载物）、`InstancedPrimitiveRenderItem` 的 9 个 buffer getter、
`PrimitiveBatchPipeline::FinalizeBatch` 的外部注入支、`RenderPrimitiveCollectSystem` 的
`is_gpu_driven`/`is_indirect ⇒ is_instanced` 两条判据、**Test 37（B5-a 契约）整块**。
`MaterialRuntimeSlot` 行宽 **88 → 48 B**（`static_assert` 与布局注释同步改；Test 38 契约文本 88→48）。

- ⚠ `gpu_driven_override` **字段不删**：`GPUCullSystem.cpp:756` 在用（引擎剔除路径），只删外部注入那支。
- Test 30 盯的是 `PrimitiveBatchPipeline.cpp:531`（现役 GPU-Driven 路径），未受影响。

### 示例侧处置

| 示例 | 处置 | 依据 |
|---|---|---|
| `ComputeFrustumCull` | **重写为正式示例** `example/Cull/GPUDrivenCull`（新目录 `example/Cull/`，FOLDER `ULRE/Example/Cull`）：8×8=64 立方体单批；示例只装配实体 + 相机，剔除全交引擎 | 实测 `ENGAGED: batches=1/1 items=64 arena_rows=64`；三方（接管 / 仅 dispatch / 纯 CPU 账）像素**逐字节全等**；0 VUID / 0 `[ERROR]` |
| `ComputeAsteroidBelt` | 源码停放 `example/ApiValidation/_Obsolete/`，**待 ④ 实例化落地后再重写为正式示例** | `EntityGPUTable::kCapacity = 4096` 冻结 + guard 拒 `InstancedPrimitiveRenderItem` ⇒ 百万规模现在进不了这条路径 |
| `SimpleMeshTriangle` | 删除（无价值） | —— |
| `ComputeDispatch` / `ComputeIndirectCount` / `ComputeTransformHierarchy` | **保留**（实测全绿，各自覆盖不同 API 面；其中 `ComputeIndirectCount` 走的正是 T3 **现役** `MaterialBatch.icb_count_buffer` 路径） | 三个各自验证 256 元素 compute+BDA 读回 / 22 PASS / 23 PASS（max_diff=1e-6） |

### 门 / 测试

- 门 `ShaderResourceSchemaRegressionGate`：**44 PASS / 0 FAIL**。
- `TestCSMIncrementalPass`：**rc=0、40 Passed / 0 Failed**（"All CSM Incremental Pass Contract Tests PASSED"）。
- 顺带修了 Test 14 一条**既有陈旧契约**：T3h `aa54f2dcb` 把写入改成归一后的 `bias_for_row`
  （`NormalizeShadowBiasToRow`），契约串仍写 `bias_multiplier` ⇒ **HEAD 上本来就红**，且它 `return failed`
  会挡住 Test 15..41（我改的 Test 38 在里面）。已把契约串同步为 `= bias_for_row;`，保留契约原意。

## §8.10 A6 侦察：三条路划分与主路径纯化清单（2026-10-05）

> 用户拍板口径：**Driven 按"策略"分路**，不按"子系统"分；**共用机制层、各自语义**。
> 本节是 A6 的落地面——量完之后比原设想小得多（原设想“逐个给出路”，实际只剩一条活洞）。

### 一、三条路 + 两条线（拍板）

| 路 | 归属 | 现状 | 缺什么 |
|---|---|---|---|
| **Mesh Driven** | 主路径（form B） | 已落地（T3d..T3i） | 主路径纯化（见下） |
| **Text Driven** | 独立（`TextRenderSystem`/`TextRenderPipeline`，2D 管线配置） | 已有独立路径、**不在**实体行/L2W 体系 | 补自己的剔除与顺序（**painter 序，必须有序**） |
| **Line · 内容线** | **主路径的一档**（`MeshShaderMode::LineQuad` = 每线程 1 段 → quad） | 已在 mesh 管线里，只缺 X 组数语义 | 公式档 + 删 guard 该条 |
| **Line · 调试线** | 独立小 Driven（**停放中**） | **今天不存在**（真线拓扑无通路） | 做的话：**每对象一行 + 每形状一个专用 mesh shader**（AABB / 球 / 胶囊…各自在 shader 内按 xyz+size 生成线段） |

### 二、判别四要素（为什么这么分）

| 要素 | Mesh（主路径） | 内容线 | 调试线 | Text |
|---|---|---|---|---|
| 元素→图元映射 | 顶点组→三角 | 段→quad（4 顶点 2 图元） | 段→**真 line**（2 顶点 1 图元） | 字形→quad |
| 元素身份/实例源 | 实体 4-ID 行 | 同左（资产几何） | 每对象一行（参数） | 字形/排版行 |
| 变换源 | L2W 行 | **L2W（唯一共用项）** | 不共用（世界空间参数） | **不用 L2W**（2D） |
| 顶点源 | 资产缓冲（BDA） | 资产缓冲 | **参数**（xyz+size/style） | atlas + 每字形参数 |
| 剔除判据 | 世界 AABB × 视锥 | 同源（同一实体的 AABB） | 不需要（调试系统自己决定画什么） | 2D：屏幕矩形/裁剪 |
| 顺序 | 融合排（B1 待定） | 不排序 | 不排序 | painter 序 |
| 拓扑 | `layout(triangles)` | `layout(triangles)`（quad 模拟） | `layout(lines)`（**无通路**） | `layout(triangles)` |

⇒ **内容线 5/7 项与 mesh 同源（只差发射公式）；调试线/Text 与 mesh 近乎 0 同源。**

### 三、两条硬技术约束（决定了"内容线 vs 调试线"必须分开，不是取向）

1. **拓扑是编译期固定的硬编码 triangles**：`MeshShaderHeaderGen.h:75` 只发 `layout(triangles, max_vertices = …)`
   ⇒ 真线拓扑**完全没有通路**；反过来说，拓扑不同的程序**天然自成一批**，与现有批键机制兼容。
2. **宽线是设备可选特性**：`VKDeviceCreater.h:127` `wideLines = SupportLevel::Want`（可选）、
   `VKPhysicalDevice.h:211 SupportWideLines()`、`lineWidthGranularity` ⇒ 现代驱动（含本机 AMDVLK）通常不支持
   ⇒ **真线只能是 1px**；**有厚度/抗锯齿的线必须 quad 模拟** ⇒ 这正是"内容线（质量）"与"调试线（1px 精确）"
   必须分开的硬件根据。

### 四、A6-a 静态清点：guard 五条洞的现状（本会话实测 + 全仓静态面）

| guard 条件 | 今天谁产（实测） | 判定 |
|---|---|---|
| 批无 mesh stage | **找不到生产者**：14 个材质里 10 个无 `[mesh_shader]` 段〔2026-10-11：原 15，ShadowMap 示例退场删掉 1 个材质〕但走缺省 `VertexPassthrough` ⇒ 都有 mesh 程序；本会话全部台架从未命中该理由 | **疑似死条件**（删前先复验） |
| 批是 lines（X 组数不同源） | **内容线**：`PlaneGrid3D` / `RenderBoundBox` / `SimplestAxis` / `GizmoUsage` / `RayPicking` / `LoadGeometry` / `LineRender` 的线框资产 | **活洞①（唯一真洞）** |
| 接管批数超 `kMaxBatches(256)` | 人为台架 `CV_STRESS=260`（CoverageVariants） | **容量类**（非语义洞） |
| 含无 owner 实体项 | **找不到生产者**：④ 之后现管线只产 `PrimitiveRenderItem`，恒带 `EntityID`；"VDM 等无实体项"属**已退役的外部注入面** | **疑似死条件**（删前先复验） |
| 实体 4-ID 行缺失 / 池行缺失 | 未挂载几何/材质的实体（收集侧本来就不进批） | **护栏**（保留） |

### 五、主路径纯化清单（A6 的落地面）

1. **内容线公式档**（唯一活洞）：
   - X 语义差的**唯一位置** = 元素数取"段数"还是"顶点数"（`line_quad.glsl.tmpl:1`
     `line_id = gl_WorkGroupID.x * group_size + gl_LocalInvocationIndex`）⇒ 两者都能从池行现算
     （`is_indexed ? index_count : vertex_count`，线再 ÷2）；
   - 实施：**批标签表加一位 `lines`（或 topology 档）** ⇒ gather 按位选公式（不再需要 CPU 分支）
     ⇒ guard 删 `is_lines` 那条；CPU 侧 `CalcMeshGroupCount(is_lines, …)` **暂时保留**（未接管批仍走 CPU 账，
     两边同语义、不分叉），到 A6-d 再删；
   - 台架：现有线框示例的 `ENGAGED` / `Parity` / `Σcount` / `overflow` / 值链 / 像素三方。
2. **删两条疑似死条件**（无 mesh stage / 无 owner 实体项）——**先跑一遍全家族台架复验零命中**，再删。
3. **容量失败模式（A6-c 已拍板：分三层，不搞一刀切）**：

   | 容量类 | 今天（有 CPU 账） | 纯 GPU 后（A6-d 删除 CPU 账） |
   |---|---|---|
   | 接管批数 > `kMaxBatches(256)` | 旧「**软降级**：该批标签留无效 ⇒ 绘制退回 CPU 账 + 一次性 `GLogError`」
     **已删**（A6-d T4b） | ✅ **已落地 fail-fast**：一次性 `GLogError` 报因 + **该批本帧不绘制**
     （清空批内项 ⇒ 下游空批跳过，既不写 CPU ICB/参数行也不发绘制命令；标签留无效 ⇒ cull 不写行）。
     `CV_STRESS=260` 预期：`ENGAGED 256/260` + 一次报因 + 超出批无绘制，`overflow=0` |
   | 实体行表 > `EntityGPUTable::kCapacity(4096)` | **fail-fast**：`GLogError` + 该实体不进表
     （P-B8-6 拍板：预算制、不扩容、不静默截断） | 同今天 |
   | 批内切片溢出（GPU 侧 `slot >= capacity`） | **计数丢弃**：跳过 + `atomicAdd(counter[batch_count])`；
     台架把 `overflow ≠ 0` 当**硬判据** | 同今天（**运行时**收窄，不是设计容量） |
   | 竞技场行数（`EntityGPUTable::kCapacity == GetGatherCapacity()`） | **不可达** ⇒ 分支已删
     （`0072145aa`）；残留风险见 §8.7 | 同今天（要挡就用 `static_assert(kCapacity <= kMaxVisibleEntities)`） |

   一句话：**设计容量超限 = 项目 bug（响亮）；运行时收窄 = 计数上报（可判）**——两类都不许静默。
4. **判据重建**：像素三方（CPU 账 vs 仅 dispatch vs 接管）在删 CPU 账后失效 ⇒ 换成
   「参考实现（`CPUCullReferenceSystem`，不出图）+ golden 像素 + 回读自校验」。
5. **才删 CPU 账**（顺序：CPU 剔除 → compaction/ICB/l2w 写入 → 排序与 `distanceToCamera`）。

### 六、顺序与并行

A6-a（本节，已完成）→ **机制层 + 契约**（含"可省项"：调试线可省切片/多批，但不能省帧槽与回读口径）
→ **内容线公式档**（A6 前置，小改动）→ Text Driven（顺它已有路径补剔除/顺序）
→ 调试线 Driven（**停放**）→ 主路径纯化 + 删 CPU 账。

### 七、A6 前置落地结果（本提交）

**内容线已能进接管**：批信息行 `BatchInfoRow` 8 → **16 B**（加 `topology` + `group_size` 两字段，
与 `entity_gpu_cull.glsl` 逐字段同序同型），CPU 在 `SetBatchLayout` 里**随行下发本批的 dispatch 档**：

| 档 | 元素数语义 | 组大小 | 出处 |
|---|---|---|---|
| `kBatchTopologyTriangles`(=0) | 顶点数 | 192（`kMeshVertexPassthroughMaxVertices`） | `MeshDispatchGroupSize(false)` |
| `kBatchTopologyLines`(=1) | **段数**（`element_count >> 1`） | 64（`kMeshLineQuadMaxInvocations`） | `MeshDispatchGroupSize(true)` |

- 组大小**单源**：`PipelineMaterialRenderer.h` 抽出 `MeshDispatchGroupSize(is_lines)` ⇒ CPU 的 ICB 组装
  （`CalcMeshGroupCount`）与批信息行同取一处，两条账不分叉；`CullPushConstants.mesh_group_size` **撤掉**
  （GLSL 不再硬编码）——结构 11×u64 + 5×u32 = 108 ⇒ 对齐 8 ⇒ **sizeof 仍 112**、`batch_count` 偏移 → **104**
  （两条静态断言随改）。
- guard 删掉 `is_lines` 那条 ⇒ 线批不再被拒；未接管批的 CPU 账 `CalcMeshGroupCount(is_lines, …)` **保留**
  （与 GPU 侧同语义，到 A6-d 随 CPU 账一起删）。

**顺手修掉线模版的一处潜在坏数据**：`line_quad.glsl.tmpl` 的
`lines_this_group = min(group, total_lines - x*group)` 是**无符号减法** ⇒ 过派发（X 偏大）时下溢成巨值
⇒ 先 `SetMeshOutputsEXT` 声明满额输出再 `return` ⇒ **未写顶点 = 未定义数据**。改为**饱和式**求本组条数
（保持单次 `SetMeshOutputsEXT` 调用）。⚠ 实测教训：改成「越界即 `SetMeshOutputsEXT(0,0)` + `return`」会在本机
AMDVLK 上**挂死并丢设备**（三个线示例各 863 / 859 / 825 次 `VK_ERROR_DEVICE_LOST`；换回 HEAD 模版即恢复）
⇒ **勿改成那种形态**。

**验收床**：`example/Debug/PlaneGrid3D`（3×(32×32, sub 8×8) 网格 = 数千段）加 `PG_CULL` / `PG_FORM_B` /
`PG_FRAMES` / `PG_DUMP`。判据：`ENGAGED batches=1/1 items=3 arena_rows=4`（此前被 guard 拒 ⇒ 从无此日志）；
三方像素（纯 CPU 账 / 仅 dispatch / 形态 B 接管）md5 全等 `a896e5be650cd097bf8fe8f6b9fc97d0`，
非背景像素 11.9%（⇒ 非空帧，判据不空转）；0 VUID / 0 设备丢失。

**回归**（改的是共享批信息行 + gather + push constant，全台架复跑）：门 44 PASS / 0 FAIL；`TestCSMIncrementalPass`
40 / 0；`TestRenderItemDataStorage` 全过；ATS 自检 D3/D4 PASS；CV 常规 `2/2 items=20`、A4 `Σcount=256
overflow=0`、AM `4/4` 与 `5/5`、PBR `1/1 items=100`、LS `1/1 items=49`、GPUDrivenCull `1/1 items=64`
——全部 `ENGAGED`、对拍 `不一致=0`、`Σcount` == 批内项数、`overflow=0`、0 VUID / 0 `[ERROR]`。

### 八、欠账（本轮不做，未来排期）：gather 侧运行长度合并 —— 「CPU 合、GPU 不合」

**现象**（2026-10-05 用户 RenderDoc 截帧发现，**非 bug**）：`GPUDrivenCull` 截帧里 64 个 cube 是
**在 ICB 内一条条画**出来的，没合批成实例。三层事实：

1. **现状是按设计**：gather 每项写 `DrawMeshTasksIndirectCommand(group_count_x, 1u, 1u)`
   （`ShaderLibrary/ecs/entity_gpu_cull.glsl:348`）⇒ 一项一条命令、Y=1；而顶点侧实例号定义 =
   `gl_InstanceIndex = draw_params.first_instance + gl_WorkGroupID.y`
   （`src/ShaderGen/meshgen/MeshShaderHeaderGen.h:123`）⇒ **Y 维度本来就是实例维度，机制已在**。
2. **CPU 账会合，接管后不会**（同示例实测对照）：`src/ecs/support/PrimitiveBatchPipeline.cpp:733`
   `mesh_cmd->groupCountY = db.instance_count > 1 ? db.instance_count : 1u` ⇒ 同几何连续项合并；
   `GPUDrivenCull` 的 64 个 cube 同几何 ⇒

   | 模式 | ICB 命令数 | 证据 |
   |---|---|---|
   | CPU 账（基线 / 仅 dispatch） | **1 条（Y=64）** | flush 日志 `first=0 count=1（gpu_driven=0）` |
   | 形态 B 接管 | **64 条（Y=1）** | gather 写死 `(x,1,1)` + RenderDoc 截帧 |

   像素完全一致 ⇒ **现有像素/对账判据对此无感**；差的是命令数（ICB 条目、per-draw 参数行、
   每条 mesh 命令的固定启动开销）。
3. **成因 = ④ 关掉的那半边的侧效应**：本轮 ④ 只删了**组件多实例**（`InstancedPrimitiveRenderItem`、
   slot 实例三字段、管线的实例展开），**CPU 侧自动合批（compaction 的 `DrawItemRange::instance_count`）
   保留着** ⇒ 出现「CPU 合、GPU 不合」的不对称。

**未来任务形状**：gather 内做**运行长度合并**——相邻幸存项 `geometry_id`（及发射相关行）相同则
`groupCountY++` 并入同一命令，不同则 flush + 起新命令 ⇒ 命令数从「项数」降到「运行段数」。

**代价（这就是不夹进 A6-b 的原因）**：gather 现为「每项一个独立 invocation」的**纯并行**模型，
而「相邻项同键则并」需要知道**前一个幸存者**的键 ⇒ 要加一趟 run-start 标记 / 前缀和（或退化成
部分串行）⇒ 属**并行性改造**，会污染机制层的判据。

**判据（必须换维度）**：**命令数**（回读 ICB 计数）——CPU 1 条 vs 接管 1 条，且 `Σcount` 与
三方像素仍全等；**像素判据对这件事永远无感**（改错了也看不出来）。台架可直接扩 `GPUDrivenCull`
（`GC_*` 家族已有）。

**登记**：`doc/backlog.md` E 线 **E2**（待排期）；**与 Text/调试线无依赖**，排在机制层 + 内容线之后。

## §9 机制层规格与契约条目（A6-b，2026-10-05）

> 本节把 §8.10 说的「机制层 + 契约」定成形：**机制层 = 所有 Driven 共用的那套底座**，
> 策略层（每个 Driven）只补四样。契约分「已落地（可执行）」与「欠账（随对应 Driven 落地）」两栏。

### 一、机制层职责（六件，全 Driven 共用）

| # | 机制件 | 内容（唯一口径） | 现状落点（符号） | 契约载体 |
|---|---|---|---|---|
| M1 | **帧槽空间** | `HGL_FRAME_SLOT_TOTAL(=8)`；每张按帧索引的表**按槽分份**，CPU 只写本帧槽 | `Context`（世界地址表）/ 实体行表 / CameraInfoStorage | 门 + needle（ring 深度 == 槽空间） |
| M2 | **地址派发 / `pc_root`** | 每帧槽基址 + `pc_root` 指向本帧槽；行的每帧地址 = `基址 + 槽 × 容量 × sizeof(行)` | `RootAddressPush` / `WorldAddresses.addr_*` | needle |
| M3 | **ICB 形状 + 间接 dispatch** | 命令 = `{X=组数, Y=实例数, Z=1}`（每项一条、Y=1 现状见 §8.10 §八）；countBuffer 必须带 `INDIRECT_BUFFER_BIT`；切片偏移 = `row_base × 12B` | `PipelineMaterialRenderer::ProcIndirectRender` | needle + VUID 兜底 |
| M4 | **批标签 / 切片 / 每批计数纪律** | 三者**按帧槽分份**且 CPU 只写当前份；切片起点 **4 行对齐**（三表 push 对齐的唯一解）；`first_instance` = **切片内序号**；计数缓冲末项 = overflow 槽 | `GPUCullSystem`（`BeginBatchTagWrite` / `SetBatchLayout` / `MapCurrentBatchCounters`） | needle（见 §四欠账） |
| M5 | **竞技场 / 容量** | `kMaxBatches(256)`；竞技场行数 == `EntityGPUTable::kCapacity(4096)` | `GPUCullSystem` / `EntityGPUTable` | 台架 `overflow=0` + 失败模式见 §8.10 §五.3 |
| M6 | **回读 + 台架基座** | 生产路径零 CPU 同步 ⇒ 读前必须 `SyncForParityReadback`；`MapCurrentBatchCounters` 返回 `kMaxBatches + 1` 项 | `GPUCullSystem` | needle + 台架 |

### 二、策略层（每个 Driven）必须提供的四样 + 自己的台架

1. **剔除算法**（每类图元的可见性判据）；
2. **发射 / gather**（元素 → 图元 的映射与 X 公式的档）；
3. **排序策略**（有无、按什么序；**网格路径有，内容线没有，Text 需要画家序**）；
4. **容量配额与失败模式**（走 M5 的三层语义，明写「哪类超限算项目 bug」）。
**外加**：该 Driven 自己的台架（正向 `ENGAGED` 行 + 数值判据 + 像素/对账 + 破坏验证），
否则「静默退回旧账」也会像素全等（§8.10 的 T3i 黑屏教训）。

### 三、可省项矩阵（哪些机制件某条路可以不要）

| 机制件 | 网格路径 | 内容线（LineQuad） | 调试线（真 `layout(lines)`） | Text |
|---|---|---|---|---|
| M1 帧槽空间 | 必需 | 必需 | 必需 | 必需 |
| M2 地址派发 / `pc_root` | 必需 | 必需 | 必需 | 必需（自己的 `pc_root` 语义） |
| M3 ICB 形状 | 必需（`Y=实例数` 待补，§八） | 必需（同网格） | 可省（量小、可直接 `vkCmdDraw`） | 可省（同） |
| M4 切片 / 多批 | 必需 | 必需 | **可省**（固定小容量、单批即可） | 可省 |
| M5 竞技场 / 容量 | 必需 | 必需 | 必需（容量更小、失败模式同三层） | 必需 |
| M6 回读 / 台架基座 | 必需 | 必需 | **必需**（否则判据只能是目视） | 必需 |
| 排序策略 | 有（批内保序 + 批号稳定） | 有（同） | 无 | **有（画家序）** |

一句话：**帧槽与回读口径任何一条路都不许省**（§8.10 §六 的"可省项"落到这两条之外）。

### 四、A6-b 已落地的可执行契约

- **门新增 `B.batch-info-struct-parity`**（`ShaderResourceSchemaRegressionGate`，基线 **45 PASS / 0 FAIL**）：
  ① 批信息行 `BatchInfoRow` 的 C++ ↔ GLSL **逐字段同序同型**（类型/长度已归一，复用
  `ExtractStructFieldsLineWise`）；② 派发档 `kBatchTopologyTriangles/Lines` ↔ GLSL `#define TOPOLOGY_*`
  **数值一致**（两处各写一遍 ⇒ 这才是漂移面）。
  **破坏验证（两处都咬住）**：档号 `TOPOLOGY_LINES 1u→2u` ⇒
  `派发档数值漂移：kBatchTopologyLines=1 vs #define TOPOLOGY_LINES=2`；行字段换序 ⇒
  `BatchInfoRow 第 1 字段漂移：C++=uint capacity / GLSL=uint topology`。
  ⚠ **门的失败明细打在 stderr**（只抓 stdout 会误判"契约没牙"）；被检文件是门运行时读取 ⇒ 破坏/还原无需重编。
- 既有同族：`S.global-addresses-struct-parity` / `W.world-addresses-struct-parity` / `E.entity-gpu-struct-parity`。

### 五、三路验收矩阵（每路：正向判据 + 反证）

| 路 | 正向判据（可判定） | 反证 |
|---|---|---|
| 网格路径 | `ENGAGED batches=x/y items=N arena_rows=M` + `Σcount == 项数` + `overflow=0` + 对拍 `不一致=0` + 三方像素全等 | 破坏：容量 −1 / 档号漂移 / 关闭 gather |
| 内容线 | 同上 + `binfo.topology == LINES` 时 X 按**段数**（同示例三方像素全等） | 破坏：把线的档改成 TRIANGLES（应缺段/错画） |
| 调试线（未落地） | 线段数 == 对象数（§8.10 §三：每对象一行、每形状一个专用 mesh shader） | 破坏：关剔除/换 shader |
| Text（未落地） | 画家序 + 剔除数 | 破坏：顺序反转 |

### 六、A6-b 欠账：**1–4 已收口**（载体 = `TestCSMIncrementalPass` 的 Test 42，10 条契约；破坏验证 8/8 咬住）

1. ✅ **帧槽 / 环深度单源**：`HGL_L2W_RING_FRAMES` 本就是 `#define ... HGL_FRAME_SLOT_TOTAL`（A5 关闭），
   故 `static_assert` 会是恒真（无用）⇒ 改用 **needle 钉单源定义 + 一条禁复活**（不许退回
   `#define HGL_L2W_RING_FRAMES 3`）；另钉 `RingLayout` 默认深度必须取该常量。同时回扫了
   `Context.cpp` 里「那个 3 是待查的既有条件」这句**陈旧注释**（A5 后已反）。
2. ✅ **切片 4 行对齐**：推进式 `(cursor + capacity + 3u) & ~3u` 的 needle + **运行期守卫**
   （`SetBatchLayout`：`row_base % 4 != 0` ⇒ 一次性 `GLogError`；装配侧 bug 不静默违 16B 对齐）。
3. ✅ **ICB countBuffer 的 `INDIRECT_BUFFER_BIT`**：needle 钉 `batch_usage | INDIRECT | TRANSFER_DST`
   那一行（缺 = VUID-02715）。
4. ✅ **回读前置**：`SyncForParityReadback` 记「已同步帧」+ `MapCurrentBatchCounters` 校验
   （未同步 ⇒ 一次性 `GLogError` 且**拒绝返回**残影数据）＋ needle 钉台架 Sync/Map 成对调用点
   （实测同一采样帧内 Sync 在 Map 之前成套执行 ⇒ 守卫不误报）。
5. ⏳ **仍开**：§八 的运行长度合并（`Y=实例数`）落地时，命令数判据一并进 §五 验收矩阵。

## §10 A6-d 计划：删 CPU 账 + 判据重建（2026-10-05 侦察定稿）

> 前提（已量化，**不按整文件行数推断**）：本轮 ②③④ 关闭后，六个台架实测**全部 100% 接管**
> （`ENGAGED batches=n/n`）⇒ 主帧 mesh 批的 CPU 账在**生产路径上已不被消费**，只剩
> 「默认（无 opt-in）时用它」与「个别批被 guard 拒时兜底」两个活口。A6-d = 关掉这两个活口并删净它。

### 一、量化前提（静态清点：面 / 落点 / 标记数 / 归属）

| 面 | 落点（符号） | 标记数 | A6-d |
|---|---|---|---|
| CPU 剔除 / 收集 / 距离 | `RenderPrimitiveCollectSystem`（frustum 判据 + `item->distanceToCamera` 写者） | `distanceToCamera` 全仓 4 处 | **删** |
| CPU 排序 | ~~`PrimitiveBatchPipeline`（renderItems 按距离 + static/movable 两处 `std::sort`）~~ ✅ **2026-10-10 订正：本行已过期** —— `PrimitiveBatchPipeline.cpp` 现**没有任何 `std::sort`**（`SortByDistance` 全仓已不存在）；唯一的排序是 `PrimitiveRenderSystem::RenderBatches` 里按 `MaterialBatch::key` 排**批** | ≤0 处 | **已完成** |
| CPU 压缩 | `DrawItemCompaction.{h,cpp}` + 调用面 | 引用 17 处 | **删** |
| CPU 写行 / ICB | `PrimitiveBatchPipeline::WriteMeshDrawCommands`（+ `draw_batches` 16 处） | 定义 1 / 档外调用 **0** | **删** |
| CPU flush 分支 | `PipelineMaterialRenderer::ProcIndirectRender` 的 CPU 支（`else` 侧） | 1 处 | **删**（GPU 支保留） |
| CPU 绘制提交 | `PrimitiveRenderSystem`（95 行，RenderDrawSubmit）／`PrimitiveOverlayRenderSystem`（87 行，RenderDebug） | — | 主帧那条**删**；overlay **留** |
| 台架参考实现 | `CPUCullReferenceSystem.{h,cpp}`（107+502）／`RenderStrategyParity`（130） | `PerformCPUCulling` 2 处 | **保留**（新判据的参考端） |
| opt-in 面 | `PrimitiveBatchPipeline:1178` 三条件早退（`!gpu_cull \|\| !IsEnabled() \|\| !IsDrawOverrideEnabled()`）+ 示例 `*_CULL/_FORM_B` 门 | 3 处 | **删**（强制接管） |

### 二、边界（**不在** A6-d 内，防误删）

1. `PrimitiveOverlayRenderSystem`（**RenderDebug 相位**的 overlay；`AlphaTestShadow`/`CascadeShadowMap` 在用）；
2. **Text**（自有 2D 管线 + 自己的 pipeline，未落地）；
3. **shadow pass 的收集**（guard 本就 `IsCurrentPassShadow()` 直接 return ⇒ 从来不走接管）；
4. 参考实现 + 台架对拍（= 新判据的一半，保留）。

### 三、判据重建（**必须先做**，否则删完就没判据了）

删 CPU 账后「三方像素（CPU 账 / 仅 dispatch / 接管）」失效 ⇒ 换成三条互补：

1. **参考实现**（已在）：`CPUCullReferenceSystem`（**不出图**）对拍可见集合 ⇒ `不一致=0`；
2. **回读自校验**（已在）：l2w 行号反查 + 值链三段 + `Σcount == 批内项数` + `overflow=0`；
3. **帧摘要（新）**：固定帧回读颜色靶 ⇒ `{宽, 高, 格式, 不同取值数, FNV-1a 64}` 与示例内**常量**比对。
   - 沿用 ATS 的「`D3 18189 px / 600662 px` 常量内联」风格，**不入库大图**（与「dump 家族退库」口径一致）；
   - `不同取值数 > 1` 兼作**判据不空转**的守卫（全清屏帧会被抓）；
   - 门形态：`<EXAMPLE>_SELFCHECK=1` → 固定帧判读 → PASS 退出 0 / FAIL 退出 1；
   - **破坏验证**：改一像素 / 少画一批 / 档号漂移 ⇒ 摘要必须变（探针：临时把阈值调偏）。

### 四、任务切分（每任务**自身可编译 + 判据绿**才进下一个）

| # | 内容 | 判据 | 动全局结构 |
|---|---|---|---|
| **T1** | **判据重建** ✅ **已完成**：帧摘要自检落到**全部六个台架**（共用件 `example/Shared/FrameDigest.h`）。用法：`<PREFIX>_FORM_B=1 <PREFIX>_SELFCHECK=1 <PREFIX>_YAW/_PITCH/_DIST=<默认>`（**前缀带下划线**；显式下发相机默认值 = 冻结相机，是摘要可判的前提） | 各台架：两遍摘要一致（确定性）→ 回填常量 → `PASS/rc=0`；敏感性对照 `YAW=75` ⇒ `FAIL/rc=1`；摘要跑同时打 `ENGAGED batches=n/n` | 否 |

**六台架摘要（1280x720 A2BGR10UN，**GPU 接管下**实跑记录；改渲染结果必须复核并重录）**：

| 台架 | `fnv1a64` | 台架 | `fnv1a64` |
|---|---|---|---|
| `GPUDrivenCull`（`GCSELF`） | `0x90E0745C2C7B8599` | `PBRSpheres`（`PBRSELF`） | `0x3DD88A0999CDC486` |
| `AlphaModes`（`AMSELF`） | `0x2FE87B15EE8B8A16` | `LoadScene`（`LSSELF`） | `0xC9E38ED2F49A069B` |
| `CoverageVariants`（`CVSELF`） | `0x8EED38BAD9B8181E` | `PlaneGrid3D`（`PGSELF`） | `0x4125C3C830CB92D3` |

两条配套改动（T1 顺带）：① `PlaneGrid3D` 补相机 env 门（`PG_YAW/PITCH/DIST/TX/TY/TZ`，与其它台架同形）；
② `PBRSpheres` 时间冻结条件补自检模式（原来只有 dump 模式冻结 `elapsed_time` ⇒ 自检两次几何不同）。
| **T2** | **强制接管** ✅ **已完成**：删三条件早退 + opt-in 面整体删除（`SetEnabled`/`SetDrawOverrideEnabled`/`enabled_`/`override_draw_`/PC 字段 `enable_gather`；PC 104 B、`batch_count` 偏移 100）+ 六示例删 `*_CULL`/`*_FORM_B` 门与两个已不可实现的模式（`GC_CULL_OFF`/`GC_DISPATCH_ONLY`） | 门 **45/0**；`TestCSM` rc=0 + Test 42；**六台架摘要逐位不变**（只给 `_SELFCHECK`、不给 `_FORM_B`）+ `ENGAGED`；ATS 自检 PASS；冒烟 0 VUID/0 ERROR | 是（**默认行为已变**） |
| **T3** | 删 CPU **排序** + `distanceToCamera` ✅ **已完成**：字段 / `Compare()` 的距离 tie-break / 专用函数 `SortByDistance()`（含声明与调用点）/ 收集侧写者（连同失去用者的 `worldPos`/`camera_pos`）；文档回扫 3 份（就地标注 + 持指针） | 门 45/0；Test 42；**六台架摘要逐位不变**（⇒ 该序不参与渲染；实测六台架均为**不依赖序**的不透明几何）+ `ENGAGED`；ATS PASS；冒烟 0 VUID/0 ERROR | 否（含 `RenderItem` 布局变更 ⇒ 清树全量重编） |
| **T4** | 删 CPU **写行 / ICB** ⚠ **侦察后收窄**：这三样（`WriteMeshDrawCommands` / `EnsureMeshDrawParams` / `draw_batches`）对「被 guard 拒的批」仍是**活路**（兜底 CPU 绘制），不能裸删；`draw_batches` 更同时是 **overlay 系统的活需**（overlay 与主帧共用同一批缓存，也一样被接管）。<br>**T4a ✅ 已完成**：删接管批仍在跑的那圈 **CPU 命令累积**（`Render` 的 per-DrawBatch 循环——`Draw()` 只累计 `indirect_draw_count`，接管批用 `gpu_capacity` ⇒ 纯浪费）+ flush 调用门脱钩 CPU 计数（`|| gpu_driven_override`；否则跳过累积后多绘制不发）。<br>**T4b ✅ 已完成**：guard 拒 = **fail-fast**（一次 `GLogError` + 该批本帧不绘制，依据 `GPUCullSystem::kMaxBatches` 既有拍板政策）<br>⚠ **范围修正（T4b 动手时的侦察）**：我原先说的「T4b 后三样即可删」**不成立**——`TryApplyGPUCullOverride` 在 `IsCurrentPassShadow()` 处**显式早退**（shadow 批共用同一 `materialBatches` 表）⇒ **shadow pass 的批仍走 CPU 绘制**
⇒ `WriteMeshDrawCommands` / `EnsureMeshDrawParams` / `draw_batches` 至今仍是**活路**。要删它们，得先让 **shadow pass 也进 GPU 路径**
（各写各的切片/档，§9 §二 四要素按 pass 复现）⇒ 记为 **T4c**。 | T4a：门 45/0；Test 42；**六台架摘要逐位不变**（尖锐判据：flush 门错即整屏无绘制）+ ENGAGED；ATS PASS；冒烟 0 VUID/0 ERROR | T4a 否 / **T4b 是**（行为变更：拒批不再兜底绘制；`CV_STRESS` 台架预期随之改） |
| **T5** | 删 **压缩**（`DrawItemCompaction` + 17 处调用面） | 同上 | 否 |
| **T6** | 删 CPU **剔除 / 收集**（只服务 CPU 账的那部分；收集相位改） | 同上 + `TestRenderItemDataStorage`/`TestCSM` 同步 | **是** |
| **T7** | 清台架旧判据（三角里的 CPU 账那一臂）与示例 `*_CULL/_FORM_B` 门 | 门 45/0 + 各自检 PASS | 否 |
| **T8** | 全量回归：所有示例（含 overlay / Text / shadow）跑一遍，摘要变的逐个复核 | 全绿 | — |

### 五、风险

| # | 风险 | 默认选择 |
|---|---|---|
| R1 | 非主帧路径（overlay / Text）被误删 | 边界见 §二；T6 收集面删除**前**按「谁消费」逐一 grep 列清单 |
| R2 | 摘要对**合法**变化过脆（相机/时序微差） | 摘要只建在**冻结相机 + 定帧退出的台架**上（`*_FRAMES` 已具备） |
| R3 | T2 改默认行为 ⇒ 个别示例暴露既有 bug | T2 判据 = 六台架摘要 + 全示例 rc=0/无 VUID；暴露的 bug 单独记档不顺手修 |
| R4 | `check/` 类文本档行尾（CRLF/无 BOM） | 若走文本档则照既有 `check/*.md` 约定；**推荐内联常量**以免新增文件 |

### 六、待拍板（3 项，均已在推荐值上）

> **订正（2026-10-07 复核）**：本节 3 项已按推荐值落地，且其「下一动作（T1 试点）」**已完成**——
> `example/Cull/GPUDrivenCull` 的帧摘要自检已在位（`GC_SELFCHECK` 家族，回归基线 `0x90E0745C2C7B8599`）。
> 本节保留作决策记录，不再作为待办入口。

1. **帧摘要存哪**：示例内**常量**（推荐，沿用 ATS 风格、不新增文件）／`check/<example>-digest.txt`；
2. **T2 强制范围**：**只主帧 mesh 批**（推荐）／连 overlay 一起（不建议：overlay 不是批路径）；
3. **opt-in API 命运**：**整体删除**（推荐，零兼容：`SetEnabled`/`SetDrawOverrideEnabled` 与示例门一起删）／保留开关（不建议）。

> 下一动作（T1 试点）：在 `example/Cull/GPUDrivenCull` 加 `GC_SELFCHECK=1`——第 8 帧回读颜色靶，
> 算 `{不同取值数, FNV-1a 64}` 与内联常量比对，PASS/FAIL 收成退出码；摘要常数由实跑记录。

---

## §11 T4c：shadow pass 进 GPU 路径（→ 写行/ICB 三样才可删）（2026-10-06 侦察）

> 本节保留**原始测量记录**；问题史 + 各方案结局 + 教训的叙事版（原 `future/gpu-cull-flicker-postmortem.md`）已于 2026-10-18 删档，存于 git 历史。

### 11.1 现状（量化，逐条有落点）

| 事实 | 落点 |
|---|---|
| 接管在**每 pass 的批次装配**里评估（不是每帧一次） | `TryApplyGPUCullOverride` ← `FinalizeBatches` ← `RunBatching`；`PrimitiveRenderPipeline` 调 cull/sort → 每 pass 一趟 |
| shadow pass **显式早退**（不接管） | `PrimitiveBatchPipeline.cpp:1171` `if (world->IsCurrentPassShadow()) return;`（理由：shadow 批共用同一 `materialBatches` 表） |
| 接管激活时 **CPU 剔除对两 pass 都跳** | `RunCulling` 的 `IsGPUCullFullTakeoverActive()` 早退；注释明写“shadow pass 同样跳（caster 全进；多画不出错，§8.3 首版口径）” |
| ⇒ 两 pass 的**输入项集相同**（全项） | 差别只在绘制集：主帧 = GPU 剔除后的子集；阴影 = 全量（今天由 CPU 写全量 ICB） |
| 阴影的绘制今天仍走 CPU 写行/ICB | `WriteMeshDrawCommands` / `EnsureMeshDrawParams` / `draw_batches` 就是它的活路（T4b 后已无其他活路） |
| 竞技场 = `EntityGPUTable::kCapacity(4096)` 行，按批切片（Σcapacity ≤ 4096，4 行对齐） | `GPUCullSystem` 切片装配 |
| 剔除派发**每帧一次**（帧槽分份） | `SetExecutionPhase(RenderFrameSync)`；**ring 深度 == 槽空间**（L2W 同类事故的不变量） |

### 11.2 目标与不可动的不变量

目标：**shadow pass 的间接命令也由 GPU 产**（CPU 不再写 ICB/参数行）⇒ 三样可删（T4c4）。
不可动：per-frame 的 ring/slot 不变量；竞技场行数 == 4096；切片 4 行对齐；`pc_root` 三表 push 的切片语义；帧槽间不得互盖（各槽各份）。

### 11.3 三条路线

| 路线 | 做法 | 阴影语义 | 规模 | 代价 / 风险 |
|---|---|---|---|---|
| **A 正确解** | 每 pass 各一次 GPU 剔除（阴影用**光锥 + caster 集**）⇒ 竞技场再分一份 | 正确（离屏 caster 也投影） | **大**：剔除从“每帧一次”变“每 pass 一次” ⇒ 帧槽/ring 机械要加 **pass 维** | R1：ring 不变量（L2W 同类）；R2：容量拍板 |
| **B 中等（建议）** | GPU 产一份“**全量直通**”行集给阴影（gather 加 no-cull 档，不需光锥） | **与今天逐字等价**（全量） | 中：gather 加档 + 阴影自己的切片（容量：分扇区或 8192） | R：容量拍板；档位表 + 契约 |
| **C 最省** | 阴影**复用主帧行集**（去早退 ⇒ 阴影也读 `gpu_row_base` 切片） | **退化**：只画主帧可见集（离屏 caster 不投影） | 小：去早退 + 阴影 push 用主帧相机行/槽 | R：**语义退化**（需拍板）；台架可能不敏感 ⇒ 不能靠摘要拤住 |

**建议 B**：与今天行为**逐字等价**（阴影=全量），不引入新语义，且不需要 pass 维的 ring 改造；A 的正确性收益（光锥剔除）只在阴影质量/性能上体现，可留作后续。
C 除非你明确接受“阴影只画主帧可见集”，否则不取。

> **拍板（2026-10-06）：走 B，且 T4c1 容量扩到 8192**（两扇区各 4096）。
> **T4c1 ✅ 已完成**：`kArenaPasses(2)` / `kArenaRowsPerPass(4096)` / `kArenaRows(8192)` +
> `ArenaSectorBase(pass)`（扇区 0 = 主帧、扇区 1 = 阴影）；gather 三表（l2w/cmds/mtl）行数改用
> `kArenaRows`（共 229 KB）。实体索引空间与批标签**不变**（标签按实体记“属哪一批”，与 pass 无关）。
> 判据：门 45/0 + Test 42 + **六台架摘要逐位不变** + 0 VUID。

### 11.4 任务切分（按 B）

- **T4c1** 竞技场按 pass 分扇区（主帧扇区 / 阴影扇区）——容量拍板：4096 不变（各半 2048）还是扩到 8192；不得跨帧槽互盖。
- **T4c2 ✅ 已完成**（行为中性地铺管道：块 0 仍受剔除、块 1 直通档待 T4c3 启用）：批行/计数**按 pass 分块**（`kBatchBlockStride = kMaxBatches + 1`，块内 = 256 行 + 1 溢出槽）；API 全带 pass（`BeginBatchTagWrite(pass)` 并清本块、`SetBatchLayout`/`EndBatchLayout`/`GetBatchLayout`/`MapCurrentBatchCounters`/计数偏移）；push 新增 `batch_block_stride`（**sizeof 104 → 112**）；shader 两遍循环（可见表口径逐字不变；阴影图只看深度、绘制序无关 ⇒ 不必搬批内序号）；gate 补第三对常量。判据：门 45/0 + Test 42 + **六台架与阴影两例摘要逐位不变** + 0 VUID。
- **T4c2 原计划（保留供追溯）** gather 加“直通档”（无剔除 ⇒ 行 = 批内项，序同 CPU 口径）+ 档位表与门/契约（同 `B.batch-info-struct-parity` 族）。
  ▸ **设计要点（本轮侦察）**：【１】**批布局（BatchInfoRow）需按 pass 分行** ——两 pass 各有自己的切片基址，
   而布局库今天每帧槽只有一份；【２】**剔除派发需覆盖两扇区**（主帧 pass 的 kMaxBatches 行 + 阴影 pass 的
   kMaxBatches 行 ⇒ 派发 2× 批数，带 pass 选择字段）。
- **T4c3 ✅ 已达成**（提交 `f4a4891f0`；前置独立修 = ICB 尺寸 `17c5e0841`）：阴影 pass 进 GPU 路径，两 pass 各自接管。
  真因链（首试失败的三层，全部已解）：
  ① **行内容并非 pass 无关** —— 4-ID 表由 collect 按 pass 写，gather 必须按
     `blk * pc.max_visible_count + entity_index` 读**本 pass 那一半**（T4c3b；`GPUCullSystem.cpp` 的 `static_assert` 守"4-ID 表 pass 容量 == 可见表上限"）；
  ② **批对象跨 pass 共享** —— 绘制侧消费的四样（切片行基址 / 容量 / 计数偏移 / 当前 pass）必须**按 pass 存**
     （`MaterialBatch::gpu_slice[RenderPassSlot::Count]` + `Render(...)` 穿 `pass_index`）；
     `gpu_driven_override` **保持单份**（批内 sort/ensure/build/write 的十处跳过是"本帧装配"语义，与 pass 无关）；
  ③ **ICB 尺寸漏改（T4c1）** —— 三张 gather 表已按 `kArenaRows`(8192) 扩，`CreateIndirectMeshTaskBuffer` 却留
     `kMaxVisibleEntities`(4096 条 = 49152 B) ⇒ 阴影扇区绘制偏移 `4096×12 = 49152` + 20 条 ⇒ 49376 越界
     ⇒ `VUID-vkCmdDrawMeshTasksIndirectCountEXT` + AMDVLK 丢设备（表现为 rc=124"挂死"）。
     ⇒ **凡扩容竞技场，把"按容量分配"的每一处列出来数一遍**（同一条坑的第二个实例：上次漏的是计数清零长度）。
  判据：ATS `rc=0` + `[ATSSELF] PASS`（第 1 帧判据，覆盖启动帧）+ 追踪 `pass={0:2,1:1}`（阴影真进接管）；
  `ShadowMap` `0xA8A369516CB04571` / `CascadeShadowMap` `0x1CDAADEBF18B7CE4` **逐位不变**；六台架不变；门 45/契约 rc=0；〔2026-10-11：ShadowMap 台架已删除（旧 shadow-map 直采示例，CSM 路线取代；如需退化为单张阴影图，把 CascadeShadowMap.cpp 的 `cfg.cascade_count` 设 1 即可）——本行基线仅适用于删除前〕
  阴影两例敏感性（`SM_FREEZE=0` / `CSM_AUTOWALK=1`）如预期变值；验证层 VUID 0、`result -4` 0。
  ⇒ **T4c4 入口条件已齐**：`WriteMeshDrawCommands` / `EnsureMeshDrawParams` / `draw_batches` 赖以存活的
    守卫早退（`IsCurrentPassShadow()` 处 return）已不存在，阴影批不再依赖 CPU 账。
- **T4c4 ✅ 全部完成（`6d1df5083` → `82e488103`）**：删 CPU 绘制链 ⇒ **CPU 渲染路径已删净**。
  - **T4c4-1 量化前提**（方法固化）：九例逐个抓 `ENGAGED batches=x/y`（要求 x==y）+ guard 拒绝次数（要求 0）
    ⇒ "无批走 CPU 绘制"。⚠ **ENGAGED 行缺失 = 静默退回 CPU 账**（像素可能仍一样）⇒ 必看项。
  - **T4c4-2**：删 `WriteMeshDrawCommands` / `ReallocICB` / CPU 自有 ICB 分支 + 变死的
    `ReleaseICB`/`BuildICBNames`；契约 needle 按**意图**换形（旧表达式转禁复活 + 新增 gather 侧正向 needle
    钉 "组数只在 gather 内按 `(element_count + group_size - 1u) / group_size` 算出"）。
  ⭐⭐ **删除边界（本线最贵的教训）**：**"零拒绝 ⇒ 无批走 CPU 绘制"只覆盖"绘制"，不覆盖"数据物化"**。
  `PrimitiveBatchPipeline::EnsureMeshDrawParams` 是 **mesh per-draw 参数池行的物化者**（gather 从该池取
  `mdph` 的 `element_count`/`is_indexed`）⇒ 按绘制语义删它 ⇒ 池行过期 ⇒ **Cascade 画面变**
  （`0x1BAB088CA3B63B3C` vs 基线 `0x1CDAADEBF18B7CE4`）。**每删一处先问：绘制侧还是写 gather 消费的数据？**
  ⚠ 配套纪律：引用检查用 `rg`/搜索工具（shell `grep '\b'` 静默全 0 命中，据此删会断构建）；
  死码表在**主删除稳定后**再取；"命中行数 ≠ 有调用"（`GPUCullSystem.cpp` 对 `ReleaseICB` 只是注释提及）。
  - **T4c4-3 ✅**（`49d9e2430`）：删 CPU 绘制批对象链 —— `MaterialBatch::draw_batches(*)`、`struct DrawBatch`、
    `DrawBatchArray`、`Renderer::Draw()`、`Render` 的 `batches`+`batch_count` 入参、Render 里的 CPU 循环、
    三个 CPU 累积计数成员、`BuildBatches`、`SortBatchItems`、`last_compaction_stats`（删前 rg 复查外部使用者=0）；
    ⚠ **2026-10-10 订正**：`BuildBatches` / `SortBatchItems` 已随 T4c4 删除，现在**全仓都不存在**（本行从"计划删除"变成"已不存在"，无需再动）；
    两个调用方（`PrimitiveRenderSystem` + `PrimitiveOverlayRenderSystem`，后者属 RenderDebug 相位边界）**同批**改；
    flush 门 ⇒ `if (owner_batch && owner_batch->gpu_driven_override)`。
    契约 needle 按意图换形三条（`RenderItem::MeshDrawKey draw_key;` / `next_item->draw_key == draw_key` /
    `if (batch.gpu_driven_override)`（16 空格缩进）⇒ 转禁复活 + 正向钉「行键只在采集期填」）。
    ⚠ **编辑工具纪律**（本轮为它们多花了好几个构建周期）：批量改动脚本**先干跑预检锚点**（全中才写盘）；
    纯声明无函数体、不能大括号配平切（按整块文本删）；函数定义首参与函数名同行、其余参数按实际对齐宽度缩进；
    往字符串追加文字注意切尾长度（`" },` 是 4 字符）；契约表解析要按条目花括号配对且限定区间边界
    （否则别的表条目会算进来造成假阳性）；**失败的权威信息是测试自己打的那条**。
  - **T4c4-4 ✅**（`82e488103`）：清 `MaterialBatch` 死字段 + 文档回扫 + 台账。**逐字段取证后只删真死件**：
    `debug_blocks_logged`（0 读者；该行注释本身已损坏 —— 两个 `///<` 串在一行）与 `icb_count_buffer_offset`
    （引擎 0 读者 —— 绘制侧自 T4c3c 起取 `gpu_slice[pass].count_buffer_offset`，批级偏移被取代；唯一写者是探针示例
    `ApiValidation/ComputeIndirectCount`，其死写入一并删）。其余字段（`own_*` / `l2w_buffer` / `mesh_draw_params_*` /
    `icb_count_buffer` / `gpu_slice` / `transform_buffer` / `renderer` …）逐个查证**仍有真实读者 ⇒ 一律不动**。
    同步修 `GPUCullSystem.h` 里已过时的「绘制侧 = icb_count_buffer_offset」注释。
    判据：清树全量重编 rc=0；八台架摘要逐位不变（D=0）+ **ENGAGED x==y**（含 Cascade 2/2、AlphaModes 4/4）+ 拒绝 0；
    ATS rc=0；门 45/0；TestCSM rc=0；ATU rc=0 且 05149=0。
  ⇒ **T4c4 全线完成 ⇒ §11 T4c 结案：引擎内已无 CPU 绘制路径。**

- **闪烁取证（"有 shadow 的示例每 3 帧一次"，2026-10-06，未结）**：
  > **订正（2026-10-07 复核）—— 本块已结案**：症状（含阴影示例每 3 帧一次整帧异常）由 §11.9 根因 +
  > §11.10 修复验证收口（ATS 图 0/1/2 每种 FNV 唯一）；本块指出的"便捷重载 `RenderTo(rt,clear,dt,cull)`
  > 不设 `is_shadow_pass`"由 §11.16 改为**目标推导**（`req.target != saved_target`，`71cc6390c`）根治。
  > ⇒ **仅剩本块内的选项②需单独立项**：「行集按帧槽分份」—— 竞技场（cmds/l2w/mtl/ICB）目前只按 pass
  > 分扇区、**不按帧槽** ⇒ 在飞帧之间会互相覆写（理论竞态，与已修症状无关，**独立待拍板**）。
  > 以下原文保留作取证记录：
  - **已测量（可靠）**：`ShadowMap` 用引擎现成的 `SyncForParityReadback` + `MapCurrentBatchCounters` 逐帧读数
    ⇒ 主帧 pass 的 0 号批计数在**交换链槽 1 上恒为 0**，槽 0/2 恒为 14 ⇒ 与"3 张交换链图"一一对应。
  - **已排除（修正测量后）**：槽 1 的**标签**（本槽有效标签=14 ✓）、**布局**（绘制行 `max=14`、计数偏移 2056
    = 槽 1 正确算术 ✓）、**批数**（`批=1` ✓）、**三个按槽地址**（`pc.batch_*_addr = 基址 + batch_slot × 槽步长` ✓）。
    ⚠ **我自己的一次测量错**：数标签时用 `Map(0, …)` 读了**第 0 槽** ⇒ 据此得的结论作废；
    改按 `Map(slot × slot_bytes, …)` 后才是真值（教训与"别单次采样"同族：**先确认量的是不是那个槽**）。
  - **最强线索**：`[TAGTRACE]` 实测**离屏（帧号=4）那一路的批装配 `pass_index=0`、`shadow_flag=0`**；
    而 `EnvironmentSystem.cpp:378/410` 对真阴影 pass **确实**签发了 `req.is_shadow_pass = true` ⇒ 说明帧号 4
    的那次装配**不是（或不只是）阴影 pass**，或标志在帧内被切换过（`Context::RenderTo`：
    `is_current_pass_shadow = req.is_shadow_pass`(:669) → 渲染 → `= false`(:735) → **之后才**归还主帧槽）。
    另一处便捷重载 `RenderTo(rt, clear, dt, cull)`(:815) **不设** `is_shadow_pass` ⇒ 走它的离屏 pass 会被判成
    主帧 pass（块 0 / 扇区 0）⇒ 与主帧**共用同一份切片与竞技场扇区** ⇒ 互相覆盖。
  - **两条修法（都在当前阶段）**：① **判定与槽同生命周期** —— 离屏 pass 期间 `is_current_pass_shadow` 与帧槽
    必须同源同置同清（把 :669/:735 与两次 `SetFrameIndex` 绑成一条），并给便捷重载补 `is_shadow_pass`；
    ② **行集按帧槽分份** —— 竞技场（cmds/l2w/mtl/ICB 命令）目前只按 pass 分扇区、**不按帧槽** ⇒
    在飞帧之间会互相覆写；按帧槽分份（2.6 MB 量级）可同时消掉跨帧竞态。
  - **下一步**：重跑"pass × 槽"组合计数取证（上一轮该测量因进程早退失败，**当次无数据**）⇒ 按 ① 收敛判定
    ⇒ ② 作为竞态根治。临时取证代码已从库中清除（`example/Shadow/ShadowMap.cpp`、`src/ecs/support/GPUCullSystem.cpp`）。

- **闪烁已量化（2026-10-06，决定性数据）**：`SM_SELFCHECK=1 FRAMEDIGEST_PERFRAME=12` 逐帧摘要——
  `frame=1/2 distinct=256 ✓` → **`frame=3 distinct=4 fnv=0x43D07F2DF9290383 <== 与首帧不同`** → `4/5 ✓` → `6` 又变 →
  `PERFRAME FAIL：12 帧，与首帧不同的帧数=4`（12/3=4 ⇒ **完全吻合"每 3 帧一次"**）。
  ⭐ **症状是"整帧近乎空"（distinct 256→4）**，比"某物体缺失"更根本 ⇒ 嫌疑指向**整帧的绘制/呈现路径**
  （某一张交换链图那一帧基本没画东西），而不是单个批的行集/切片。
  ⚠ **`FRAMEDIGEST_PERFRAME=N` 此前一直失效**（真缺陷，已修）：读回钩子是**一次性**的
  （`VKSwapchainRenderTarget.cpp:216` 执行后即清除）⇒ 逐帧模式必须**自续装**；
  修前只打第 1 帧就停摆 ⇒ 这也是"两份二进制第 8 帧摘要总一样"的原因（`FrameDigest.h:85` 自己记着
  "每 3 帧缺一帧的摘要在第 8 帧采样恰好避开"——8 不是 3 的倍数）。
- **竞技场按帧槽分份（修法 ②）：实测对该症状完全中性，已回退**。三处改动（三表+ICB 按 `HGL_FRAME_SLOT_TOTAL`
  扩份、切片起点 = 帧槽基址 + pass 扇区）落地后逐帧摘要**逐位不变**（`D=4` 不变）⇒ **不是病因**。
  按"未被判据证明的改动不留"回退；**它背后的跨帧竞态仍是理论风险**（行集只按 pass 分扇区、不按帧槽），
  但与本次症状无关 ⇒ **推迟**，将来真要动时记住它只需三处、且第四处出现就说明有代码在拆解 `row_base`。
- **下一步（新线索）**：查"某一张交换链图那一帧整帧近乎空"——按 image 序号打印该帧的绘制量/批计数，
  或把 `SM_SELFCHECK` 的逐帧摘要**打上 image 序号**，确认空白帧是否恒为同一张图；再判定是"该帧没画"
  还是"画了但读的是别一张"（呈现/回读时序）。

- **闪烁定位收紧（2026-10-06，决定性）**：`FrameDigest.h` 逐帧摘要已**接上 image 序号**（harness 改动），实测——
  `image=0: 帧[2,5,8,…] distinct=[256] ✓` / **`image=1: 帧[3,6,9,…] distinct=[4]`（5/5 全空）** / `image=2 ✓`
  ⇒ **空白帧恒为同一张交换链图（image=1）**，即"三张图里有一张从来没被画出内容"（distinct=4 = 只剩清屏色）。
  这与最早那条测量（**槽 1 的每批计数读出来恒 0**）是同一件事（槽号 = 图号）。
- ⭐⭐ **硬矛盾（下一步的靶心）**：同一次追踪里，**槽 1 那一帧的 CPU 侧链条完全健康**——
  `frame=1 pass=0 出口=ok 槽=1 就绪=1 几何批=1 辖批=1 items=14 标签项=14 布局容量和=14（应相等）`
  + `GPU cull draw override ENGAGED: batches=1/1 items=14`
  + `mesh indirect flush engaged: first=0 count=14（gpu_driven=1 row_base=0 capacity=14）`
  ⇒ 而它的 **countBuffer 里是 0**（最早测量）⇒ **绘制以 count=14 发出、GPU 侧计数器却是 0**。
  排除项："没接管"（ENGAGED ✓）、"没发绘制"（flush ✓）、"就绪门拦住"（**四个槽 `就绪` 全为 1** ✗）、
  "标签/布局/批数/槽地址错"（逐项已验 ✓）。
  ⇒ 剩余两个候选（都指向 **GPU 侧次序**）：
  ① **剔除派发所在 cmd 与主帧绘制所在 cmd 之间缺少正确的先后**（计数器写入晚于绘制读取）；
  ② **计数器清零落在绘制读取之后**（清零与派发同 cmd、与绘制跨 cmd）。
  **靶心判据**：给"计数器的写"与"绘制的读"各打一个 GPU 时间戳（或直接用引擎既有 parity 回读在同一帧内前后各读一次），
  确认 0 是"还没写"还是"被清了"。
- ⚠ **本轮我自己踩的两个读数伪影（都已作废，勿重蹈）**：
  ① 把两个**长度不同**的日志列表**按位置 zip** ⇒ 得到"派发槽恒比绘制槽大 1"的**假恒定偏移**（真因是两个列表
     起点不同，23922 vs 23920 条）；据此推出的"还原时重读图号"改动**实测中性、已回退**。
  ② 在**截断样本**上断言"没有 pass_index=1"（只看了前 26 行）。
  ⇒ 纪律：**先确认量的是那个对象/槽，再拿"全量分布"下结论**；不同长度的列表要先对齐再比较。

> **订正（2026-10-07）**：以上 §11.4 尾部的「下一步（新线索）」「硬矛盾（下一步的靶心）」两条线索已全部收口——分别由紧随其后的「⭐⭐ 真根因与修复（已结案）」（闪烁案：`is_shadow_pass` 漏设 ⇒ 两 pass 共享切片的竞争赢家）、§11.9/§11.10（第二例）、§11.11–§11.15（第三例）。**勿再按它们重走取证。**

## ⭐⭐ 真根因与修复（2026-10-06，已结案）

**症状**：接收阴影的示例在**其中一张交换链图**上整帧近乎空（`distinct 256→4`），每 3 帧一次，恒为同一 image；
且**改动看似无关的提交（甚至只加注释）会翻转判定**。

**根因**：`RenderPassRequest.is_shadow_pass` 是**调用方设置、可以漏设**的旗标。
`ShadowMapApp::RenderShadowMap()` 把**主世界**渲染进 shadow RT 时**没设它**（`RenderTo(rt,clear,dt,cull)` 便捷重载同样没设）
⇒ 那次离屏渲染的 `is_current_pass_shadow` 为 false ⇒ 批装配取 `pass_index = Main`
⇒ **把两 pass 共享的批对象的 `gpu_slice[Main]` 改写成离屏自己的切片** ⇒ 主帧绘制随后读到它、
去读**别的帧槽**的计数（0）⇒ 该图整帧空。
"只加注释也翻转"的机理：**两条路径写同一份切片，谁最后写由时序决定**；每份二进制的代码布局固定了时序
⇒ 每个提交的判定"稳定但不同"（这正是我数轮误判的根源——不是代码变了，是竞争赢家变了）。

**修复**：调用方声明 `req.is_shadow_pass = true`（示例侧一行）。
**验证（全套绿）**：逐帧摘要 30 帧 ×3 次 ⇒ 三图取值集合全为 `[256]`（闪烁消除）；
`ShadowMap` `0xA8A369516CB04571` / `CascadeShadowMap` `0x1CDAADEBF18B7CE4` **逐位不变**〔2026-10-11：ShadowMap 台架已删除（旧 shadow-map 直采示例，CSM 路线取代；如需退化为单张阴影图，把 CascadeShadowMap.cpp 的 `cfg.cascade_count` 设 1 即可）——本行基线仅适用于删除前〕
（阴影 pass 改走 `pass=1` 后渲染结果不变——与 T4c2 的设计判断一致：阴影图只看深度、与绘制序无关）；
`AlphaTestShadow` 契约 rc=0（四个子契约全绿）；门绿。

**遗留两处（同一坑，待拍板方向）**：
1. `Context::RenderTo(rt, clear, dt, cull)` 便捷重载**不设旗标**，调用者 `SceneGraph/module/OffscreenWorld.cpp:147`
   ⇒ 那个离屏世界仍被判成主帧 pass（同一后果，只是尚未以闪烁形式暴露）。
2. `TestCSMIncrementalPass` 的契约只断言"**默认值必须为 false**"，**没守"离屏渲染必须声明自身 pass 身份"**
   ——这正是该 bug 能长期存活的制度原因。
   ⇒ **引擎侧正解**（`46e2da935` 落地；判据后由 §11.16 修正）：pass 身份**由目标推导**或改成不可漏设的字段，
   并把契约从"默认值"改为"离屏必须声明/推导正确"。

### 第二例：`AlphaTestShadow` / `CascadeShadowMap` 的每图内容差异（**已结案**：第二例 = §11.9 根因 + §11.10 修复验证；本节另一线索「每图一帧全空」= 第三例，由 §11.15 收口。以下为当时原文，保留不改）

- **症状**：同一张交换链图内的各帧内容**完全一致**（同图 FNV 唯一 ✓），但 **image1 与 image0/2 不同**
  （ATS：`0x21AB6CF6CBB6BA49` vs `0x7E3E43A9365E4A13`，`distinct 254 vs 255`；24 帧中差异 8~10 帧）。
  粗细判据（`distinct<50`）**看不见**它——必须用"同图 FNV 唯一"才能暴露（教训：判据要到字节级）。
- **已排除（隔离实验，2026-10-06）**：给 `EnvironmentSystem` 的两条阴影路径加 `ENV_NO_SHADOW=1` 跳过**整个**
  阴影 pass（装配 + 相机发布 + 派发全跳过）⇒ **ATS 依旧每图不一致** ⇒ **与阴影 pass 无关**
  （ShadowMap 那一例确实是阴影相关，已修；这两例不是）。
- **用户提供的 RenderDoc 事实（ATS 缺地面帧）**：那条 `vkCmdDrawMeshTasksIndirectCountEXT` 的
  `countBufferOffset = 2056` ⇒ **折算槽号 = 1 = 该帧图号（读对了）**；`maxDrawCount = 1`（切片容量对）；
  **唯独计数为 0** ⇒ ⇒ **读的位置正确，是"该槽的计数没被产出"**。
  结合"只有大幅面的地面缺失、近处方块仍在"⇒ 指向 **该帧主帧剔除没有为这个批产出计数**
  （可见性判定把它剔掉，或该批未进派发）。
- **我自己的取证失误（5 次，全部作废，勿重蹈）**：按位置 zip 长度不同的日志、按条数切帧（224/30 非整数）、
  用会回绕的帧号当字典键（不同周期同号帧被合并）、锚点前后开 40 行窗口（混入三帧）、正则漏 `WxH` 段。
  ⇒ **日志里"绘制侧"没有逐帧键**，才逼出这一连串对齐错误。
- **下一步（已就位）**：① 给 `PipelineMaterialRenderer` 的 flush 追踪行**加帧号**（一行）⇒ 对齐由构造保证；
  ② 与派发行逐帧并排，专看该批的计数与"该帧相机行/可见性行"；③ 若确认被剔，靶心即 `cull_ok` 的输入。
  ⭐ **三例的现成判据**：`<前缀>_SELFCHECK=1 FRAMEDIGEST_PERFRAME=N` ⇒ 判词 `与首帧不同的帧数=0` 才通过
  （ShadowMap 现为 0 ✓；ATS/Cascade 为 8~10 ✗）。

- **负结果（2026-10-06，逐条排除，别再重走）**：
  ① **"计数器清零→compute" 与 "compute→indirect 消费" 两条 barrier 都在且掩码正确**
     （`TRANSFER/TRANSFER_WRITE → COMPUTE/SHADER_READ|WRITE`；`COMPUTE/SHADER_WRITE →
     DRAW_INDIRECT/INDIRECT_COMMAND_READ`）⇒ **"缺 barrier 导致读到清掉的 0"不成立**。
     但源码注释里那句"跨 cmd 靠同队列 submission order 保证可见性"**本身是错的**（那只保证执行次序，
     不建立内存可见性）——代码侥幸写对，**注释已改对**（改这条时别把 dst 阶段 `DRAW_INDIRECT` 删掉）。
  ② **槽就绪门不拦**：四个槽（0/1/2/4）的 `就绪` 实测**全为 1**。
  ③ **接管与发出链健康**：槽 1 那帧 `出口=ok 辖批=1 items=14 标签项=14 布局容量和=14`、
     `ENGAGED batches=1/1`、`flush ... count=14`。
  ④ **槽地址/标签/布局/批数**逐项已验正确（见上一节）。
  ⇒ **真正剩余的疑点只剩一个**：绘制"以 count=14 发出"与"countBuffer 读出 0"之间，**读回端是否在读同一槽**——
    即 `MapCurrentBatchCounters`（用 `context_->GetFrameIndex()`）与逐帧摘要/探针标签（用
    `rt->GetCurrentFrameIndex()`）**是否同源**。若不同源，则"槽 1 恒 0"这个*标签*本身就是错的，
    需要改用**同一个**取槽源再量一次（这也是下一轮的第一个动作）。
  ⚠ 诊断代码的失败路径**不许静默**（本轮探针走了 `rt`/`gpu_cull` 为空的无声分支 ⇒ 输出 0 行、
    既看不出"探针没装上"也看不出"链路没走到"，白等一个构建周期）。

- **下一步该换手段（黑盒探针已连续三次失败，别再重复）**：
  ① **帧首/帧尾双读**（自装回读钩子 + `ComputeFrameDigest`）——设计上成立，实测钩子未触发（0 行），
     与共用件那次"一次性钩子"同类；**再碰回读钩子前，先在钩子体第一行无条件 `printf` 验证它到底有没有被调用**。
  ② **更值得做的一步：定性归属**——在 **T4c-ini（帧槽就绪门）/ T4c3 之前**的提交上构建同一示例、
     跑同一套**逐帧摘要**，看 `image=1` 是否同样恒空：
     - 同样恒空 ⇒ 与近期改动无关（更早的帧槽/呈现路径问题）；
     - 不再恒空 ⇒ 逐提交二分，范围立刻收敛到那几笔。
  ③ 判据一律用**逐帧摘要**（`<PREFIX>_SELFCHECK=1 FRAMEDIGEST_PERFRAME=N` ⇒ `与首帧不同的帧数=0` 即通过）。

  --- 以下为首试失败时的原始记录（保留作对照）---
删「只在主帧 pass 接管」的早退（它正是 shadow 至今走 CPU 写行/ICB 的唯一原因）；
  `pass_index = IsCurrentPassShadow() ? 1 : 0` 同时定**块**（`BatchBlockBase`）与**扇区**（`ArenaSectorBase`）；切片起点从本 pass 的扇区基址起
  （主帧 0／阴影 4096）⇒ 直通档真正启用；`ApplyGPUCullOverrideHook` 带 pass；ENGAGED 日志按 pass 各记一次。
  判据：**阴影两例摘要逐位不变**（接管前后画面一致 = 直通档 ≡ CPU 全量 ICB）+ 六台架摘要不变 + 门/契约。
  **原计划（保留供追溯）** 阴影 pass 消费自己的切片 + 去掉早退。
  ❗ **实测结果（已回退）**：ShadowMap 摘要逐位不变、六台架不变，但 **ATS 契约破（rc=1）+ Cascade 挂**（两事例均 rc=124）。
  根因：GPU 行的内容取自**实体行**，而实体行**每帧只有一份**、内容是**最后一个 pass**（主帧）写入的
  ⇒ 阴影行里带的是**主帧的材质变体**；而阴影程序与材质行本是**按 pass 解析**的
  （`GetShadowProgram` / `shadow_pass_idsum` 那套就是实例）⇒ 遮罩/变体用错（ATS 锭空契约破），Cascade 挂是同一错配在增量级联路径上的后果。
  → **B 路线的前提被推翻**（原以为“与今天逐字等价”）：行内容**并非 pass 无关**。
  出路：（a）给阴影一份**自己的实体行风味**（实体行空间×2 + collect 按 pass 写，shader 按 pass 取行基址）；
  （b）让 shader 侧**按阴影程序解析材质**（需一张每材质的阴影变体表）；（c）缩范围：阴影暂留 CPU 路径（T4c4 删三样继续受阻）。
  本轮已回退到 T4c2 的绿色状态（仅保留一处**真 bug 修**：计数清零未跟上分块尺寸）。
  ✅ **路线 (a) 第一步已落（T4c3a）**：新增单头 `RenderPassSlot.h`（Main/Shadow/Count）；
  **实体 4-ID 表按 pass 分半**（行空间→每 pass 一半，`PassBase(pass)+index`；设备镜像 512 KB → 1 MB），
  收集侧 `SetRenderIDs(pass, …)` 写本 pass 那一半；行为中性（阴影半边待启用）。
  ✅ **T4c3b/c 已完成**（提交 `f4a4891f0`）；原交接内容：`entity_gpu_cull.glsl` 的 4-ID 读放进两遍循环内（行号 = `blk * pc.max_visible_count + entity_index`，
  容量与可见表同值故不加 push 字段）+ 重启两 pass 接管（重放 T4c3 补丁，并把接管侧那处 `Row(0u,…)` 改穿 pass）。
  ▸ ~~**R3 定稿**：阴影批的 `pc_root` 必须用 GPU 行所属的槽、不能用阴影相机行~~
   ❌ **该推断已被推翻（T4c3 定稿）**：竞技场三表（l2w/cmds/mtl）**不以相机为维**（按帧槽分份），
   且行内容全是 pass 无关的（`transform_id` / `geometry_id`+切片内序号 / 材质位）⇒ 谁写都等价；
   而 `pc_root.camera_row` 的用途是让 mesh shader 读**该 pass 的相机矩阵** ⇒ 阴影 pass 就该用**光源相机行**（既有代码本就如此，无需改）。
   真正要求只有一条：该相机行的 global-addresses 槽必须已发布。
   ⚠ 已知边界：某批**只存在于阴影 pass**时，主帧那次 `BeginBatchTagWrite` 会清掉整表 ⇒ 该批阴影扇区无行；
   当前台架/示例不涉及（两侧批成员一致），将来真出现需按 pass 各持标签数组。
- **T4c4** 删 `WriteMeshDrawCommands` / `EnsureMeshDrawParams`（`MaterialBatch` 版）/ `draw_batches`（含 `DrawBatch` 结构、CPU ICB 缓冲、`ReallocICB`；注意 `Geometry::EnsureMeshDrawParams` 是**几何侧另一个**，不能误删）。

**判据**：阴影三例（`ShadowMap` / `AlphaTestShadow` / `CascadeShadowMap`）摘要不变 + 六台架摘要不变 + 门/契约 + 0 VUID/0 ERROR。

> **判据前置 ✅ 已完成**：`ShadowMap` / `CascadeShadowMap` 已按 CV_/GC_/PG_ 同一打法补上帧摘要自检
> （`SM_SELFCHECK` / `CSM_SELFCHECK`，第 8 帧色彩靶回读；退出码即契约）。实跑记录：
> ShadowMap **`0xA8A369516CB04571`**（1280×720）、Cascade **`0x1CDAADEBF18B7CE4`**（1600×900）。〔2026-10-11：ShadowMap 台架已删除（旧 shadow-map 直采示例，CSM 路线取代；如需退化为单张阴影图，把 CascadeShadowMap.cpp 的 `cfg.cascade_count` 设 1 即可）——本行基线仅适用于删除前〕
> **必须从第 0 帧起冻结动画**（ShadowMap 网格自旋 + **光源环绕**；Cascade 时长驱动轨道），
> 否则摘要随时序漂移（实测两次不同）；ShadowMap 另给 `SM_FREEZE=0` 专供敏感性验证。
> 敏感性已证：`SM_FREEZE=0` 与 `CSM_AUTOWALK=1` 均使摘要变 + rc=1。
> ⚠ 两例必须**以仓库根为 cwd、经 python subprocess 启动**（从 bash 直起 ⇒ `GLSLCompiler` 插件加载失败、rc=127 零输出）。

**风险**：R1 扇区容量不足时的口径（与 T4b 同：fail-fast）；R2 参考系统的对拍是否要跟着分 pass；R3 `pc_root` 的相机行在阴影下取谁（与切片槽一致）；R4 `CV_STRESS` 类压测在两个扇区下的容量口径。

### §11.9 第二例根因（ATS/Cascade 每图一帧内容异常）—— 判据盲区 + 标签表缺 pass 维度

**症状**：`AlphaTestShadow` / `CascadeShadowMap` 每 3 帧有一帧内容异常（地面消失），
`ShadowMap` 无（其离屏 pass 与主帧同槽，掩盖了差异）；帧摘要按图号分组 ⇒ 恒定某一张图不同。

**定位过程（全部为实测，非推断）**：
1. 逐帧计数回读（示例侧探针，`printf` 通路）：`c0`=0 恒出现在**同一图槽**，`c1`=2 恒正常；
2. 引擎自带 CPU↔GPU 剔除对拍（`CPUCullReferenceSystem`）报 `mismatch=0` —— 但发现
   **采样周期 60 与交换链图数 3 同倍（60 % 3 == 0）⇒ 判据永远只落在图槽 0**，
   结构上看不到"某一张图整体异常"。把周期改为 61（与 3 互质）后采样扫过所有图槽；
3. 修好采样后，引擎自己的两本账判据给出决定性数据（**每次采样同形**）：
   `Σcount=2 < global=3`、**`overflow=1`**、可见性对拍 `CPU=3 GPU=3` ——
   即：**可见性判定正确，但 gather 少写一行、且有实体被当"切片溢出"丢弃**。

**根因（代码层不对称，两处"两 pass 同源"假设不成立）**：
- **批**分区表**（`batch_info`）与**计数表**都按 pass 分块（`kArenaPasses × kBatchBlockStride`），
  **唯独批标签表只有帧槽维度**：`batch_tag_addr_ + slot * kMaxVisibleEntities * 4`；
  `BeginBatchTagWrite(pass)` 更是**整表清空重填**（`GPUCullSystem.cpp:410-412`）⇒
  一帧内**后装配的 pass 会盖掉先装配 pass 的标签**；
  而着色器查表也不带 pass 项：`entity_gpu_cull.glsl:323` 的
  `BatchTagRef(pc.batch_tag_addr).tags[entity_index]`（同一文件 `:381` 的 4-ID 表**已**按
  `blk * pc.max_visible_count + entity_index` 分半 —— 标签表漏了同款处理）。
- `pc.batch_count` 亦为**单份共享值**（`GPUCullSystem.h:257`，着色器 `:56` 注释"两 pass 同值"），
  而着色器**一次派发内自己循环写两个 pass 块**（`:329` `blk` 循环）⇒
  后 `EndBatchLayout` 的批数会作用到两个 pass（实测参考实现读到 `batches=1`，即阴影的批数）。
⇒ 静止物体被按**另一个 pass 的批索引**写行 ⇒ 全部挤进批 0（其 `capacity=1`）⇒ 溢出 ⇒ 该行被丢。

**修复方案（照抄同文件既有范式，6 处）**：
1. `tag_slot_bytes` 乘 `kArenaPasses`（缓冲按 pass 分块）；
2. `BeginBatchTagWrite(pass)`：按 `pass` 块 Map，且**只清本块**；
3. `entity_gpu_cull.glsl:323`：改为 `tags[blk * pc.max_visible_count + entity_index]`（1 行）；
4. `pc.batch_count` 改为按 pass 取值（`batch_count_[kArenaPasses]`），
   `GetBatchCount()` 返回主帧那份（参考实现读的就是主帧）；
5. 参考实现同步（对拍口径按主帧）；
6. 判据：`Σ批计数 == 全局幸存` 在**全量接管**时为**严格相等**（现仅查 `sum > global` 单向）。

**遗留**：本项（示例侧计数探针、布局打印、采样周期诊断口）已全部回退或转正；
临时探针纪律见技能 `ulre-entity-gpu-cull`（探针必须先自证能出声；长跑输出**不要**套 `tail`，
缓冲会让健康运行看起来像卡死 —— 本轮两次误判均源于此）。

### §11.10 第二例修复与验证（标签表/批数补齐 pass 维度）

按 §11.9 方案落地，实际改动 **3 文件 6 处**：

| 处 | 改动 |
|---|---|
| `GPUCullSystem.h` | `batch_count_` → `batch_count_[kArenaPasses]`；`GetBatchCount()` 返回**主帧**那份；`GetBatchLayout` 界检查按 pass |
| `GPUCullSystem.cpp` | 标签表 `tag_slot_bytes` 乘 `kArenaPasses`；`BeginBatchTagWrite(pass)` 只 Map/只清**本块**；`EndBatchLayout` 存按 pass；派发下发 `pc.batch_count = batch_count_[0]`；**`pc.batch_tag_addr` 槽步长同步改为 `kArenaPasses × kMaxVisibleEntities`**（pass 项由着色器加，故基址不带 pass）；溢出槽偏移与逐批前置校验同步 |
| `entity_gpu_cull.glsl` | 标签查表改为**两个 pass 各查自己那一块**（`tags[blk * pc.max_visible_count + entity_index]`，与同文件 4-ID 表同式）；早退语义拆分：**两块都无**才 `return`，单块无则 `continue`（否则会连带跳过材质行写入） |

**踩到的两个坑（均已修，记录以免重犯）**：
1. **漏改 `pc.batch_tag_addr` 的槽步长** —— 缓冲已改为"每帧槽两块"，但下发基址仍用旧步长 ⇒ 槽 ≥ 1 时指进上一个槽的块 1 ⇒ 标签全错。表现：**D3 契约 `receive_shadow` 直接 FAIL（变红 0 px，阴影整体消失）**、坏图 `distinct` 塌到 4。⇒ 改寻址类改动必须把**缓冲尺寸 / 写入映射 / 下发基址 / 着色器索引**四处列成清单逐一对账。
2. **枚举使用点被 `head` 截断** ⇒ 漏掉 `GetOverflowCounterByteOffset` 与 `ApplyGPUCullOverrideHook` 里的两处 `batch_count_`（编译器只报第一个）。⇒ 改成员形态时用 `rg <名>` **去重列出全部**，不靠编译器逐个发现。

**验证（全部实测）**：
- ATS 逐帧摘要按图号分组：**图 0/1/2 的 FNV 种类各为 1**（修复前坏图恒 2 种、`distinct=4` 空图）；`distinct` 回到 255/254/255；
- ATS 契约 `ATS_SELFCHECK=1`：**D1 c0 PASS（填充率 57.6%）/ D3 receive_shadow PASS（变红 18187 px）/ D3 bias_multiplier PASS / D4 行池写入 PASS / selfcheck PASS (exit 0)**；
- `TestCSMIncrementalPass`（自仓库根）：**All CSM Incremental Pass Contract Tests PASSED**；
- `ShadowMap` / `CascadeShadowMap`：每图 FNV 种类=1、`distinct=256`（注：两者自检首帧即退出，每图仅 1 帧样本，"逐帧稳定"这一条证据弱，强证在 ATS）。

**§11.10 补充：回归面（修复后实测）**

| 台架 | 判据 | 结果 |
|---|---|---|
| AlphaTestShadow | 逐帧摘要按图号（10 帧/图） | 图 0/1/2 各 1 种 FNV；契约 D1/D3/D4 全 PASS（receive_shadow 变红 18187 px） |
| CascadeShadowMap / ShadowMap | 逐帧摘要按图号 | 各图 1 种 FNV、distinct=256 |
| AlphaModes / CoverageVariants / PBRSpheres / LoadScene / PlaneGrid3D | 逐帧摘要按图号 | 各图 1 种 FNV |
| **GPUDrivenCull** | **自带自检对基线（冻结相机）** | **fnv=0x90E0745C2C7B8599 == 基线 ⇒ PASS（逐位未变）** |
| TestCSMIncrementalPass（自仓库根） | 契约 | All CSM Incremental Pass Contract Tests PASSED |

判据选择说明：`GPUDrivenCull` 默认模式物体在轨道运动（`GC_STATIC` 只冻结**相机**），
"逐图 FNV 唯一"对它不适用 ⇒ 改用其自带自检（冻结取景对基线），这才是该示例设计的回归口径，
且给出逐位一致的强结论。另注：各示例 `*_SELFCHECK` 早退 ⇒ 部分台架每图仅 1 帧样本。

**另发现（非本任务，供决策）**：`src/ecs/CMakeLists.txt` 给这些测试只设了
`VS_DEBUGGER_WORKING_DIRECTORY`（仅 VS 调试器生效），**未设 `ctest` 用的 `WORKING_DIRECTORY`**
⇒ 凡按相对路径读 `ShaderLibrary/...` 的测试在 `ctest` 下必红（实测 CSM 契约与
TransformFlatStorage 从仓库根跑均 RC=0，经 ctest 跑则 FAIL）。

### §11.11 负结果：每图一帧全空（非阴影示例同样命中）—— 环假说不成立 —— **已由 §11.15 收口**

**症状（用户报）**：不只 ATS/Cascade，**所有走 GPU 剔除的示例**每 3 帧有 1 帧整幅空白。
用**横向判据**（冻结取景下三张图的 FNV 必须彼此相等）首次照出——此前判据都是"单帧对单值"，
结构上抓不到"三张图内容不同"。

**实测**：PBRSpheres / AlphaModes / CoverageVariants / LoadScene / PlaneGrid3D 的
图 0/图 2 均等于各自**已知基线**，图 1 全空（`distinct=2~4`）；且 `LoadScene` 与
`PlaneGrid3D` 的图 1 FNV **完全相同**（`0x16F0718FCC800383`）⇒ 纯清屏、与场景无关。
Cascade 图 1 是 `distinct=256` 的满图（另一种形态）。

**已排除（全部实测）**：
- 就绪门：坏帧上 `出口=ok 就绪=1` ⇒ 接管确已发生；
- 派发槽号 / 绘制计数偏移：逐图正确对应（0 / 2056 / 4112），用户 RenderDoc 佐证
  `countBufferOffset` 正确、`maxDrawCount=100` 非零 ⇒ 布局与寻址正确；
- 标签表：写标签的槽 == 派发读标签的槽（逐帧一致），且标签值合法（`0x00000000`，非 `0xFFFFFFFF`）；
- 实体行表：`Context.cpp:1503` 明确写进**每一份**；
- world addresses：`:1461` 初始化时**为每个槽都写**；
- 可见性：CPU↔GPU 对拍 `match`、`不一致=0` ⇒ 着色器认得出实体。

**假说 A（L2W 环被渲染目标切换推进）—— 已实施并实测否定**：
`Context::SetFrameIndex` 会被主帧/离屏各调一次（`SetFrameIndex(4)`），
而环的帧号决定动态行基址（`RingLayout::GetBaseIndex = static + frame × dynamic`）。
把环改为"只跟主帧序号推进"后重建（改动确已进二进制：exe 时间戳晚于源码、0 错误）——
**六台架的图 1 FNV 与修复前逐字节相同** ⇒ 环与此症状无关，改动已回退。

**下一个该查的方向**（供接手）：那一帧"只清屏"说明**绘制没画任何东西**，
而可见性/标签/布局/寻址均已验证正确 ⇒ 矛盾点收敛到
**"cull 写入的槽"与"draw 读取的槽"在同一帧内的时序**：
cull 在 `RenderFrameSync`（帧末）跑并**同时清理本槽计数**，而 draw 发生在其之前 ——
即 draw 读的是"上一次轮到该槽"的计数。当某槽的写入与清理**跨帧错位**时，
draw 就会读到已清零的值。接手判据建议：在该槽的 cull 命令与 draw 之间插入
"计数回读 + 帧号"记录（我的 `printf` 探针法已验证可用）。

### §11.12 每图一帧全空：CPU 侧全部输入已逐字节排除，收敛到 GPU 执行侧 —— **已由 §11.15 收口**

**症状**：所有走 GPU 剔除的示例每 3 帧有 1 帧纯清屏（`LoadScene` 与 `PlaneGrid3D` 的空白帧
FNV 完全相同 = 纯清屏、与场景无关）。固定取景下三图 FNV 本应相等，实测图 0/2 相等且等于
各自已知基线、图 1 为空。

**已逐字节验证为正确（且各帧槽一致）**：
| 输入 | 证据 |
|---|---|
| 标签表内容 | RenderDoc：槽 1 那份（偏移 32768）前 **400 项为 `0`（= 批 0 有效）**，之后才是 `0xFF` |
| 布局表 BatchInfo | RenderDoc：偏移 0 与 8224 都是 `(0,100,0,192)`，正常帧/空白帧一致 |
| 计数/标签基址 | 探针：`count_base_bytes` = 0/2056/4112、`tag_base_bytes` = 0/32768/65536 逐槽正确 |
| 派发槽号 | 探针：`dispatch_slot` = 0/1/2 逐图正确（与图号一致） |
| 相机行 | 探针：三帧槽 `CommitRow` 全部成功（行号 0/1/2、失败 0 次），写入值三槽一致（`1.358 0 0 0 0 -2.414 0 0`） |
| 实体行表 | `Context.cpp:1503` 写进**每一份** |
| world addresses | `:1461` 初始化即为每槽写 |
| 接管是否发生 | 追踪：`出口=ok 就绪=1`；`ApplyGPUCullOverrideHook` 的拒绝路径探针**从未触发**（含 `items > capacity`） |
| 可见性对拍 | `match`、`不一致=0` |

**已否定的假说（均已实施并实测否定，不留改动）**：
1. L2W 环被渲染目标切换推进（§11.11）；
2. 标签表缺 pass 维度（该修复保留，但它是**另一个**真实缺陷，与本症状无关）；
3. `batch->items > info.capacity`（400 > 100）导致接管被拒 —— 拒绝探针未触发；
4. 相机行未激活/未提交/值为零 —— 三者均已实测为正常；
5. 布局/标签/基址的槽寻址 —— 逐槽正确。

**收敛结论**：**CPU 侧与所有按槽分份的资源都是正确的**，而 GPU 在其中一个槽上产出 0
（槽 0/2 的计数为 100，槽 1 为 0）⇒ 差异只能发生在 **GPU 执行侧**：该槽对应的那次派发
**要么没执行、要么其写入落在别处**。

**下一步唯一有效手段（GPU 侧可观测）**：在剔除着色器出口把每个线程的判据
（`visible` / `cull_ok` / 读到的 `batch` / `binfo.capacity` / `pc.camera_row`）写入一张
**诊断 SSBO**，帧末回读打印，与 CPU 参考逐项对照。GLSL 无法打印，故必须走"写 SSBO + 回读"。
这是本轮之后唯一还未使用的观测面。

### §11.13 两个实验：计数无关、命令步长 12 正确；图像 1 读到的是"恒零"而非"错位" —— **已由 §11.15 收口（槽 1 → 槽 0 阴影半区 = 从未写过的全零）**

**实验 A（计数无关）**：`FORCE_COUNT=7` 时在**每帧绘制前**把 8 个帧槽的批计数直写为 7
（打印确认生效 31 次）——**画面逐字节不变**（图 1 仍为空）。⇒ **绘制内容不依赖该计数**，
计数写入/清零时序不是本症状的原因。

**实验 B（命令步长）**：怀疑"着色器 `buffer_reference_align=16` ⇒ 数组步长 16，而绘制按
`first × 12` 读"⇒ 把绘制侧步长改为 16 并放大 ICB 容量后：
- 图 0/图 2 的 FNV **与已知基线都不同**，且同一图在 24 帧内出现 **5 种**取值（内容逐帧乱变）；
- **图 1 仍恒为空（`distinct=4`，逐帧相同）**。

⇒ **步长 12 是正解**：`VKDevice.h:379` 明确"`buffer_reference_align=16` 的承诺必须由**基址**
兑现"——该对齐约束**基址**，不约束数组元素步长；强改 16 会让绘制读到错位数据（逐帧乱变），
**反证**原步长正确。改动已回退。

**由实验 B 得到的新事实**：**错位时内容会逐帧乱变**（图 0/2 即如此），而**图 1 恒定为空**
⇒ 图 1 的绘制读到的不是"错位垃圾"，而是**恒定的零** ⇒ **那一批的命令行在绘制时刻从未被写过**
⇒ 与最初硬数据（该槽 `c0=0`、剔除产出 0）**回到同一点**：该帧槽的剔除/收集**没有产出**。

**下一步收敛点**：既然所有按槽分份的 CPU 侧输入都已逐字节验证正确、且与计数/步长无关，
就只剩"该帧槽的派发是否真的执行/其写入是否真的落盘"这一层 ⇒ 需 GPU 侧可观测
（剔除着色器出口写诊断 SSBO 后回读）。

### §11.14 逐项实测收尾：CPU 侧每一项都正确，剩一条自相矛盾留作接手第一站（**已由 §11.15 一击命中结案，勿再重走**）

本轮把"所有按帧槽分份的输入"**逐项实测**（不再靠代码推断）：

| 输入 | 实测值 |
|---|---|
| 标签表（该槽） | 前 100 项 = 批 0（有效），之后 `0xFF` |
| 布局表 BatchInfo | 偏移 0 与 8224 均为 `(0,100,0,192)` |
| 计数/标签基址 | 逐槽 `0/2056/4112`、`0/32768/65536` |
| 相机行 | 行号 0/1/2；内容三槽逐位相同（`1.358 0 0 0 0 -2.414 0 0`） |
| **实体行表（8 份）** | **每份 Active/Visible 计数均 `101/101`** |
| **world addresses 基址** | **逐帧 `pc == expect`**（`13021216768/896/1024` 随槽变） |
| 派发规模 | `entity_count=4096`、`batch_count=1` 逐帧恒定 |
| GPU 自有幸存数 | 槽 0/2 = 100；**槽 1 = 0** |

⇒ **CPU 侧再无未测项**，而 GPU 在一个槽上产出 0。

**留作接手第一站的自相矛盾**：`draw_count` 是**全帧单份 1 个 u32**（非按帧槽），
由剔除在**帧末**写入；回读发生在**帧首**，因此 `image=1` 帧读到的应是 `image=0` 帧（槽 0）
的剔除结果 = 100 —— **实测却是 0**。⇒ 该量在"写入时刻 ↔ 读取时刻"之间存在尚未建模的
按槽/跨帧关系。下一步只需在该缓冲上同时记录**写入时的帧号/槽号**与**读回时的帧号/槽号即定位**。 ⇒ **该「下一步」已作废：§11.15 用 `channels.culldump` 在派发后全量回读即一击定位（帧首回读永远看不到本帧派发写了什么）。**

**方法论收获（已生效）**：① 判据必须**横向**（冻结取景下多张交换链图的内容应相等），
"单帧对单值"的基线结构上抓不到本类缺陷；② 每轮改动**构建失败即不采信运行结果**；
③ 只读探针一律走"`printf` + 纯插入/整行区间替换"，锚点用原文而不改写。

### §11.15 第三例结案：4-ID 表下发地址漏 pass 分半的槽步长（特殊 dump 版一击命中）

**根因**（T4c3a 漏改点，与 §11.10 坑 1 同一条坑的第二实例）：
T4c3a 把实体 4-ID 表按 pass 分半（每帧槽 `kRowStride = 2×kCapacity` = 8192 行），
分配与上传都改了 `kRowStride`，**唯独 `SyncWorldAddresses` 下发 `addr_entity_render_ids`
仍按 `kCapacity` 步进** ⇒ 槽 s 指向半份错位地址：

- 槽 1 → 槽 0 的**阴影半区**：无阴影示例 = 从未写过的全零 ⇒ 着色器 `geometry_id==0`
  在视锥测试**前** early-return ⇒ `DrawCount=0` ⇒ 整帧纯清屏；
  有阴影示例（Cascade）= 阴影风味 ⇒ 满图但内容不同（§11.11"另一种形态"至此解释）；
- 槽 2 → 下一份 pass-0 半区：上传把每份写成相同内容 ⇒ 恰好正确 ⇒ 正常。

⇒ 解释了全部观测：只坏一张图、CPU 侧每样实测正确、标签/布局无罪、对拍看不见（CPU 参考不走设备地址）。

**定位手段 = 特殊 dump 版（`channels.culldump = true`，转正为常驻诊断件）**：每次 compute 派发后
`WaitLastSubmitFence()` 再全量回读所有帧槽的计数/布局/标签/命令 + 绘制端 `COUNT_DUMP` +
`[WADUMP]` 每帧下发基址。**帧首读回永远看不到本帧派发写了什么**——观测必须锚在派发后。

**修复**：`Context.cpp` 三处同口径 `kCapacity`→`kRowStride`（下发/清零/日志）。

**验证**：PBRSpheres 三图全等 `0x3DD88A0999CDC486`（空白图消失、好图逐字节未变）；
七台架每图唯一+横向相等+回基线（Cascade 多帧三图全等旧基线 `0x1CDAADEBF18B7CE4`）；
ATS 契约 D1/D3/D4 全 PASS；CSM 契约 42 项 PASSED；门 45/0。

**收尾教训**：初验时 Cascade 一度 `0x1BAB…`，真凶是排查期遗留的**无门控热路径同步回读**
（每批每帧 `ReadbackBuffer` = GPU stall，改变帧间节奏）——收进门控后立刻回基线。
**未门控的观测本身会改变被观测系统**；清理调试件属于判据环节，不是卫生环节。

### §11.16 pass 身份推导判据修正：「非交换链」⇒「异于本世界主目标」（2026-10-07）

`46e2da935` 的推导 `is_current_pass_shadow = req.is_shadow_pass || 非交换链目标` 把
**独立离屏世界**（OffscreenWorld：自有批表/自有 GPUCullSystem/`RenderTo(自有RT)`）误判成
shadow pass ⇒ collect 走阴影精简链只收 ShadowCaster ⇒ 子世界无 caster ⇒ 收集为空 ⇒
RTT/RTT-ColorDepth 离屏 cmd 只剩 Begin/EndRendering（RenderDoc 实证零绘制）。
ShadowMap 离屏本就只该收 caster ⇒ 它正常，恰好掩盖误判。

**正解**（`Context.cpp::RenderTo`）：判据按「**是否与主帧共享批对象**」建模 =
`req.target != render_target`（目标异于**本世界主目标**才判 shadow）；
`== render_target`（含独立离屏世界对自有 RT）走完整 forward。第一例兜底与
`req.is_shadow_pass` 显式声明两个正交条件都保留。

**判据（实测）**：RTT 两例离屏每帧 `pass=0 几何批=1` + flush 逐帧 + 0 ERROR；阴影三例/ATS/
AsyncTextureUpload/门/CSM 全对基线逐字节不变。

**教训**：兜底推导要按**资源争用拓扑**建模，不按"目标外观（离屏/交换链）"建模。

### §11.17 离屏单帧世界（RenderOnce）永不绘制 —— 五层"晚一帧"缺陷族结案（2026-10-07）

专项档案（症状/取证时间线/验收判据全量，原 `future/offscreen-onetime-world-stall-analysis.md`）已于 2026-10-18 删档，存于 git 历史；本节保留结论。
拓扑独特性：**独立子世界（OffscreenWorld 自有批表 + 自有 GPUCullSystem）× 只渲染一帧（RenderOnce）**。
每层缺陷本质都是"晚一帧"，每帧驱动的世界第 2 帧自愈 ⇒ 全部潜伏；单帧世界逐层暴露。

| 层 | 根因 | 修复 | 状态 |
|---|---|---|---|
| ① pass 身份 | 兜底推导「非交换链 ⇒ shadow」误判子世界 ⇒ collect 只收 caster ⇒ 收集为空 | 判据改「异于本世界主目标」（比 `saved_target` 快照）| `71cc6390c`（§11.16）|
| ② 初始化时机 | guard（RenderBatch）早于 GPUCullSystem 延迟初始化（RenderFrameSync）⇒ 首帧 `BeginBatchTagWrite` 拿空缓冲静默 return | guard 入口 `EnsureInitialized()` + null 出口改响亮 | `2f9a8a437` |
| ③ 定帧护栏 | `IsDispatchSettled`（≥2 帧）/「槽首写不接管」= T3g"退 CPU 账"时代遗迹 ⇒ 单帧世界永久 undsettled | 两条护栏整体删除（`IsSlotLayoutWritten`/`slot_layout_written_` 零读者同删；`IsDispatchSettled` 本体保留给 CPUCullReferenceSystem 采样门）| `2f9a8a437` |
| ④ 脏行上传时机 | 实体行/4-ID 表脏行上传挂帧首 `SetFrameIndex` ⇒ 本帧 collect 标的行传不到本帧派发 | `PrepareRenderPassSetup` 在 RenderCollect 后补一次 `SyncWorldAddresses()` | `2f9a8a437` |
| ⑤ **变换提交时机（真·最后一步）** | `SubmitTransformUpdates`（= L2W 矩阵行与实体行 `row[Transform]` 的**唯一落盘点**）只挂在 `RecordPreparedRenderPhaseRange`（BeginRenderPass **之后**的绘制录制段）⇒ 同帧 RenderFrameSync 的剔除派发恒读**上一帧**的写入；单帧世界首帧 `row[Transform]=0` ⇒ cull 着色器视锥测试前 return ⇒ `DrawCount=0` | `PrepareRenderPassSetup` 在 `SetFrameIndex` 后、collect 前补一次 `SubmitTransformUpdates`（同帧幂等 ⇒ 绘制段第二次调用退化 no-op）| 本批 |

**⑤ 的取证捷径（关键教训）**：`channels.culldump` 的 `[EntityDump]` 段读的是 **CPU 权威表**
（`GPUCullSystem.cpp` `context_->GetEntityGPUTable().Row(e)`），不是 GPU 回读 ⇒
`transform_id=0` 直接证明**派发时刻 CPU 侧就没写过**（上传路径无罪，专项档 §7 P1 的方向 1/2
都不成立——不是 Mobility 登记也不是 `GetDirtyCount()==0` 短路，是**写者相位晚于消费者相位**）。
诊断件输出的是哪一侧的数据，必须先确认再当证据。

**附带收益**：修复后主世界 frame=0 即 `DrawCount=1`（此前恒 0/0 第 2 帧才自愈）——
A6-d 删 CPU 账后每个应用的**首帧**都静默空白一帧，同批修掉。

**验收（实测）**：RTT/RTTCD 离屏 `transform_id=1`（静态 1+slot）+ `DrawCount=1` +
`COUNT_IN_BUFFER=1` + flush engaged；八台架逐字节对基线；ATS/TestCSM/ATU(05149=0)/门 45/0 全绿。

**契约固化（Test 42 = M8/M9，破坏验证过）**：M8 钉 Context.cpp 的提交点（needle 带注释锚）；
M9 钉 guard 入口 `EnsureInitialized` + 禁 `gpu_cull->IsDispatchSettled()`/`IsSlotLayoutWritten`
在接管路径复活。

**新不变式测试床**：「单帧渲染世界」正式成为回归床——RTT 两例已装帧摘要自检
（`RTT_SELFCHECK=1` → `0xBA9CA214778AF3A4`、`RTTCD_SELFCHECK=1` → `0xD71ABABC2BE460C0`，
第 8 帧、冻结自旋、distinct≥50 不空转守卫；三次连跑一致后回填）。
任何"晚一帧"类回潮在 RTT 上直接 FAIL，不再依赖每帧世界的自愈掩盖。

**遗留核查（2026-10-07 已定案，勿再立项查"恒 4"）**：离屏 RT frame index 恒 4 = **设计行为**——
`RenderTargetDesc.slot_count` 默认 1（全仓无人传 >1）⇒ `slot_index=(x+1)%1` 恒 0 ⇒
`GetCurrentFrameIndex()=data_slot_base+0`，首个离屏 RT 带基址 = `HGL_FRAME_SLOT_MAIN`(4)。
**"恒槽跨帧覆写"风险不成立**（双重串行化）：① 深度 1 带自保护（`BeginRender` 复用槽前
`WaitLastSubmitFence`，VKRenderTargetData.cpp:51）；② `RenderTo` 下沿 `rt->WaitFence()`
⇒ 每次离屏提交返回前已 CPU 落地。代价只是离屏无 CPU-GPU 重叠（本就被②串行化）。
⭐ **真缺陷（顺带定位）= 槽带只借不还**：`next_offscreen_slot_base` 只增
（RenderTargetManager.cpp:255），`Destroy/Release` 不归还，离屏带共 4 个（[4,8)）——
**CSM 一条级联占一带**（EnvironmentSystem.cpp:190-197）⇒ 一次 Enable 即耗尽 ⇒
这就是 C3 遗留缺陷①「二次 `EnableMainLightShadow` ⇒ 越界(起点=8)」的**根因**；
OffscreenWorld+CSM 同应用 = 第 5 个离屏 RT 必 fail-fast（合理组合，非极端场景）。
`Resize` 不泄漏（`RebuildOffscreenRT` 复用同一 rtd 只重建纹理/FBO）。
**已修（2026-10-07，走 A，`d9b2c6886`）**：新增 `OffscreenSlotBands`（占用位图 first-fit
+ 显式归还，纯算术可单测，`inc/hgl/graph/module/OffscreenSlotBands.h`）；RTM 游标删除、
registry 登记 slot_base/slot_count、`Destroy` 归还（创建失败路径 BandGuard 收口）、`Release`
后 Reset。EnvironmentSystem 零改动（handle reset ⇒ Deleter ⇒ Destroy ⇒ 归还天然接通）。
判据：Test 43（生命周期账 a-f 含 C3① 场景重放 + §9 M10 接线契约/游标禁复活）；端到端探针
二次 `EnableMainLightShadow` first=1 second=1、0 VUID（**订正：该探针在 Init 期、队列无在途工作 ⇒ 不足以销 C3②，勿据此判定**）；
八台架/ATS/ATU/门 45/0/TestCSM 43 用例全绿。

**C3② 另行结案（2026-10-07，`5a085a627`）**：世界存活时 `DisableMainLightShadow` ⇒ `vkDestroySemaphore(): VkSemaphore[CSM_Cascade_0:Lane] that is currently in use by VkQueue`（VUID-vkDestroySemaphore-semaphore-05149）。
根因：lane 不只被本 RT 的提交 signal，还被**主帧提交 await**（`Context.cpp` 的 `AddFrameLaneWait(rt->GetLane(), rt->GetLaneValue())`）⇒ 那个队列不属于 `RenderTargetData`、结构里枚举不到 ⇒ 只等自己的 queue fence 不够。
修复：`RenderTargetData` 增加 device 句柄（`CreateOffscreenRT` 注入），`Clear()` 在销毁 fbo/cmd_bufs/queues/lane **之前** `device->WaitIdle()`（空守卫覆盖创建失败路径；RT 销毁是低频路径 ⇒ 全局静默代价可接受）。
判据：「运行中 Disable」探针（第 120 帧、世界已渲染 119 帧）修前 in-use 命中 **2**、修后 **0**（验证层 1.4.363 确认在加载）；Test 43 增 §9 **M11** needle（连守卫一起钉 `if(device)`，两种破坏模式均点名 rc=43，逐字节还原复绿）。探针验后已撤。
