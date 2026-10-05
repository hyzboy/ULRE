# 实体行驱动 GPU 剔除（T3 消费者先行）—— 设计 + 拍板点 + 任务分解

> 版次：v1（2026-10-03）。**本稿只做设计与拍板，不含代码改动。**
> 起因：用户拍板「**先把消费者定下来并同批做**」——`EntityGPU` 派生表（设计稿 T3）不再单独建，
> 而是与它的第一个真消费者「**剔除 shader 读实体行决定画谁**」一起落地。
> 关联真源：`doc/future/entitygpu-and-ulrescene-design.md`（B8 设计稿：T1 已落结构体 / T3 派生表 /
> P-B8-2 落位 = 世界私有 / P-B8-6 容量 4096）、`doc/future/ULRE_FINAL_TARGET_v2_设计约束.md` §1/§3、
> `doc/world-addresses-and-camera-model-plan.md` §2（地址三档）、`doc/gpu-driven-4id-draw-item-plan-2026-09.md`。

---

## 0. 现状实测（本次核对，非估算）

### 0.1 `EntityGPU` 是死结构：全仓零读者零写者

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
  ④ `src/ecs/support/TestRenderItemDataStorage.cpp` 的**三份内嵌 GLSL 夹具** + 逐字段 `static_assert`
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
    uint64_t addr_sky;                  // 世界 profile 的 SkyInfo（单份 ⇒ 全帧槽同址）
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

### 8.3 下一步顺序（不要重做侦察：落点看 §7，坑看 §7.6/§8.2b–§8.2g；**T3d…T3h 全达成**）

1. ~~性能对比~~ **已完成（T3g，结论见 §8.2f"收益的正确表述"）**：
   `gpu_dispatch` 异步化后 ~150us/帧（旧实现 1.1ms 里大头是每帧 fence + 每帧
   CreateQueue/CreateCmd 泄漏，不是 GPU 剔除）；小场景（100 实体）GPU 路径仍贵于
   CPU 剔除（7us）——收益在规模无关性（派发成本不随实体数增长），盈亏平衡数千实体。
   ~~形态 A 的去留~~ **已裁决（T3g）**：整体退役；形态 B = 唯一 GPU 路径。
2. **放开接管范围的既有欠账**（形态 B 首版 guard 挡在外的，按欠账逐个补）：
   ① ~~ShadowProxy GPU 行~~ **已完成（T3h，见 §8.2g）**；
   ② **meshlet 几何**（X 组数 = GetMeshletCount 分支）—— 池行补 meshlet_count 字段
      或另源，与 CPU 同口径后才放开；
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
- ② meshlet（补 GPU 可读 meshlet_count 源）、④ 实例化（per-item instance_count 源）仍待办；
  DGC 按拍板**不处理**（开发机无支持；形态 B 不需要任何扩展，只依赖 mesh shader）。

---

## §8.7 未完成清单（2026-10-05 整理；**新会话照此接手**）

### A. 本线（GPU 剔除）欠账

| # | 内容 | 现状 / 落点 | 接手判据 |
|---|------|-------------|----------|
| **A1** | **② meshlet 几何**（X 组数走 CPU `GetMeshletCount()` 分支） | guard 仍把这类批挡在接管外（池行 `addr_meshlets != 0` ⇒ 该批不接管）。需给 GPU 一个**可读的 meshlet_count 源**（池行加字段或另表） | 与 CPU 同口径后放开；门 44/0；该批接管且像素三方全等 |
| ~~**A2**~~ | ~~**④ 实例化/多实例 item**~~ **won't-do（2026-10-05 关闭）** | **查实无真实使用者**：唯一活调用方是 A3 台架（它为「被拒」而存在）、`SetInstance4ID` 只有测试在调、collector 对多实例写「全部同 4-ID」、上游无「每实例不同」的生产者 ⇒ 就算把 Y 写成实例数也只是同一项原地叠 N 遍，判据立不起来 | 按零兼容**整体删除**：`InstancedPrimitiveRenderItem` + 8 个自由函数 + slot 实例三字段（**行宽 48→40 B**）+ guard 该条 + A3 的④类项（其「异 program 自成一批」半保留 ⇒ A3 现为 5/5 全量）。存储侧 `AllocateContiguous/ReleaseContiguous` 保留（低层存储原语，Test 6 覆盖）；**CPU 侧自动合批不受影响** |
| ~~**A3**~~ | ~~**部分接管路径从未被真实触发**~~ **已完成（`c96cac8cd`，AlphaModes `AM_PARTIAL=1`）** | 台架 = Opaque 行最后一格改"同 def + billboard 顶点节点"（⇒ 独立批）+ `AllocateContiguousInstances(…,1)`（⇒ `InstancedPrimitiveRenderItem`，④ 类项）。实测：reject 日志（④ 理由）+ `ENGAGED batches=4/5 items=27` + `Parity 28/28/0` + `Σcount=27 < global=28 overflow=0 值链=27/27`（台架判据本就 Σ≤global，未误报）+ 未接管批确实被 CPU 账画出（Opaque 行 7 块）+ 像素三方逐字节全等 + 默认关时仍 4/4。**2026-10-05 补记**：④ 类项已整体删除 ⇒ 本台架的 `AllocateContiguousInstances(…,1)` 半消失，实测变为 `ENGAGED batches=5/5 items=28 arena_rows=36`（全量接管；异 program 自成一批照常被正确处理）⇒ 本行原「4/5 部分接管」判据改由 A4 批数超限（`CV_STRESS=260` ⇒ 256/260）覆盖 | 已达成（判据已迁移） |
| ~~**A4**~~ | ~~**容量边界未验**~~ **已完成（`CoverageVariants` 加 `CV_STRESS=N` 台架）** | N 个「同 def(Lit) + 仅 `alpha_cutoff` 不同」的配方（阈值进 `#define HGL_ALPHA_CUTOFF <v>` ⇒ program/批各不同）。实测 `CV_STRESS=260 CV_DIST=60`：**告警 1 次**（`接管批数超 kMaxBatches(256)`）+ `ENGAGED batches=256/260 items=256 arena_rows=1024`（=256×4，4 行对齐）+ `Parity 260/260/0` + `batch slices 256 rows=256 Σcount=256 global=260 overflow=0 值链=256/256`（**Σcount < global 合法**、无假红）+ 像素（CPU 账 vs 部分接管）逐字节全等 ⇒ 未接管的 4 批仍由 CPU 账画出。**负控**（临时改码，验完即撤）已证两条硬判据非空转：竞技场容量临时改 256 ⇒ `批次累计项数超共享竞技场` 精确在 64 批×4 行=256 顶格触发；批容量少一行 ⇒ `[T3i] 切片溢出！overflow=N`。**新发现**：竞技场那条在**现容量下不可达**——`EntityGPUTable::kCapacity(4096) == GetGatherCapacity()(4096)` 且**每实体至多 1 个批 item**（多批床台实测 items == 实体数）⇒ Σitems ≤ 4096 ⇒ 对齐后恒 ≤ 容量。**拍板 ①**：该分支已删除，随之删除只为它存在的 `GPUCullSystem::GetGatherCapacity()`（零残留）；删后判据复验不变（`batches=256/260`、`Σcount=256<global=260`、门 44/0）。⚠ 残留风险：若将来 `EntityGPUTable::kCapacity` 被抬到超过竞技场行数，切片会越过 gather 缓冲（静默越界写）——挡这个要的是 `static_assert(kCapacity <= kMaxVisibleEntities)`（机会性手段，②被否） | 已达成 |
| ~~**A5**~~ | ~~**L2W 环深度 = 3 vs 帧槽空间 = 8**~~ **已闭（代码层已按不变式写死）** | `inc/hgl/common/RenderOptions.h:33`：`#define HGL_L2W_RING_FRAMES HGL_FRAME_SLOT_TOTAL` ⇒ **环深度 ≡ 帧槽空间**，不可能再出现"离屏槽与主帧槽别名"（该别名曾导致 prepass 覆写主帧矩阵、整帧只剩清屏色）。§8.4 的旧"待查"已过时 | 已闭 |
| **A6** | **CPU 渲染路径的去留** | 按 2026-10-04 拍板：等 ②③④ 覆盖真实场景后再删或按 RELEASE 宏屏蔽（取回校验常驻） | — |

### B. 本次会话（alpha 呈现链）带出的新欠账

| # | 内容 | 现状 | 建议 |
|---|------|------|------|
| **B1** | **混合排的绘制顺序自由度** | `alpha_blend` 真生效后混合排结果与**绘制顺序**相关（引擎无透明排序）；GPU 接管的行序 = atomicAdd 幸存序 ⇒ 与 CPU 账可不同 ⇒ **像素全等判据对混合排不保证**（实测：多次复跑中偶发一次差异，且只可能落在混合排；序无关的 opaque/alpha_test/dither 恒可用于硬判据） | 要硬判据：让接管路径的混合批沿用 CPU 序，或给混合批加固定排序键 |
| **B2** | **unlit / 其它材质源的 alpha 语义未统一** | 只有 Lit 源改成 `alpha = base_color.a × opacity_mask.r`；unlit/sky 的 alpha 仍来自各自源模块（`forward_flat` 的 `lighting.alpha`） | 若要同一"比例 × 图案"语义 ⇒ 逐个材质源改对应 alpha 求值 |
| ~~**B3**~~ | ~~`alpha_to_coverage`~~ **已按拍板整体删除（2026-10-05）** | 该特性**永不支持** ⇒ 清掉全部残留：`MaterialPipelineConfig.alpha_to_coverage`、TOML 键、`MaterialCoverageMode::AlphaToCoverage`、`BlendMode/PassType` 的 A2C 枚举项、Vulkan 侧（EDS3 的 `extendedDynamicState3AlphaToCoverageEnable` 启用 + `vkCmdSetAlphaToCoverageEnableEXT` proc/命令录制 + 动态状态表项 + 名字表项）、门与文档 | 全仓 grep 零残留（`alpha_to_coverage`/`alphaToCoverage`/`AlphaToCoverage`/`ALPHA_TO_COVERAGE`/`ForwardA2C`，`backup/` 与第三方目录除外） |
| **B4** | **dither 阈值是位置哈希（白噪声）** | `ShaderLibrary/common/alpha_compositor.glsl:HGLDitherThreshold` = `fract(sin(dot(pixel,...))*43758.5453)` | 想要**有序抖动点阵** ⇒ 换 Bayer 4×4（一处改动） |
| **B5** | **引擎 API 缺口**：`ChangeTexture2DArray(tex, const void*, …)` **只有声明没有实现**（LNK2019；raw 版 `ChangeTexture2D` 有实现） | 谁想用数组的 raw 上传就会链接失败（本次已踩） | 补实现，或删声明避免误用 |

### C. 文档/测试床现状（本次会话后）

- 测试床两个：`example/Material/CoverageVariants`（同 def 两覆盖变体 ⇒ 2 批）、
  `example/Material/AlphaModes`（4 行 alpha 方式 × 7 档比例 ⇒ 4 批）；
  env 门控 `CV_*` / `AM_*`（`*_CULL` / `*_FORM_B` / `*_FRAMES` / `*_DUMP` / 相机覆盖），默认全关。
- 判据口径：门 44 PASS / 0 FAIL；`ENGAGED batches=x/y`（正向）；台架 Parity + `Σcount` + `overflow` + 值链；
  像素三方（CPU 账 / 仅 dispatch / 接管）逐字节全等（**混合排见 B1**）。
- 本节若与 §8.3/§8.6 冲突，以本节（最新）为准。

---

## §8.8 A1（② meshlet 接管）侦察结论与任务切分（2026-10-05）

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
