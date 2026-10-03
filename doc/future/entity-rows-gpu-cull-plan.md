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

> 重做 T3d-1 时这四件**同一批次**做完（新 GLSL + 模块计数 + 门用例 + 提取器），
> 判据 **门 43 PASS / 0 FAIL** 才提交 —— 避免中途留下红树。

