# ULRE 实例化渲染与合批设计（技术文档）

> **版次**：v3（2026-10-08，补档）。**用途 = 研究/评审底稿**：把一个具体问题（"GPU 接管后 64 个 cube 变成 64 条命令"）
> 背后的机制、方案空间、取舍与被否理由一次讲全，可离线通读。
>
> **v2 修订说明（2026-10-08 拍板）**：
> 1. **技术栈边界明确**：引擎**最近两年内不启用 Meshlet 技术**，无需考虑 Meshlet 切分、Cluster 剔除或 Task Shader（放大着色器）级联空跑。
> 2. **S2 显式实例组方案优化**：免除原“预留空槽 + Mesh Shader 内 flag 早退（emit 0 顶点）”方案；由于 S2 数据本来每帧重写，更新端直接**紧凑连续写入（Compact Write）**活跃实例行，并将间接命令的 `groupCountY` 直接写入实际活跃数 $N_{active}$，达成 100% 满载派发，彻底消除空跑 Workgroup。
> 3. **静态批与动态批切片解耦**：静态批命令条数在烘焙时固定为桶数，每帧仅做桶级剔除写 `Y=N` 或 `Y=0`，不与动态项竞争 `counters[batch]` 的 `atomicAdd`。
> 4. **S1 空间分块落地规格**：明确采用松散规则网格（Loose Uniform Grid），AABB 中心点入桶。
> 5. **Q1/Q2 全部闭环定案**（见 §7、§8）。
>
> **v3 补档（2026-10-08，一致性订正 + 语义补记；与执行方案 v4 同批）**：
> 1. **一致性**：§6 的 T4 行按 D8 改为「紧凑排布（Compact Write）+ 动态设 `Y=N_active` + 密集数组增删」；
>    §9.3 配套版本号对齐（执行方案 = v4）；术语表把「稳定槽位」拆成「**冻结布局**」与「**成员槽位**」两项，
>    §4.3/§5.1 用词同批改正。
> 2. **语义补记（命令表的帧槽）**：S1 每帧改命令 `Y`（0/N）落在**单份竞技场**上 —— 与既有欠账
>    （`entity-rows-gpu-cull-plan.md` §8.10 §十一：竞技场只按 pass 分扇区、不按帧槽）**同源、不新增风险类别**；
>    根治手段（命令表按帧槽分份）体量很小（`桶数 × 8 × 12B`）。见 §7 R5。
> 3. **语义补记（S2 成员行号的时效）**：D8 的 swap-and-pop 只保证「**切片集合不变**」，成员↔槽位映射每帧可变
>    ⇒ 契约明确：**成员行号只在「本帧更新之后、下一帧更新之前」有效**，外部不得缓存；要跨帧稳定只能 tombstone
>    （= Q1 已否的 flag 形态，属取舍）。见 §5.1。
> 4. **关系澄清**：S1 与 CSM 的静态 caster 缓存**各管一段**（见 §4.5 新增行）。
>
> **配套**：执行方案（怎么做/怎么验）= `doc/future/e2-gather-run-length-merge-plan.md`（v3）；
> 现象原始记录 = `doc/future/entity-rows-gpu-cull-plan.md` §8.10 §八（含用户 RenderDoc 截帧证据）；
> 机制层规格 = 同档 §9；任务登记 = `doc/backlog.md` E 线 E2。
>
> **本文所有事实都带 path:symbol**；数字都来自实测或有出处的记录。
> ⚠ 行号属 **2026-10-07 的 HEAD**（仅供定位，会随改动失效 ⇒ **引用请以符号/文件为准**）。

---

## 0. 摘要

- **现象**：形态 B（GPU 接管）下，`GPUDrivenCull` 的 64 个同几何 cube 被画成 **64 条 indirect 命令**
  （每条 `Y=1`）；而引擎自己的约定是 `Y = 实例数`。像素结果与"一条命令 Y=64"完全一致 ⇒
  **现有像素/对账判据对这件事永远无感**，差的是命令数与每条命令的固定启动开销。
- **定性**：这不是机制缺失（ICB 的 Y 维度、mesh 侧实例号、行表都已就位），而是
  **"没有人产出 Y>1"**：CPU 侧的实例合批记账（`DrawItemRange::instance_count`）自 CPU 绘制路径整删后
  已无消费者；GPU 路径写死 `Y=1`。
- **结论（2026-10-07 / 10-08 拍板）**：**不做"每帧动态合并"**（它必须在运行期解决"合并命令的实例行恰好连续"这条
  硬约束，而运行期行号来自无序的 `atomicAdd` ⇒ 要么加扫描/前缀和，要么被实体块边界切碎；
  而绝大多数几何本来就不动）。改为**「改变时烘焙布局」**：
  **S1 静态实例烘焙**（静态集合变更时一次按 `键 × 空间块` 分桶、排行、写命令表；每帧只做**桶级剔除**，
  不可见桶 `Y=0` 即不画）+ **S2 显式实例组**（开发者显式指定，成员槽位紧凑连续，更新端每帧只写活跃数据、动态设 `Y=N_{active}`，不重排布局，0 线程空跑；
  用于"静态模型挂在会动父体上"、粒子、Foliage）；**静态批与动态批切片解耦**；**动态项完全不合**。

---

## 1. 问题

### 1.1 实测口径

| 口径 | 命令数 | 说明 |
|---|---|---|
| CPU 账（历史基线） | **1 条（Y=64）** | 用户 RenderDoc 截帧 + flush 日志 `first=0 count=1（gpu_driven=0）` |
| 形态 B 接管（现状） | **64 条（Y=1）** | gather 写死 `(x,1,1)`，RenderDoc 截帧可见"一条条画" |

- **像素完全一致** ⇒ 判据必须换维度（见 §5.3）。
- ⚠ **"CPU 1 条"已不能当活对照臂**：CPU 绘制路径已整体删除（T4c4；未接管 = fail-fast 不画）。
  现在 `DrawItemRange::instance_count`（`inc/hgl/ecs/support/DrawItemCompaction.h`）**没有任何非测试读者**
  —— 实例合批的记账还在，产出它的人已经不在了。

### 1.2 与引擎自己的约定不一致（本问题的准确定性）

- `ShaderLibrary/ecs/entity_gpu_cull.glsl:133`：「引擎约定：`groupCountX` = 组数（元素数/组大小），
  **`groupCountY` = 实例数**，`groupCountZ` = 1」；同样的约定写在
  `inc/hgl/vk/buffer/IndirectCommandBuffer.h:56`（`X=组数…，Y=实例数（实例轴），Z 恒 1`）。
- 全仓 `groupCountY` 的写者只有 gather 一处（写 `1u`），读者只有 dump 诊断
  （`GPUCullSystem.cpp` 内打印）⇒ **"Y = 实例数"这条约定目前无人兑现**。

---

## 2. 机制解剖（决定一切的五条硬事实）

### 2.1 命令形状与 Y 维度

```glsl
// ShaderLibrary/ecs/entity_gpu_cull.glsl
struct DrawMeshTasksIndirectCommand { uint groupCountX; uint groupCountY; uint groupCountZ; };
DrawIndirectCommandsRef(pc.indirect_commands_addr).commands[row] = DrawMeshTasksIndirectCommand(group_count_x, 1u, 1u);
```

- `group_count_x` = `(element_count + group_size - 1) / group_size`，其中
  `element_count = is_indexed ? index_count : vertex_count`（线：`>>= 1`，元素数 = 段数），
  `group_size` 来自批行（`MeshDispatchGroupSize(is_lines)` 单一真源：
  `inc/hgl/ecs/support/PipelineMaterialRenderer.h:48`，CPU 经 `SetBatchLayout` 下发，GLSL 不硬编码）。
- **Y = 实例数**（引擎约定，见 §1.2）⇒ 把 Y 设成 N、`first_instance` 设成桶首行，就是"一条命令画 N 个实例"。

### 2.2 实例号 = 行号（**本问题的硬约束**）

```cpp
// src/ShaderGen/meshgen/MeshShaderHeaderGen.h:123
#define gl_InstanceIndex (draw_params.first_instance + gl_WorkGroupID.y)
// src/ShaderGen/meshgen/MeshShaderVaryingGen.h:115 / :148
fragDataIndexID[vid / 3u] = gl_InstanceIndex;
const uint data_id = gl_InstanceIndex;
```

- `draw_params` = 每条命令的 8B 参数行：`struct MeshDrawCommand { uint geometry_id; uint first_instance; }`
  （`inc/hgl/graph/ShaderBufferSources.h`；GLSL 镜像 `BatchCmdRow`，消费点 `cmds[gl_DrawID]`
  —— `entity_gpu_cull.glsl` 的注释原文："消费点 `cmds[gl_DrawID]`，`first_instance` 进 `gl_InstanceIndex` 链"）。
- `data_id` 直接作为**行号**去查 l2w / 材质 / 4-ID 行（`ShaderLibrary/common/RenderItemResolve.glsl` 的正通模式）。
- ⇒ **一条 Y=N 的命令，它的 N 个实例行号必须恰好连续**：`first_instance + 0 … first_instance + N−1`。
  **这是"合批"能否成立的第一性约束**：行不连续 ⇒ 实例读到**别人的**行 ⇒ 错画（而且同几何同材质时像素可能还对，判据也抓不住）。

### 2.3 行槽分配与"计数器的双角色"

```glsl
const uint slot = atomicAdd(counters.counters[block_base + batch], 1u);
if (slot >= binfo.capacity) { atomicAdd(counters.counters[block_base + pc.batch_count], 1u); continue; }  // 溢出槽
const uint row = binfo.row_base + slot;
```

- 行槽由**每批一个 `atomicAdd`** 分配：**数量确定、顺序不确定** ⇒ 同一批的相邻同键项可能拿到 `slot=5` 与 `slot=9`。
- 绘制侧把这个计数值**当作命令条数**使用：
  `src/ecs/support/PipelineMaterialRenderer.cpp`（`ProcIndirectRender`）
  ⇒ `vkCmdDrawMeshTasksIndirectCount(first = slice.row_base, count = 本批那一格, max = slice.capacity)`。
- ⚠ **双角色**：今天"1 命令/项"让"行槽计数"与"命令条数"恰好相等 ⇒ 没暴露。一旦合并，两者必须分离
  （行按项、命令按桶）——S1/S2 的做法是让**桶的行号不再走运行期分配**，该计数格退回纯"命令条数"语义。

### 2.4 批的构成

- 批 = **同管线**（`b->key.shader_program`），`capacity = b->items.size()`（`PrimitiveBatchPipeline.cpp`
  的批装配循环）⇒ **一个批内可以有多种几何** ⇒ "一批一命令"并不成立，段合并才有意义。
- 批级常量：`topology` / `group_size` 随 `SetBatchLayout(pass, index, row_base, capacity, topology, group_size)`
  下发（`inc/hgl/ecs/support/GPUCullSystem.h`）⇒ **逐项键实际退化为 `geometry_id`**
  （+ geometry 行的 `index_base/vertex_base/is_indexed`，严格起见应纳入键）。
- 容量与纪律（机制层 M4/M5）：`kMaxBatches = 256`、`kMaxVisibleEntities = 4096`、竞技场
  `kArenaRows = kArenaPasses(2) × kArenaRowsPerPass(4096)`；切片起点 4 行对齐；计数缓冲末位 = overflow 槽。

### 2.5 派发形状

```glsl
layout(local_size_x = 64) in;                 // entity_gpu_cull.glsl
uint entity_index = gl_GlobalInvocationID.x;  // 实体表轴的一维网格
```

- 一帧一次 compute；**一个 workgroup = 实体表里连续的 64 个实体号**（与批无关，实体号也不保证同批相邻）。
  ⇒ 若在运行期做"块内合并"，合并范围就被这 64 个实体号硬切（见 §3.1）。

### 2.6 数据流全景

```
场景实体（EntityGPU 行：flags/row[]；4-ID 行：geometry/material/texture）
   │
   ├─(CPU，每帧) 批装配 PrimitiveBatchPipeline
   │     批键 = 管线；产出：批标签表（按实体 × 按 pass）、批分区表(row_base, capacity, topology, group_size)、
   │                       每批计数缓冲（末位 = overflow 槽）
   │
   └─(GPU，每帧一次 compute) entity_gpu_cull.glsl（cull + gather 合体）
         ① 逐实体：活跃位/可见位、geometry_id（=0 跳过）、视锥剔除
                 ⇒ 幸存计数 atomicAdd + 写可见表
         ② 查**本 pass 自己那一块**批标签（两 pass 独立，T4c2）
         ③ slot = atomicAdd(本批计数)（越界 ⇒ 溢出槽）
         ④ 写四张行表，row = row_base + slot：
              · ICB 命令   = (X, Y=1, Z=1)        ← 【本问题要改的就是这里】
              · l2w_out    = transform_id
              · cmds_out   = { geometry_id, first_instance = slot }
              · mtl_out    = { 材质行, 纹理 id, 阴影 bias/receive }
   │
   ├─(CPU，绘制) PipelineMaterialRenderer::ProcIndirectRender
   │     vkCmdDrawMeshTasksIndirectCount(first = row_base, count = 本批计数, max = capacity)
   │
   └─(GPU，mesh shader) gl_DrawID → cmds[gl_DrawID] → (geometry_id, first_instance)
                        gl_InstanceIndex = first_instance + gl_WorkGroupID.y
                        data_id = gl_InstanceIndex → 读 l2w / cmds / mtl 行 → 顶点/材质
```

---

## 3. 方案空间（含被否的两条）

### 3.1 形态 A —— 块内合并（workgroup-local run-length encode）

- **做法**：仍是单 pass。workgroup 内 3 步：① 逐实体算可见/标签/键；② 块内（shared memory）算
  "段起始掩码 + 段内前缀和" ⇒ 段内序号；③ 每块每批一次 `atomicAdd(计数, 块内段数)` 取段基址，
  段首项写命令 `(X, 段长, 1)`、段内各项写 `first_instance = 段基址`，四表按 `row = row_base + 段基址 + 段内序号` 写。
- **能力边界**：段必须落在**同一个 64 实体块内**且**实体号连续**。512 个同键 cube（连续）⇒ 8 条（理想 1 条）；
  批内实体号交错（多批混合的常见形态）⇒ 段被切碎，≈ 不合并。
- **代价**：块内扫描 + 每块每批一次原子；无新 pass、无新全局缓冲。
- **否决理由**：收益被 64 边界与"实体号连续"两个前提吃掉；且它只解决"块内"，不解决"动态"的本质问题（见 §3.3）。

### 3.2 形态 B —— 全局扫描（多 pass）

| # | pass | dispatch | 输入 → 输出 |
|---|---|---|---|
| P1 | `cull_tag_key` | ⌈4096/64⌉ 组 × 64 | 可见/标签/键 ⇒ `cull_scratch[entity]`（**新 scratch**），不写输出表 |
| P2 | `scan_blocks` | 64 组 × 64 | 块内**按批掩码**的独占前缀和 ⇒ 块内 `rank` + `block_sum[block][batch]`（**新 scratch**） |
| P3 | `scan_blocksums` | 1 组（块数 ≤64 固定） | 每块每批基址 `block_base`（**新 scratch**）⇒ rank 全局有序 |
| P4 | `emit_rank_key` | ⌈4096/64⌉ 组 × 64 | `slot = base + 块内 rank`；键按 rank 写 `key_by_rank[batch][rank]`（**新 scratch**） |
| P5 | `runs` | ⌈项数/64⌉ 组 | `key_by_rank[r]` vs `[r−1]` ⇒ 段掩码 ⇒ 扫描 ⇒ `run_id`；段首写命令（**新 scratch**：段表） |
| P6 | `emit_rows` | ⌈4096/64⌉ 组 × 64 | 四张行表按 `row = row_base + 段基址 + 段内序号` 写 |

- **收益**：批内**所有**相邻同键项都合并（含跨块、含任意规模）。
- **代价**：实体轴被扫 **3 遍**（P1/P4/P6，今天 1 遍）+ 两次扫描 + 4 个新 scratch；
  **行槽分配从 `atomicAdd` 换成扫描** ⇒ 与机制层 M4（切片 4 行对齐 / 每批计数 / overflow 槽 /
  "`Σcount` == 项数"）**正面相撞**；新 scratch 若不按帧槽分份即引入新的跨帧覆写面。
  （理论上可压成"单 pass decoupled look-back 扫描"，但跨块通信的复杂度/死锁风险不值得。）
- **否决理由**：代价与"动态项占比"完全不成比例（见 §3.3），且与既有机制契约冲突面最大。

### 3.3 为什么"每帧动态合批"实质糟糕（三条论证）

1. **它必须在运行期解决 2.2 的硬约束**：合并要求"实例行连续"，而运行期行号来自无序 `atomicAdd`
   ⇒ 要么加扫描/前缀和（B：3 遍实体轴 + 4 个 scratch），要么把合并范围限制成块内（A：被 64 实体边界切）。
2. **它每帧都要付**：无论本帧数据有没有变，扫描/原子/键比较都要重来。而**绝大多数几何基本不动** ——
   动的是少数（角色、载具、粒子）。
3. **它把机制层判据一起拖进沟**：命令数 ≠ 项数后，"每批计数"的语义、`Σcount == 项数` 契约、
   对拍的"slot k ↔ 行 k"值链都要重写；而这些判据是 CSM/阴影/GPU 剔除多轮验证攒下来的资产。

> **一句话**：动态合批是"用运行期的贵操作，去优化一批本来就不动的数据"。正确做法是把**布局**这件
> 能一次算清的事情挪到**变更期**（§4）。

---

## 4. 采用方案：布局烘焙（build-time layout）

### 4.1 核心思想：把三个面分开，各自决定"什么时候算"

| 面 | 内容 | 动态合并（被否） | **烘焙（采用）** |
|---|---|---|---|
| ① 成员集合 | 谁属于哪个组 | 每帧重算 | **变更/注册时定** |
| ② 布局 | 组内行怎么排（行号/命令表） | 每帧重排（⇒ 扫描） | **变更/注册时定** |
| ③ 每帧数据 | L2W / 材质参数 / 可见性 | 每帧写 | 每帧写（**不重排**） |

- ②冻结后，**2.2 的连续性硬约束在构建期天然成立**（行是排好的），运行期只剩"写数据"与"桶级剔除"。
- 判据也随之变简单：**烘焙次数 == 变更次数**（单调计数器差分，可离线复核，不受 vsync/时序干扰）。

### 4.2 S1 —— 静态实例烘焙（主力）

- **判定源**：`TransformAccessor::IsStatic()`（`inc/hgl/ecs/support/TransformAccessor.h:126-127`；
  先例：CSM 条带路径用 `mobility_filter=Static` 筛静态 caster）—— 不新增"作者标记"机制。
- **触发**：静态集合**变更时**（作者改场景 / 批量导入完成 / 显式 `RebuildStaticBatches()`），**不是每帧**。
- **产物**：
  - `静态命令表`：每**桶**一条 `{X, Y = 桶内成员数, Z = 1}`；**桶 = 键 × 空间块（chunk）**；
  - `静态行池`：每桶行**构建期排好** ⇒ 满足 §2.2；
  - `静态 L2W 池`：**单份**（静态物体每帧 L2W 相同 ⇒ 不需要按帧槽分份；新增"静态池不轮转"语义）。
- **每帧代价**：**桶级剔除** —— 桶 AABB 对视锥，不可见桶把命令的 `Y` 置 0（`Y=0` 的 indirect 命令 = 不画，
  **无需压缩命令列表**）。由微型 GPU Compute Pass 执行（桶数量级几十~几百，单 Workgroup 即可完成视锥平面求交），不可见置 `Y=0`，并在进入渲染前插入 `SHADER_WRITE -> INDIRECT_COMMAND_READ` 屏障。
- **空间分块规则**：采用**松散规则网格（Loose Uniform Grid）**（如 32m/64m 步长），以实例 AABB 中心点入桶，天然防止跨边界物体缝隙与重复切分。
- **重烤防竞态**：`RebuildStaticBatches()` 发生时（场景重载/批量加载），必须等待上一帧 GPU 消费完成（Fence 同步或过渡缓冲），确保单份静态池在写时不发生跨帧读写冲突。

### 4.3 S2 —— 显式实例组（开发者指定）

**场景**（用户提出的 Ext）：FPS/RTS 里大量敌方士兵，头盔/手雷是**静态模型、位置在动**（挂在会动的父体上）。

- **与 S1 同机制，只差 ③**：**布局（切片集合）**在注册时冻结、**成员槽位每帧紧凑重排**，每帧只写活跃数据
  ⇒ **位置在动不影响合批**。
- **技术栈定调（2026-10-08）**：**引擎两年内不启用 Meshlet 技术**，无需考虑 Meshlet 切分、Cluster 剔除或 Task Shader 级联派发。
- **写端紧凑排列（Compact Write，淘汰原 Shader flag 早退方案）**：
  - 原方案让预留空槽并在 Mesh Shader 内通过 flag 早退（`emit 0 顶点`），这在实例较稀疏时会导致大量无效 Workgroup 派发空跑。
  - 既然 S2 成员的 L2W / 材质等数据**本来就是每帧由更新端写入**，更新端在写数据时直接将本帧**活跃成员紧凑连续写入**组切片的前 $N_{active}$ 个槽位；
  - 间接命令的 `groupCountY` 直接写入当前活跃数 $N_{active}$。
  - **收益**：
    1. 连续性硬约束（§2.2）天然满足（连续排在 `0 .. N_{active}-1`）；
    2. 硬件派发 100% 满载，**零空跑 Workgroup**，完全免除 Shader 内分支与早期退出开销；
    3. 成员增删：CPU 维护活跃列表（密集数组，删除采用 swap-and-pop），增删**完全不重建布局**。
- **剔除粒度**：组级 AABB 视锥剔除（整组出视锥置 `Y=0`）。
- **开发者接口**：引擎分配并回填句柄（`AddInstance(group, item) -> InstanceHandle`，销毁调 `RemoveInstance`）。
- **每帧数据池**：S2 行池**按帧槽分份**（每帧在写），与 S1 的单份静态池是两条独立池/生命周期。
- **粒子 / Foliage** = "S2 + 程序化 per-instance 数据源（粒子缓冲 / 密度图）" ⇒ 属 S2 的数据源扩展，
  本任务只建 S2 的机制。

### 4.4 动态项与其它

- **动态项（非 `IsStatic`、未加入任何显式组）：完全不合**（保持现状：每项一条命令）。
  理由：它数量少、变化快，动态合并的每帧成本收不回来（§3.3）。
- **一个组只能一种 geometry + 一种 pipeline**（否则 X 与着色器程序不同，合不成一条命令）⇒ 头盔、手雷各成一组。

### 4.5 与既有机制的接点

| 既有件 | 位置 | 烘焙方案的关系 |
|---|---|---|
| 静态判定 | `TransformAccessor::IsStatic()` / `mobility_filter` | **直接复用** |
| 批键（管线） | `PrimitiveBatchPipeline` | 组键 = 批键 + 几何（+ 空间块） |
| 批标签 / 分区 / 计数 | `GPUCullSystem::BeginBatchTagWrite / SetBatchLayout / MapCurrentBatchCounters` | **静态批与动态批切片解耦**：静态批命令条数固定为桶数，不走 `atomicAdd`，走固定 Count 或独立计数；动态批维持既有原子计数 |
| 行表（l2w/cmds/mtl/4-ID） | gather 写的四表 | 内容口径不变；S1 行池单份、S2 行池按帧槽分份 |
| 帧槽纪律 | `HGL_FRAME_SLOT_TOTAL` 等（机制层 M1） | S2 遵守；S1 破例（单份）并写进契约 |
| CSM 的静态 caster 缓存（`mobility_filter=Static` 条带路径） | `doc/csm-mechanism.md` / `doc/backlog.md` D5 | **各管一段**：S1 管**颜色 pass 的绘制命令**、CSM 管**阴影图内容**（静态缓存 + 条带滚动）⇒ **不共享静态集合**（两边各自按 `IsStatic` 划集合）；若要共享需另立项 |

---

## 5. 契约与判据

### 5.1 契约变更

| 现契约 | 变更 |
|---|---|
| 命令 = 每项一条、`Y=1`（§9 M3） | S1/S2：命令 = **每桶一条**，`Y = 桶内有效成员数（可 0）` |
| `Σcount == 批内项数`（§8.10 §五） | S1/S2 组：**命令条数 == 桶数**；**`ΣY == 组内有效成员数`**；动态项口径不变 |
| 行槽由 `atomicAdd` 运行期分配 | S1：**行号 = 组行基址 + 桶内序号**（构建期定，不变）；S2：**行号 = 组行基址 + 帧内紧凑序号**（每帧重排） |
| 行号时效（新增） | **成员行号只在「本帧更新之后、下一帧更新之前」有效**（S2 紧凑重排 ⇒ 槽位可换主）；外部不得缓存跨帧行号。要跨帧稳定只能 tombstone（= Q1 已否的 flag 形态，属取舍） |
| 所有按帧表按帧槽分份 | S1 静态池 = **单份**（新语义）；S2 池按帧槽分份 |

### 5.2 判据清单

1. **命令数**（draw 实际消费条数）：静态 64 cube ⇒ **1 条**；
2. **`ΣY` == 组内有效成员数**；
3. 现有全套不变：三方像素全等 / `不一致=0` / `overflow=0` / 0 VUID / 八台架帧摘要逐位；
4. **烘焙次数 == 变更次数**（计数器差分；离线可复核）；
5. **桶级剔除 vs 逐实例剔除**的可见集合对拍（桶不变更时恒等）；
6. **台架**：扩 `GPUDrivenCull`（`GC_*` 家族）：`GC_BAKE=1` 打印 `桶数 / 命令数 / ΣY / 烘焙次数`。

### 5.3 为什么判据必须换维度

- 像素/对账判据对"命令数"**结构性无感**（合得对不对不影响像素）。
- 历史对照臂（"CPU 1 条 Y=64"）**已随 CPU 绘制路径删除而失效**。
- 替代对照臂 = **引擎侧推算**：把每批切片内的 `cmds_out` 几何序列读回来，数"连续同键的段数"
  ⇒ 与命令数直接对比（今天应为 `命令数=64 / 段数=1`，把"浪费"变成一个活的数字）。

---

## 6. 实施步骤（执行细节见配套执行方案档）

| 步 | 内容 | 关键判据 |
|---|---|---|
| T1 | **判据前置**：读回 命令数 / `ΣY` / 项数 / 行序可合并段数（`GC_BAKE=1`） | 现状基线 `命令数=64 / 段数=1` |
| T2 | 静态集合 + 变更检测 + 静态 L2W 独立池（单份） | 只在其变更帧重建（计数器） |
| T3 | 烘焙：`键 × 空间块` 分桶 + 排行 + 写静态命令表 + 每帧桶级剔除 | 命令数 = 1（64 cube）；像素全等；剔除对拍 |
| T4 | S2 显式实例组（引擎分配 `InstanceHandle` + **写端紧凑排布 Compact Write** + 动态设 `Y=N_active`；密集数组 swap-and-pop 增删） | 增删不重建布局（切片集合不变）；`ΣY == N_active`；0 线程空跑 |
| T5 | 台架 + 破坏验证（剔除漏判 / 行基址错位 / 变更不重建） | 3 条破坏验证全有牙 |
| T6 | 文档销账 | 旧口径清理 |

回滚点：`pre-e2-bake`。

## 7. 风险与开放问题

| # | 风险/问题 | 说明 |
|---|---|---|
| R1 | 行连续性被破坏 | S1/S2 构建期排行 ⇒ 天然满足；仍需破坏验证兜底 |
| R2 | 键覆盖不全（漏 geometry 行的 `index_base/vertex_base/is_indexed`） | 键元组要写明；破坏验证 |
| R3 | 桶粒度过粗 ⇒ 剔除质量差 | 空间块尺寸可调 + 台架量化 |
| R4 | 变更检测漏 ⇒ 陈旧命令表 | 版本号 + 计数器判据 + 破坏验证 |
| R5 | 静态池/命令表单份 与"按帧槽分份"纪律冲突 | ① 重烤帧做 GPU 在途同步保护；② **每帧桶级剔除改命令 `Y` 也是单份写** ⇒ 与既有欠账（§8.10 §十一：竞技场不按帧槽）**同源、不新增类别**；根治 = 命令表按帧槽分份（体量 `桶数 × 8 × 12B`）；③ 契约与池注释写明 |
| R6 | S2 成员超容量 / 增删竞态 | 容量冻结 + fail-fast；密集数组增删只改索引映射，不动静态布局 |
| R7 | 竞技场"按 pass 分扇区、不按帧槽"的既有欠账 | 本方案不碰；若触及同批记录 |
| Q1 | S2 剔除与有效性形态（原早退 vs 紧凑排布） | **已拍板（2026-10-08）**：放弃 Shader 内 flag 早退；更新端每帧紧凑排布活跃项，设 `Y=N_{active}`，0 空跑 |
| Q2 | S2 的 `(组, 序号)`：开发者显式写，还是引擎分配句柄？ | **已拍板（2026-10-08）**：引擎统一分配管理句柄（`AddInstance -> InstanceHandle`） |

## 8. 决策记录（2026-10-07 / 2026-10-08）

| 决定 | 内容 | 理由 |
|---|---|---|
| D1 | 静态判定源 = 现成 `IsStatic()` | 机制已在，不造第二套"作者标记" |
| D2 | 桶粒度 = **键 + 空间块** | 桶级剔除需要分辨率 |
| D3 | **动态项完全不合** | 每帧动态合并实质糟糕（§3.3） |
| D4 | 静态 L2W = **独立池单份** | 静态每帧相同 ⇒ 省 8× 内存 |
| D5 | 形态 A / B = **已评估未选**（非证伪） | 若将来出现"每帧都在变且必须最小命令数"的场景再回看 |
| D6 | **S2 显式实例组** = 会动父体下静态模型的合批手法：冻结布局 + 每帧写数据 | 动的是数据不是布局 |
| D7 | **两年内不启用 Meshlet 技术** | 彻底剥离 Task Shader / Meshlet Cluster 依赖，简化实现复杂度 |
| D8 | **S2 改用写端紧凑排列（Compact Write）** | 替代原 Mesh Shader flag 早退；100% 满载派发，彻底消除空跑 Workgroup |
| D9 | **静态批与动态批切片解耦** | 静态批走固定桶数命令，不与动态项争用 `atomicAdd` 计数器 |
| D10 | **S1 采用松散规则网格（Loose Uniform Grid）** | 按 AABB 中心点映射空间块，避免跨边界缝隙与多重切分 |

## 9. 附录

### 9.1 术语表

| 词 | 含义 |
|---|---|
| ICB / 间接命令 | indirect command buffer：GPU 自己写出的绘制命令（本引擎 = mesh tasks 命令 `{X,Y,Z}`） |
| Y 维度 | 实例轴（本引擎约定 `Y = 实例数`） |
| 行 / 行池 | 每个实例一份数据行（l2w / 材质 / 纹理 / 4-ID），行号 = 实例号 |
| 槽位（slot） | 切片内序号；`行 = row_base + slot` |
| 切片（slice） | 一个批在竞技场里的行区间 `[row_base, row_base + capacity)` |
| 键 | 决定"能否共用一条命令"的字段元组（≥ `geometry_id`；+ 批级 `topology`/`group_size`） |
| 桶（bucket） | `键 × 空间块` 的实例集合 = 烘焙后的最小绘制单位（一条命令） |
| 烘焙（bake） | 在"变更时"一次性算好布局（分桶/排行/命令表），帧间复用 |
| 冻结布局 | 桶/组的**集合与切片**在构建/注册时定死（成员不增删即不变） |
| 成员槽位 | `行号 = 组基址 + 组内序号`；**S1 构建期定死（不变）**、**S2 每帧紧凑重排（仅帧内有效，外部不得缓存）** |
| 动态合并 | 每帧重新决定"谁和谁一批"（本方案**不做**） |

### 9.2 关键代码索引

| 关注点 | 位置 |
|---|---|
| 命令写法 / 期望容量 / 可见表 | `ShaderLibrary/ecs/entity_gpu_cull.glsl`（gather 尾部；`DrawMeshTasksIndirectCommand`；`BatchInfoRow`/`BatchCmdRow`/`MeshDrawParamsHeadData`） |
| 引擎命令约定（`Y=实例数`） | `ShaderLibrary/ecs/entity_gpu_cull.glsl:133`、`inc/hgl/vk/buffer/IndirectCommandBuffer.h:56` |
| 实例号 = 行号 | `src/ShaderGen/meshgen/MeshShaderHeaderGen.h:123`、`MeshShaderVaryingGen.h:115/:148`、`ShaderLibrary/common/RenderItemResolve.glsl` |
| 命令参数行（8B） | `inc/hgl/graph/ShaderBufferSources.h`（`MeshDrawCommand{geometry_id, first_instance}` + 偏移断言） |
| 批装配 / 批键 / 容量 | `src/ecs/support/PrimitiveBatchPipeline.cpp`（`SetBatchLayout` 调用点、`MeshDispatchGroupSize` 单源） |
| 批标签 / 分区 / 计数 / 溢出 / 回读 | `inc/hgl/ecs/support/GPUCullSystem.h`（`BeginBatchTagWrite`/`SetBatchLayout`/`GetBatchLayout`/`MapCurrentBatchCounters`/`SyncForParityReadback`/`kMaxBatches`/`kArenaRows`） |
| 绘制消费（`IndirectCount`） | `src/ecs/support/PipelineMaterialRenderer.cpp`（`ProcIndirectRender`） |
| 三方对拍基座 | `src/ecs/support/CPUCullReferenceSystem.cpp`（`SyncForParityReadback` + `MapCurrentBatchCounters` + 值链对拍） |
| 静态判定 | `inc/hgl/ecs/support/TransformAccessor.h:126-127`（`IsStatic` / `IsMovable`） |
| 历史（已删/失效） | `DrawItemRange::instance_count`（`DrawItemCompaction.h`，已无绘制消费者）；`groupCountY = instance_count`（§8.10 §八 的旧引用，代码已无） |

### 9.3 相关文档

- 执行方案（含 T1..T6、破坏验证、回滚点）：`doc/future/e2-gather-run-length-merge-plan.md`（v4）
- 现象与原始证据（RenderDoc 截帧、flush 日志、CPU vs 接管对照表）：`doc/future/entity-rows-gpu-cull-plan.md` §8.10 §八
- 机制层规格与契约条目（M1..M6）：同档 §9
- 静态/动态与阴影域的先例（`mobility_filter=Static` 条带路径）：`doc/csm-mechanism.md`、`doc/backlog.md` D5
- 任务登记：`doc/backlog.md` E 线 **E2**
