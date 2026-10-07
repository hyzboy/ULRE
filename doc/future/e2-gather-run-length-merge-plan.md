# E2 执行方案：gather 侧运行长度合并（「一项一条 ICB 命令」→「一段一条」）

> **版次**：v1（2026-10-07）。范围 = 把 gather 写出的 indirect 命令从「每项一条（Y=1）」改成
> 「相邻同键合并成一条（Y=段长）」。**不含** 剔除算法/批装配/切片机制的改动（那三条是机制层，见
> `entity-rows-gpu-cull-plan.md` §9）。
>
> **来源**：`doc/backlog.md` E 线 **E2**（2026-10-05 用户留置）；详版设计 = `doc/future/entity-rows-gpu-cull-plan.md`
> §8.10 §八（现象与代价）与 §9（机制层规格）。本稿把"怎么做 + 怎么验"定成形。
>
> **状态**：待拍板形态（A 块内合并 / B 全局扫描）后开工；T1 是判据前置，与形态无关。

---

## 0. 现象与目标

| 口径 | CPU 账（历史基线） | 形态 B 接管（现状） |
|---|---|---|
| `GPUDrivenCull` 的 64 个同几何 cube | **1 条命令（Y=64）** | **64 条命令（Y=1）** |

- 像素**完全一致** ⇒ 现有像素/对账判据对此**永远无感**；差的是命令数（ICB 条目、per-draw 参数行、
  每条 mesh 命令的固定启动开销）。
- 成因 = ④ 只删了**组件多实例**那半边，而 CPU 侧合批（`DrawItemRange::instance_count`）保留着
  ⇒ 出现「CPU 合、GPU 不合」的不对称（§8.10 §八）。
- 目标：gather 内做**运行长度合并**——相邻幸存项键相同则 `groupCountY++` 并入同一命令，不同则起新命令
  ⇒ 命令数从「项数」降到「运行段数」。

## 1. 已核实的现状（逐条给 path:line）

1. **命令写法（要改的地方）**：`ShaderLibrary/ecs/entity_gpu_cull.glsl` 的 gather 尾部：
   `DrawIndirectCommandsRef(pc.indirect_commands_addr).commands[row] = DrawMeshTasksIndirectCommand(group_count_x, 1u, 1u);`
   —— `row = binfo.row_base + slot`，`slot = atomicAdd(counters.counters[block_base+batch], 1u)`。
2. **Y 维度本来就是实例维度**（机制已在，无需新机制）：
   `src/ShaderGen/meshgen/MeshShaderHeaderGen.h:123` ⇒ `#define gl_InstanceIndex (draw_params.first_instance + gl_WorkGroupID.y)`
   ⇒ 把 Y 设成段长、`first_instance` 设成**段首槽**，即得"一条命令画 N 个实例"。
3. **实例号直接当行号用**（**这是本任务的硬约束**）：
   `MeshShaderVaryingGen.h:148` ⇒ `const uint data_id = gl_InstanceIndex;`；
   `ShaderLibrary/common/RenderItemResolve.glsl` 的直通模式以 `gl_InstanceIndex` 取行；
   `fragDataIndexID[vid/3u] = gl_InstanceIndex`（`MeshShaderVaryingGen.h:115`）。
   ⇒ **一条合并命令的 Y 个实例，其行号必须恰好是连续的一段**
   （`first_instance + 0 … first_instance + Y−1`），否则实例会读到**别人的** l2w/mtl 行。
4. **行/槽由 atomicAdd 决定数量与归属**（上引 `slot = atomicAdd(...)`）：
   数量是确定的，**相邻性不受保证**（同一段的两项可能拿到 slot=5 与 slot=9）。
   ⇒ 只改命令写法**必然错**（实例映射到不属于本段的行）；必须让"段内实例占连续槽"。
5. **draw 消费的"条数"就是每批计数**：`PipelineMaterialRenderer::ProcIndirectRender()`
   ⇒ `vkCmdDrawMeshTasksIndirectCount(first = slice.row_base, count = 切片计数(本批), max = slice.capacity)`
   （`src/ecs/support/PipelineMaterialRenderer.cpp:35-56`）。
   ⇒ **命令数 = 每批计数数组那一格的值**（今天 == 幸存项数 == 64）。
   ⚠ 由此得一条**结构性约束**：合并后"每批计数"要同时承担两个角色 —— gather 的**行槽分配计数**（按项）
   与 draw 的**命令条数**（按段）。今天两者恰好相等（1 命令/项）才没暴露；合并后**必须分开**
   （或让行槽计数由段基址+段内序号合成）⇒ 这是 T2 契约里要明写的一条。
6. **批是"同管线"不是"同几何"**（决定了合并是否必要）：`PrimitiveBatchPipeline.cpp:969-977`
   ⇒ `capacity = b->items.size()`，批键 = `b->key.shader_program`（管线）⇒ **一个批内可以有多种几何**
   ⇒ 段合并确实必要（不是"一批一命令"就完事）。
   同处还给出：`topology` / `group_size` 是**批级常量**（随 `SetBatchLayout(..., topology, group_size)` 下发）
   ⇒ **逐项键实际退化为 `geometry_id`**（它决定 X 与 mesh 侧经 `cmds[gl_DrawID].geometry_id` 的几何回读）
   ⇒ §2 顺带② 的键清单可据此收紧为「`geometry_id` + 批级 `topology`/`group_size`（后者批内恒定，
   只需在批级校验）」。
6. **判据基座已在**：`CPUCullReferenceSystem`（引擎默认系统，`src/ecs/core/DefaultSystems.cpp:112` 注册）
   已经 `SyncForParityReadback()` + `MapCurrentBatchCounters(0)` 读回每批计数与溢出槽
   （`src/ecs/support/CPUCullReferenceSystem.cpp:109` 起），并做三方值链对拍。
7. **⚠ CPU 路径已整条删除**（T4c4；未接管 = fail-fast 不画）⇒ §8.10 表里"CPU 1 条（Y=64）"是**历史事实**，
   不能再作为活判据的对照臂。替代：**CPU 侧按对拍数据推算期望运行段数**（见 §4）。

## 2. 形态 A / B（待拍板）

### 形态 A —— **块内合并（workgroup-local run-length encode）**（推荐）

- **做法**：gather 保持单 pass；每个 workgroup（`local_size_x = 64`）对自己处理的实体做
  **块内**三轮：① 逐实体算可见/标签 + 键；② **块内**（shared memory）算"段起始掩码 + 段内前缀和"
  ⇒ 段内序号；③ 每批每块一次 `atomicAdd(counters[batch], 块内向该批发出的**段数**)` 取段基址，
  段首项写命令 `(X, 段长, 1)`、段内各写 `first_instance = 段基址`（= 段首槽），四张行表按
  `row = row_base + 段基址 + 段内序号` 写。
- **合并范围**：段落在**同一个 64 实体块内**才合并。64 个同几何 cube（8×8 网格，实体号连续）恰好落在
  1~2 个块内 ⇒ 命令数 64 → 1~2 ✔ 典型场景拿满。
- **代价**：块内扫描（shared memory + 同步）、每批每块一次原子、段数取代项数进计数。
  **无新 pass、无新全局缓冲、无跨块顺序依赖**。
- **风险**：块边界不合并（可接受，写在契约里）；每块最多 64 项 ⇒ 段长 ≤ 64。

### 形态 B —— **全局扫描（多 pass：survivor 位图 → 两次前缀和 → 段基址）**

- **做法**：新增 2~3 个 pass：① 逐实体写"幸存 + 键"；② 对实体轴做前缀和（每批一段）得 survivor rank；
  ③ 在**压缩后**的键序列上算段起始（`key[r] != key[r−1]`）+ 段的 rank→段基址映射；
  ④ gather 按段基址 + 段内序号写行与命令。
- **收益**：**同一个批内所有相邻同键项**都合并（含跨块）
- **代价**：3~4 个 pass + 新 scratch 缓冲 + 每批 4096 实体的扫描（`EntityGPUTable::kCapacity`）+
  与 M4（切片 4 行对齐、计数末位 overflow 槽）、M5（竞技场容量）的交互面变大 ⇒ **与 §8.10 §八 说的
  "并行性改造会污染机制层判据"正面相撞**。
- **不推荐**：收益增量只在"同键段横跨 64 实体边界"时出现，而 S5/§8.10 记录的典型场景（cube 阵列、
  同材质批）本就连续 ⇒ 用 A 的 1/10 代价拿到绝大部分收益。

> **顺带把两件事一并钉住**（无论选哪个形态）：
> ① **确定性序号**：形态 A 的段内序号来自块内前缀和（而非 atomic 顺序）⇒ 行号分配变成**确定**的
>   （今天 atomicAdd 下"哪一项拿到哪个槽"是不确定的；对拍靠反查表所以无感，但确定性是额外的正向收益）；
> ② **键的定义**（= 段可合并的判据）必须与命令内容一一对应：`geometry_id`（决定 X = 组数）
>   + `binfo.topology`（线/三角影响 `element_count >>= 1`）+ `binfo.group_size`（X 分母）
>   + 每行的 `index_base/vertex_base/is_indexed`（决定 draw_params 回读的几何）——**键不覆盖它们就会
>   把不同 X 的项并进一条命令**（错画）。落地时把"键 = 这些字段的元组"写成契约。

## 3. 契约与判据变更（换维度）

| 现契约 | 变更 |
|---|---|
| 「命令 = 每项一条、Y=1」（§9 M3） | 「命令 = 每**段**一条，Y = 段长（≥1），`first_instance` = 段首槽」 |
| 「Σcount == 批内项数」（§8.10 §五） | 「**命令数 == 运行段数**」（每批计数值 = 段数）+「**Σ(Y) == 批内项数**」（实例总数守恒） |
| 「切片容量 = 行数」（M5） | 不变（行仍按项分配；命令数 ≤ 行数），但 overflow 槽语义订正为"**段数**超容量" |
| 「slot k ↔ gather 行 k」（对拍值链） | 变强：段内连续 + 确定性序号（对拍改按段首/段长核对） |

**判据（必须换维度，像素对这件事永远无感）**：
1. **命令数**（每批计数值 = draw 实际消费的条数）；
2. **ΣY == 批内项数**（读取 ICB 切片内命令的 Y 求和）；
3. 现有全套不变：三方像素全等 / `不一致=0` / `overflow=0` / 0 VUID / 八台架摘要逐位；
4. **台架**：扩 `GPUDrivenCull`（`GC_*` 家族已在）——`GC_CMDCOUNT=1` 打印
   `批 b: 命令数=… 项数=… ΣY=… 期望段数=…`，期望段数由 CPU 侧对拍数据推算（§4）。

## 4. 期望段数怎么算（判据的对照臂）

CPU 侧（`CPUCullReferenceSystem` 已有对拍数据：存活实体 + 每实体的 geometry/材质 + 批归属）：

- 对每个接管批 b：按**该批的项序**（实体号升序；形态 A 下 = 块内序号序）遍历幸存项，
  `key(i) == key(i−1)` 则续段，否则起新段 ⇒ **期望段数**。
- 形态 A 的期望段数还要按"块边界（每 64 实体）截断"再统计（块内不跨块合并）——
  这正是形态 A 的契约，写在判据里。

## 5. 分步实施（每步自身可编译 + 判据可跑；逐步回滚点）

| 步 | 内容 | 判据 |
|---|---|---|
| **T1** | **判据前置**：`CPUCullReferenceSystem` 加"命令数 / 项数 / 期望段数"三项回读与打印（`GC_CMDCOUNT=1` 门控；不算 ICB 的 Y 也能先报"命令数 == 项数"的**现状基线**） | `GPUDrivenCull` 跑出 `命令数=64 / 项数=64 / 期望段数=1`；其余判据不变 |
| **T2** | 契约与文档：M3/§五 口径改写 + 键的定义（§2 顺带①）写成源码契约（禁复活：`DrawMeshTasksIndirectCommand(…, 1u, 1u)` 直写形态） | `TestCSMIncrementalPass` 新契约绿 |
| **T3** | 形态 A 落地：块内键检测 + 段内前缀和 + 段基址 + 命令书写（Y=段长 / first_instance=段首槽）+ 行表按段内序号写 | `GC_CMDCOUNT=1` ⇒ 命令数=段数、ΣY=项数；三方像素/`不一致=0`/`overflow=0` 不变 |
| **T4** | 台架与破坏验证（把段键少写一项 / 段长算错 / 段基址错位 各造一次 ⇒ 必须红，且点名判据） | 破坏验证 3 条全有牙 |
| **T5** | 文档销账（backlog E2 → ✅、§8.10 §八 订正、技能更新） | 全仓 grep 无"一项一条"旧口径残留（历史标注除外） |

**回滚点**：tag `pre-e2-merge`。

## 6. 风险

| # | 风险 | 处置 |
|---|---|---|
| R1 | **行号连续性**被破坏（实例读到别人的行）⇒ 画错 | §1.3 的硬约束写进契约（T2）；台架三方像素 + `ΣY==项数` 双判据 |
| R2 | **键覆盖不全**（少一个影响 X/几何 的字段）⇒ 不同 X 的项被并 ⇒ 画错 | §2 顺带② 的字段清单写进契约；T4 用"少一个字段"做破坏验证 |
| R3 | 块边界不合并 ⇒ 命令数没降到位 | 契约里写明合并范围；台架按"期望段数（含块边界截断）"断言 |
| R4 | 段数与切片容量/overflow 槽语义错配 | T2 把 overflow 语义订正为"段数超容量"；`overflow=0` 仍是硬判据 |
| R5 | 对拍值链（slot k ↔ 行 k）失配 | 形态 A 引入块内前缀和后，序号变确定 ⇒ T3 同批更新对拍的口径 |
| R6 | 与 `HGL_FRAME_SLOT_TOTAL`/竞技场按 pass 分扇区（不按帧槽）的既有欠账交互 | 本任务**不碰**帧槽分份；竞技场欠账另记（§8.10 §十一 行集按帧槽分份） |

## 7. 待拍板

1. **形态**：A（块内合并，推荐）还是 B（全局扫描）？
2. 逐项键是否**只取 `geometry_id`**（已核实：`topology`/`group_size` 是批级常量，
   `index_base/vertex_base/is_indexed` 由 geometry 行唯一确定 ⇒ 取 `geometry_id` 即等价于取 X 的三要素），
   还是把 geometry 行的 `index_base/vertex_base/is_indexed` 也纳入元组（更严格：geometry 行若被运行期改写，
   仅比 id 会漏）？——倾向**纳入**（比较 5 个整数，代价可忽略，绝无"不同 X 并进一条"的风险）。
