# stage B（存储形）执行方案 v1

> 起草：2026-10-02（`D:\ULRE` 机器），对应 HEAD `43ef874cc`。
> **本方案不重复复述已定稿内容**，只做两件事：把 stage B 拆成**可独立验收的批次**；把**必须拍板的分叉**逐条列出（含推荐值）。
> 真源文档（口径以它们为准，不在本文复述）：
> - `doc/future/ULRE_FINAL_TARGET_v2_设计约束.md` §0–§3（总纲三条 / 行 arena 形态 / 句柄语义 / EntityGPU 布局）、§9（两阶段、五护栏 P1–P5、材质 1:N:N、中间态纪律）。
> - `doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md` §9.1（stage A 全记录）、§9.2（T10 任务书）、§9.3（T11，已延后）、§9.7（固定验证集）。
> - `doc/future/stage_B_debt_list.md`（A0–A7 遗留项与差距清单，逐条带 `path:line`）。
> - `doc/world-addresses-and-camera-model-plan.md`（地址三档 + 相机模型定稿）。

---

## 0. 设计输入

### 0.1 已定（不再讨论）

| # | 已定内容 | 出处 |
|---|---|---|
| 1 | 三条总纲：**单一真源** / **预算制**（声明式容量 + 预分配；超限 = 一次性明确告警，**不引入 Editor/Release 分支**）/ **CPU 权威 + GPU 派生** | v2 §0 |
| 2 | **每个 (scope, 组件类型) 一个定长行 arena = 一个 SSBO**；Entity 槽里存**该 arena 内的行号**；scope 静态（类型 → scope 是编译期常量表） | v2 §1 |
| 3 | **句柄语义**（不是租约）：长生命周期、析构不归还行；失效检测 = **行内 revision 校验**；句柄 24 B（4 B 填充装 revision） | v2 §2 |
| 4 | 五类 GPU 可见组件 = `Transform` / `MaterialData` / `MaterialRuntime` / `Geometry` / `Texture`；`persistent_id` 不引入 | v2 §3 |
| 5 | `EntityGPU` = `flags(4)+type[16](16)+row[16](64)+work_flags(4)` ⇒ `alignas(64)` ⇒ **128 B**；**16 槽** | v2 §3 |
| 6 | **顺序硬约束：T10（容量与行号冻结）必须早于 T11**；**T11 + `.ulrescene` 直载已延后**（触发条件 = 子场景树快速插入/展开设计完成） | 主计划 §9.2/§9.3 |
| 7 | 中间态纪律五条：每步可编译可测 / 同批删旧代码且 `grep` 零残留 / 禁 `if(新旧并存)`、`if(特例)` / 太大就切分不加兼容层 / 行为等价重排用**同输入同结果**对拍 | v2 §9.4、主计划 §9.1 头 |
| 8 | 固定验证集：build 0 error → 10 测试 rc=0 → 门 42 PASS/0 FAIL → 示例 0 真 error + `策略判定对拍` 不一致 0 → 行尾/BOM 逐文件保持；**清 obj 全量重编后才下结论** | 主计划 §9.7 + 本仓铁律 |
| 9 | 明确不做：GPU 侧 48 B TRS SSBO、L2W 改 `Matrix4x3f`、全局 `g_CurrentTransformStorage`、HLOD/prefab、场景直载（延后） | v2 §8 |

### 0.2 现状（实测，2026-10-02 本机）

| 项 | 现状 | 差距 |
|---|---|---|
| 类型表 | `inc/hgl/ecs/support/ComponentTypeTable.h:66-75`：6 项（`None` + 5 类），Transform 有 `gpu_row_bytes=64`，其余四类 `0` + `cpu_type_name="(待 ID 化)"` | **arena 只是字符串**：`arena_name` 全仓唯一消费者是契约测试（`src/ecs/support/TestTransformFlatStorage.cpp:30`）；**没有** `(scope,type) → 分配器/注册表` 的运行时实体 |
| 行存储 | 只有 Transform 一类（`TransformDataStorage`，`hgl::ValueArray` **自增长、无上限**、无分区、无预分配） | `Initialize(...)` 不存在；行号会随增长漂移 |
| GPU 行面 | `TransformAssignmentBuffer` 按需扩容并**重建** L2W/L2WIndex SSBO（`src/ecs/support/TransformAssignmentBuffer.cpp:163` `EnsureCapacity`、`:511` `L2W recreated`） | 行号/地址在运行期会变 ⇒ 与"预烘焙 ID / 后续场景直载"的物理前提冲突 |
| 值句柄 | 已是值句柄：`TransformAccessor` / `BoundingBoxAccessor`（24 B，世代校验） | `MaterialData` / `GeometryData` / `ShadowProxy` / `Camera` / `MaterialRuntimeSlot` 仍返回**裸指针**（`inc/hgl/ecs/core/Context.h:461-472`、`:553-583`、`:662-682`） |
| 组件载体 | `ComponentTypeOf<T>` 特化只有两处（`GeometryData`、`MaterialData`） | `MaterialRuntime` / `Texture` **无组件载体**（类型表已就位，无消费者） |
| `EntityGPU` | **零实现**（仅 `Entity.h:29`、`ComponentTypeTable.h:121` 两条注释） | 见 B8（延后） |
| `.ulrescene` | 全仓 **0 命中** | 见 B8（延后） |
| 行字节账目 | 由存储**自列**（`TransformDataStorage::GetPerRowFields`，19 条平行数组） | **B0 已实测闭合**：**每行 279 B**（其中派生缓存 `local_mat4+world_mat4` = 128 B）；279 − 64（`children` 32 + `fixed_pixel` 32）= 215 → 再扣 T8 元数据 26 B = 189 ⇒ 历史 189 B（T0 前）与 279 B（T8 后）两口径完全吻合 |
| 已有预算先例 | `MaterialRuntimeTable` / `MaterialVariantTable` 4096 行上限 fail-fast；`CameraInfoStorage` 16 槽 × 帧槽 | Transform 一侧**没有**任何上限声明 |

### 0.3 待拍板（每项：选项 + 推荐 + 影响）

**P-1 arena 落位：Global 类的 CPU 真值 + 行号空间住在哪？** —— ✅ **已拍板（2026-10-02，用户）：选 A**（Global 类型的行 arena 归设备级，由 `GraphicsContext` 拥有；世界/Entity 只持 arena 内行号）
- A（**推荐**）：按 v2 §1 字面执行 —— Global 类型（`Geometry`/`MaterialData`/`Texture`）的行 arena 归**设备级**（与 `GlobalSSBOBufferRegistry` 同域，由 `GraphicsContext` 拥有），世界/Entity 只持"arena 内行号" ⇒ 跨世界共享语义一致、行号全局可解析。代价：要为设备级 arena 确立 **CPU 侧真值宿主**（现有 registry 只有 GPU 行池）。
- B：沿用现状（世界级存储 + 私有容器），`Global` 只作语义标签 ⇒ 跨世界同资源重复行、行号非全局。
- 影响：选 B 之后再迁 A = 二次迁移（正是要避免的）；**这一项不定，B3/B4/B6 不能开工**。

**P-2 Transform 预算数值（B1 直接需要）** —— ✅ **已拍板（2026-10-02，用户）：`static 4096` / `dynamic 4096`**（超限 = 一次性明确告警 + 不扩容）
- 推荐：**static 4096 / dynamic 4096**（与材质行 1024、表 4096 同口径），超限 = 一次性明确告警 + 不扩容（fail-fast）。
- 分区：`Allocate` 按 `Mobility` 落段，静态段 `[0, static_budget)`、动态段其后；**预算分开**（静态段一次性排定，动态段按帧槽）。
- 影响：数值拍小 ⇒ 大型示例直接超限（可判定、可复现）；拍大 ⇒ 只多占内存。**若你要按实测峰值 ×N 的公式来定，请给 N。**

**P-3 `EntityGPU::type[16]` 的语义（B8 需要）** —— ✅ **已拍板（2026-10-02，用户）：选 A**（槽位 = `ComponentType` 值，每类型最多一行）
- A（**推荐**）：**类型固定位次** —— 槽位 = `ComponentType` 值，每类型最多一行 ⇒ 与 `Entity::component_mask`（位掩码）1:1、与 v2 §2"一个组件只占一行"一致、`implies` 表直接可用；16 槽 ⇒ 类型种类上限 16（现 6）。
- B：每实体最多 16 个组件**实例**（任意类型）⇒ 需要新的实例序号语义，与"一个类型一行"冲突。
- 影响：选 B 则 `ComponentType` 需要扩容 + `component_mask` 位图要换表示。

**P-4 `system_group_component_counts`（按组名字符串计数）** —— ✅ **已拍板（2026-10-02，用户）：按推荐执行**（随 B3–B6 收敛为类型位掩码驱动）
- 推荐：随 B3–B6 的组件转换，收成 **"类型位掩码 → 组"的编译期映射**（现有键域仅 3 个编译期常量字符串：`Primitive`/`Line`/`Text`），删掉运行期字符串哈希。
- 注意：它是**活逻辑**（挂卸即装/启组），A0 已把它从"零行为变化"批次里移出 ⇒ 只在对应组件批次里同批收敛。

**P-5 `MaterialRuntimeSlot` 的每实例指针/句柄字段（L2W / L2WIndex / MeshDrawCommand / MaterialInstanceAddresses / Indirect 任务缓冲 + offset）** —— ✅ **已拍板（2026-10-02，用户）：按推荐执行**（行内只留资源池行号 + 偏移，指针移出）
- 推荐：行内只留 **资源池行号 + 偏移**，指针移出到世界侧旁表/设备缓冲注册表（T8 教训：行内不放重型/指针类型，`math::AABB` 352 B 那次）。
- 影响：不定 ⇒ B5 无法定义行宽。

**P-6 相机终态** —— ✅ **已拍板（2026-10-02，用户）：不新开 (World,Camera) arena**，参数并入既有世界相机行空间
- 推荐：**不新开 (World, Camera) arena** —— 相机参数并入既有**世界相机行空间**（`CameraInfoStorage` 16 槽 × 8 帧槽 + 三级解析已定稿，C1–C3 刚整删过一批同类双源），`CameraComponent` 退化为参数载体/句柄。
- 备选：`(World, Camera)` 行 arena + 参数进 arena（把 19 个参数字段行化）。代价：与既有相机行空间形成第二个真源，需要一并裁掉一个。

**P-7 `positionSourceSpec` / `transformPolicySpec`（`RenderItem` 虚接口）** —— ✅ **已拍板（2026-10-02，用户）：本阶段不动**
- 推荐：**本阶段不动**（唯一消费者 = `PrimitiveBatchPipeline.cpp:292-300`），stage B 收尾时再评估是否降为每实例选择器。避免在存储批次里夹带接口重构。

---

## 1. 目标与非目标

**目标**：把 stage A 定下的职责一对一映射到 `(scope, 类型)` 行 arena + 值句柄 + 固定行号；每批独立可运行、可对拍。

**非目标**（本阶段明确不做）：GPU 侧 48 B TRS SSBO / `Matrix4x3f` L2W / 全局 `g_CurrentTransformStorage` / HLOD / prefab / 场景直载（**延后**，见 0.1-6） / Editor 分支 / 为通过验收而保留的兼容层或特例分支。

---

## 2. 批次划分（每批独立可运行）

### B0 —— 本机基线与验证集（**无代码改动，先做**）
- 内容：在本机把**固定验证集真正跑起来**（该 build 树此前只产出示例 exe，10 个测试与 `SchemaGate` 从未构建过），并跑一次 `ProbeTransformDiagnostics` 把行字节/`sizeof` 基线钉在本机数字上。
- 判据：① 10 个测试 exe + 门构建成功（build 0 error，仅 4 条既有 C4715）；② 10 测试 rc=0、门 42 PASS/0 FAIL；③ Test 9 打印的"19 条平行数组 × 每行 N B"记入本方案（**消除 189/279 两个口径**）；④ 探针输出 `sizeof(Entity/Component/TransformAccessor)`、每行字节、堆分配次数。
- 反证：无（不改代码）。若本机跑不出绿，先解决问题再开工 B1 —— **不允许在"验证集没跑过"的状态下开始改存储**。
- **✅ 已完成（2026-10-02，本机实测）**：
  - 构建：11 目标（10 测试 + `ShaderResourceSchemaRegressionGate`）+ 探针 `ProbeTransformDiagnostics`，`BUILD_RC=0`、`error` 计数 **0**。
  - 测试：10 个测试**全 rc=0**（`TestCSMIncrementalPass` 实测 **28 Passed**）。
  - 门：rc=0，实测 **[PASS] 42 / [FAIL] 0**。
  - 行字节（探针 + Test 9 双证据，口径一致）：**每行 279 B** / 19 条平行数组；`Test 9` 原文"7 行 × 19 条平行数组 = 每行 279 B，其中派生/缓存 128 B"。
  - `sizeof` 基线：`EntityID` 8 B / `Component` 104 B / **`TransformAccessor` 24 B** / `Entity` 168 B / `TransformDataStorage` 872 B。
  - 堆基线：n=1000 ⇒ 7.151 块/实体、643.5 B/实体；n=10000 ⇒ 7.000 块/实体、684.7 B/实体。
  - ⇒ **后续批次的结构改动一律与这组数字对比**（尤其 B2 的行字节：279 → 目标 215 B）。

### B1 —— T10-a：Transform 行空间冻结（声明式预算 + 预分配 + 超限 fail-fast）
#### ⚠ B1 侦察结论（2026-10-02 实测，**修正了 T10 的表述**）
T10 原表述是"`Allocate()` 动态扩容 ⇒ 行号漂移"，实测真因更靠前：**L2W 行号本身是"名单排名"的函数**，与 buffer 是否重建无关。
链条（全部有 `path:line`）：
- 片元/顶点侧取 L2W 走 `ResolveTransformID(gl_InstanceIndex)` ⇒ **`item->transform_index` 就是 L2W 行号**（`ShaderLibrary/vertex/helpers/orient_world.glsl:20` `l2w.mats[transformID]`；`src/ecs/support/PrimitiveBatchPipeline.cpp:959` 把它逐实例写进批内 index 行表）。
- 行号算法：静态 `transform_index = 静态排名 + 1`；动态 `= dynamic_base + 动态排名`，`dynamic_base = 静态数 + 1 + frame × 动态数`（`PrimitiveBatchPipeline.cpp:313`、`inc/hgl/vk/buffer/RingLayout.h` `GetBaseIndex`、`TransformSystem::RefreshHandleOrder` 按世界名单顺序发排名）。
- 全局"TransformID 行表"同样是排名口径：`rows[1+i] = 1+i`（静态）、`rows[1+static_count+i] = dynamic_base+i`（动态）——`src/ecs/support/TransformAssignmentBuffer.cpp` `WriteTransformIndexRows`。
⇒ **后果**：静态名单里插入/删除/换边任意一项，**其后所有静态物体的 L2W 行号整体平移**（不是"缓冲区重建"那么简单）；这也是"预烘焙 ID / 场景直载"不可行的同一根因。

**B1 因此切成两步**（都不引入兼容层）：
- **B1a（预算与一次性分配）**：`TransformDataStorage::Initialize(max_statics=4096, max_dynamics=4096)`（声明式预算 + 预留容量 + 超限**一次性明确告警**且不扩容）；`TransformAssignmentBuffer` 在 `Initialize` 期按预算**一次性**建 L2W / index 行表（行数 = `1 + max_statics + ring_frames × max_dynamics`），删掉 `EnsureCapacity` 的动态重建与 `L2W recreated` 路径。此步**不改行号含义**，只保证"容量/地址在启动后固定、永不重建"。
- **B1b（行号冻结）**：把 L2W 行号从"名单排名"改为**稳定槽位**的函数 —— 静态段 `row = 1 + static_slot`、动态段 `row = 1 + max_statics + frame × max_dynamics + dynamic_slot`；槽位在变换首次被分类时分配、**不再随名单顺序变动**。代价/语义：Mobility 换边 ⇒ 该物体换段 ⇒ **旧行号作废并留一次性告警**（这正是 §9.2 说的"行号已移动"情形；今天则是全体静默平移）。
- 验收（两步共用）：示例日志 `L2W recreated` **0 次**；行数运行期**恒定**；超预算场景报**一次性明确告警**且不扩容；10 测试 + 门 rc=0；示例对拍不一致 0。

#### ✅ B1a 已完成（2026-10-02，D: 机实测；清整棵 Debug obj 树后全量重编）

**改动**（7 文件 / `+243 −141`；行尾/BOM 逐文件保真，`TransformSystem.cpp` 维持 LF-only）：
- `inc/hgl/ecs/support/TransformDataStorage.h`：`kDefaultMaxStaticRows/kDefaultMaxDynamicRows = 4096/4096`（P-2）、`Initialize(static, dynamic)`（一次性 `Reserve` 全部 19 条平行数组 + `children`/`fixed_pixel`）、预算访问器与超限计数/告警闩、构造即 `Initialize()`；`Allocate()` 超预算 ⇒ **拒绝分配**（`INVALID_HANDLE`）+ **一次性明确告警**，行数不变。
- `inc/hgl/ecs/support/TransformAssignmentBuffer.h` + `src/ecs/support/TransformAssignmentBuffer.cpp`：构造带预算；`StatTransform` / `EnsureTransformIndexRowsCapacity` / `transform_policy` / `"L2W recreated"` / `power_to_2` **整体删除**，改为一次性 `AllocateBudgetBuffers()`；每帧入口 `EnsureCapacity(…, policy)` → `EnsureBudget(static, dynamic)`；设备 SSBO 上限不足时**按上限截断 + 报错**（不静默）；顺带修掉 `MaxTransformCount` 未初始化（无 device 时读未初始化值）。
- 调用点：`TransformSystem.cpp`（创建处传预算，真源 = 世界变换存储）、`LineRenderPipeline.cpp`（`EnsureBudget`）、`Context.cpp`（`CreateTransform` 对拒绝分配返 `INVALID_TRANSFORM_ID`）。
- 契约：`TestTransformFlatStorage` 新增 **Test 11**（行为：默认预算 4096+4096 / 预算内为追加序 / 超限拒绝 + 不扩容 + 告警闩 / 二次超限只增计数）与 **Test 12**（14 条禁复活源码契约：L2W 扩容重建路径 0 残留、预算期一次性分配在位、两个每帧入口均走 `EnsureBudget`）。

**实测证据**（rc 与数字均为真跑）：
- 全量构建 **rc=0 / 0 error**（清整棵 Debug obj 树后一次全量 + 一次降并发收口；既有 4 条 `C4715` 仍在 `src/InlineGeometry/*`）。
- **10 测试全 rc=0**；`TestCSMIncrementalPass` **28 Passed**；门 **42 PASS / 0 FAIL**；Test 11 / Test 12 均 Passed。
- 示例 `PBRSpheres` / `CascadeShadowMap` / `GizmoUsage` 各 8 秒：**`L2W recreated` = 0 次**（重建路径已消灭）；预算期一次性分配实测 **`L2W rows=36865（static=4096 + dynamic=4096）bytes=2359360`、`index rows=8193`**（36865 = 1 + 4096 + 8×4096，ring = 渲染目标帧数 8）；`budget_overflow_warn=0`（真实场景未触顶）；`策略判定对拍` **不一致 0**（collect 85200 / 167400 / 22500）。
- `sizeof(TransformDataStorage)`：**872 → 888 B**（+3 个 `uint32_t` + 1 个 `bool`，含填充）；每行字节仍 279 B（未动行布局）。
- **反证**（破坏 → 必红 → 逐字节还原 → 必绿）：把 `Allocate()` 的预算判定改成 `if (false && …)` ⇒ `TestTransformFlatStorage` **rc=13**、诊断 `超预算未拒绝分配（id=4 rows=5 overflow=0 warned=0）`；`sha256` 还原（`fcfc03b3…`）后重编 ⇒ **rc=0**、Test 11/12 Passed。
- 一次性告警实测有牙：Test 11 的告警行只出现 **1 次**（`[TransformDataStorage] 变换行预算超限：budget=4…`）—— 告警闩生效。

**⚠ 记账（需你拍板的一个副作用）**：L2W 现在是**每世界固定 2.36 MB**（`1 + 4096 + 8×4096` 行 × 64 B）——旧的按需分配对 100 物体场景只有约 8 KB。主导项是 **dynamic 4096 × ring 8**。若要收，只需把 `kDefaultMaxDynamicRows` 调小（例如 1024 ⇒ 786 KB/世界）；静止场景多数不需要 4096 个 movable 行。

**构建环境注意（本机踩到，已留档）**：清整棵 obj 树后全量构建会先撞 `TexConvCore` 的 **LNK1104**（`out\Windows_64_Release\TexImage.lib` 缺失）——这是计划文档 §9.6 记过的既有条件，**修复 = 先做一次 `--config Release --target TexImage`**（本次实测 `REL_RC=0`，之后 Debug 全量即 0 error）。另：`-j 8` 全量构建时 `vcpkg z-applocal`（POST_BUILD 拷 DLL）会因并发写同一输出目录报 **MSB3075「拒绝访问」**（目标文件当时并未被占用）⇒ **降并发（`-j 4`）重跑即过**，不是代码问题。


- 内容：① `TransformDataStorage` 支持**声明式容量 + 预分配**（行数组一次到位、空闲行复用走世代 `+2` 保持奇数，与 v2 §2 一致）+ **静态/动态分区**；② 删除 `TransformAssignmentBuffer::EnsureCapacity` 的动态扩容路径与 `L2W recreated`（改为预算期一次性建 L2W/L2WIndex，容量按预算固定）；③ 超预算 = **一次性明确告警 + 不扩容**。
- 落点（预期）：`inc/hgl/ecs/support/TransformDataStorage.h`（存储为 header-only）、`inc/hgl/ecs/support/TransformAssignmentBuffer.h` + `src/ecs/support/TransformAssignmentBuffer.cpp`、`src/ecs/systems/tick/TransformSystem.cpp:217`、`src/ecs/support/line/LineRenderPipeline.cpp:843`、`src/ecs/core/Context.cpp`（世界创建处 Initialize）、契约测试（`TestTransformFlatStorage` 行账目 / `TestCSMIncrementalPass` 探针口径）。
- 判据：① build 0 error；② 示例运行日志 **`L2W recreated` 出现 0 次**（构建期一次性建好）；③ 行数在整段运行期**恒定**且等于声明预算（探针/契约打印）；④ 构造超预算场景 ⇒ 一次性明确告警 + **不扩容**（文案可判定）；⑤ 10 测试 rc=0 + 门 rc=0 + 抽跑示例 0 真 error + `策略判定对拍` 不一致 0；⑥ `grep EnsureCapacity`（L2W 一侧）零残留。
- 反证：抽掉超预算 fail-fast（继续静默扩容）⇒ 对应断言必红；把预算调到示例规模以下 ⇒ 必报明确告警。


#### ✅ B1b 已完成（2026-10-02，D: 机实测；行号 = 稳定槽位）

**语义（终态）**：L2W 行号 = `静态 1 + slot` / `动态 1 + 预算static + frame × 预算dynamic + slot`；
槽位在变换首次登记时分配（**下标即槽位**），注销只留下空洞、由空闲栈复用 ⇒ **别的物体行号一格不动**。
改前是"名单排名"（`RefreshHandleOrder` 按世界名单顺序发排名）⇒ 删/加/换边任意一项都会让它后面的所有静态物体整体平移。

**改动**（10 文件）：
- `inc/hgl/ecs/core/Context.h` + `src/ecs/core/Context.cpp`：两个变换数组改为**槽位表**（下标 = 槽位、空洞 = `INVALID_TRANSFORM_ID`）+ 空闲栈；新增 `TryGetTransformSlot` / `GetLiveTransformCount` / `GetTransformRowMoveCount`；`RegisterTransform`（幂等登记 / 换段先归还旧槽）、`MigrateTransform`（归还旧段槽位 → 新段取槽 → **换边计数 + 一次性告警**）、`UnregisterTransform`（置空洞 + 入空闲表）全部重写；段预算用尽 = **一次性明确告警 + 拒绝**。
- `inc/hgl/ecs/support/TransformID.h`：新增 `INVALID_TRANSFORM_SLOT`（并注明 `0` 是合法槽位）。
- `inc/hgl/ecs/support/TransformAssignmentBuffer.h` + `.cpp`：`GetDynamicBaseIndex()` 改为**预算决定**（去掉 `static_count/dynamic_count` 参数）；两个写入口按**槽位表**寻址并跳过空洞（空洞行不写、无人引用）。
- `inc/hgl/ecs/systems/tick/TransformSystem.h` + `.cpp`：`static_index_map/dynamic_index_map` → **`static_slot_map/dynamic_slot_map`**（值 = 槽位，每帧由世界槽位表派生、值本身稳定）；`TryGetTransformGroupIndex` → **`TryGetTransformSlot`**；脏索引 = 槽位；`RefreshHandleOrder` 不再发明排名。
- `src/ecs/support/PrimitiveBatchPipeline.cpp` + `src/ecs/support/line/LineRenderPipeline.cpp`：`item->transform_index` / Line 的 transform_id 一律 `静态 1+slot` / `动态 基址+slot`。
- `TestTransformFlatStorage.cpp`：新增 **Test 13**（稳定槽位契约：销毁中间物体不影响他者槽位 / 空洞复用 / 换边换段并计数 / 段预算用尽拒绝分配）；Test 12 增 **7 条 B1b 禁复活契约**（`TryGetTransformGroupIndex`/`static_index_map`/`dynamic_index_map` = 0；槽位 API 与 `MigrateTransform` 通知在位）。

**实测证据**：
- 全量构建 **rc=0 / 0 error**；**10 测试 + 门全 rc=0**（门 42 PASS / 0 FAIL、`TestCSMIncrementalPass` 28 Passed、Test 11/12/13 Passed）。
- 示例 6 个（含**真用 Movable/Migrate 的** `SimplestAxis` 与 `PlaneGrid3D`）各 8 秒：`L2W recreated`=**0**、`L2W map failed`=**0**（⇒ 动态 ring 的 Map 偏移全在预算内）、`预算超限`=0、真 error 行 **0**、`策略判定对拍` **不一致 0**（PBRSpheres 85800 / CSM 168600 / Gizmo 22500）。
- **反证**：把 `ReleaseTransformSlot` 改回"erase 整体平移"（= 改前语义）⇒ `TestTransformFlatStorage` **rc=15**、诊断 `销毁中间物体后其它物体槽位漂移（A 5→5 / C 7→6）`；`sha256` 逐字节还原（`5c260188…`）⇒ **rc=0**。
- 换边告警实测有牙：`Test 8`/`Test 13` 输出里可见 `[ECSContext] Mobility 换边 ⇒ L2W 行号移动（transform=2 旧槽=2 → 新段槽=0）`。

#### ✅ B1b-2 已完成（2026-10-02）：删除只写不读的全局 index 行表

**取证（删之前）**：`ECS:LocalToWorldIndex` 这个 SSBO 全仓**没有任何消费者** ——
唯一引用是它自己的 getter `GetTransformIndexRowsBuffer()`；shader 侧 `l2w_index` 的地址 `pc_root.addr_l2w_index`
只由 **per-batch** `l2w_index_buffer` 经 `RootAddressPush::fill` 供给（`fill` 在 `nullptr` 时保持 0，
`inc/hgl/graph/RootAddressPush.h:64-74`）；按名 `grep LocalToWorldIndex` 的其余命中全是描述符时代的**类型枚举**（`SSBOTypes.h` /
`DescriptorSemantic.h` / `ShaderResourceSchema.h` / `DescriptorContract.cpp`）或 ShaderGen 的**类型名**，与这块 buffer 无关。

**删除面**：`TransformAssignmentBuffer.h/.cpp` 里的 `transform_index_rows_buffer` / `transform_index_rows_max_count` /
`GetTransformIndexRowsBuffer()` / `WriteTransformIndexRows()`（含 ctor 初始化、`Clear()` 释放、`AllocateBudgetBuffers` 的分配块、
`EnsureBudget` 的每帧重写调用）**整块删除**（−71 行）。全仓残留复核：`transform_index_rows` / `IndexRowsBuffer` /
`WriteTransformIndexRows` / `GetTransformIndexRowsBuffer` = **0**（`ECS:LocalToWorldIndex` 只剩删除说明注释）。

**实测证据**：全量构建 **rc=0 / 0 error**；**10 测试 + 门 rc=0**（门 42 PASS / 0 FAIL、Test 12/13 Passed）；
6 示例（`SimplestAxis`/`PlaneGrid3D`/`PBRSpheres`/`CascadeShadowMap`/`GizmoUsage`/`LineRender`）各 8 秒：
`L2W recreated`=0、`L2W map failed`=0、真 error 行 0、`策略判定对拍` 不一致 **0**；
分配日志现在只剩一条 `L2W rows=36865（static=4096 + dynamic=4096）bytes=2359360`（不再有 `index rows=8193`）⇒ **每帧一次 8193 元素的
`std::vector` 分配 + 一次 GPU 写彻底消失**，且省掉 32 KB 设备缓冲。

**⚠ 两项记账**：
1. **全局 index 行表 `ECS:LocalToWorldIndex` 是"只写不读"的死路** —— ✅ **已在 B1b-2 删除**（见上一节）。
2. **窗口示例的 warning 级日志被抑制**（6 个示例实测 0 条 warning）⇒ 新加的"换边告警"只能在单测里看到；若要在示例里肉眼确认，需要临时提升日志级别。
3. 构建环境：`vcpkg z-applocal` 的并发竞态本批又复现一次（这次报 `MSB3073`），**重跑 `cmake --build` 即过**；手动执行同一条 applocal 命令 rc=0，证明确非文件/权限损坏。

### B2 —— T10-b：行字节瘦身 —— ✅ 已完成（2026-10-02）

- **改动**：`children`（`std::vector<std::vector<HandleID>>`，32 B/行固定开销、绝大多数为空）→ **CSR**
  （`child_offset`/`child_count` 各 4 B/行 + 共享 `child_pool`；新增 `GetChildCount`/`GetChild`/`HasChild`/`CopyChildren`
  取代"返回 vector 引用"的旧口，`AddChild` 幂等 + span 贴池尾时零浪费续写，`RemoveChild` 行内前移保持区间连续）；
  `fixed_pixel`（32 B/行 × 全表）→ **稀疏 `unordered_map<HandleID,FixedPixelState>`**（只有被 gizmo 配置过的行才占位，释放即摘除）。
  顺带删掉零调用者的 `TransformAccessor::GetChildren()`（旧 API 形状无法在 CSR 上保留）。
- **实测行字节（Test 9 与探针两处一致）**：**每行 279 B → 223 B**（−56 B/行，−20%）；
  分解：−`children` 32、−`fixed_pixel` 32、+`child_offset` 4、+`child_count` 4。
  行账目仍是 19 条平行数组（两条 32 B 项换成两条 4 B 项）。**注**：文档旧填的"目标 215 B"是"两者都归零"的推算口径；
  实际 CSR 需 offset/count ⇒ **223 B 是实测值**，也是本步的真实下限（再压只能动 `local_mat4+world_mat4` 的 128 B 派生缓存，属另一条线）。
- **判据**：① build **rc=0 / 0 error**（purge 后全量）；② 10 测试 + 门 **全 rc=0**（门 42 PASS / 0 FAIL）；
  ③ 6 示例（含 gizmo）各 8 秒：`L2W recreated`=0、`L2W map failed`=0、真 error 行 0、`策略判定对拍` 不一致 **0**；
  ④ 新增 **Test 14**：fixed-pixel 稀疏表按行隔离 + 释放即摘除 + 子表 CSR 增删查与快照一致。
- **反证**：抽掉 `Deallocate` 里的 `fixed_pixel_map.erase(id)` ⇒ `TestTransformFlatStorage` **rc=16**、
  诊断 `行释放后 fixed-pixel 条目未摘除（稀疏表泄漏）`；`sha256` 逐字节还原（`8e4c9a20…`）后重编 ⇒ **rc=0**。
- `sizeof(TransformDataStorage)`：888 → 1016 B（存储对象每世界一个；行字节才是关键指标）。

### B3 —— Geometry 的 GPU 可用部分 ID 化（(scope,type) arena 第一次真实落地）—— ✅ 三批（B3-i / B3-ii / B3-iii）全部完成并验证

**语义订正（2026-10-02 拍板，修正本方案第一版的理解）**：目标**不是**"把 `GeometryData` 变成纯数据"，
而是**把其中 GPU 能直接寻址的部分抽成固定 ID 结构**，喂未来 GPUDriven/GPUScene —— 与
`doc/gpu-driven-4id-draw-item-plan-2026-09.md` 的 4-ID 方案同源。据此，`runtime_geometry` / `runtime_draw_range`
**不是"要找地方放"的指针，而是本来就该消失的中间物**。

**侦察结论（保留，实测）**：几何/材质两个组件都**不是纯数据**（几何 4 指针、材质整片重类型 + 指针），且全仓
**无 asset/纹理注册表**（`grep AssetManager|AssetRegistry|RegisterAsset` **0 命中**；示例把 asset 当栈对象自持，
`example/Material/PBRSpheres.cpp:103`）。

**字段对账（`MeshDrawParams` 行 vs `GeometryData`，全部实测）**

| `GeometryData` 现有 | `MeshDrawParams` 行（`ShaderBufferSources.h:15-40`，池行 = `geometry_id`） |
|---|---|
| `runtime_geometry`（`Geometry*`） | ✅ **取代它的就是行本身**（顶点流 9×BDA + 索引 BDA + meshlet 3×BDA + 头部字段） |
| `runtime_draw_range`.`first_index`/`vertex_offset` | ✅ **同源同值**：`index_base = Geometry::GetFirstIndex()`、`vertex_base = GetVertexOffset()`（`VKGeometry.cpp:119-120`） |
| `…`.`index_count`/`vertex_count` | ⚠️ 被压进 `total_vertices`（`VKGeometry.cpp:121`；CPU 组 ICB 算的是同一表达式，`PrimitiveBatchPipeline.cpp:719-720`）⇒ **本批拆开** |
| `…`.`data_index_count`/`data_vertex_count` | ❌ 行里没有 ⇒ **本批上 GPU**（全仓仅 `Primitive::GetDataVertexCount/GetDataIndexCount`，ECS 侧零消费者） |
| `runtime_data_buffer`（`GeometryDataBuffer`） | ✅ GPU 面被取代（SSBO 顶点方案**无 VIL/bindings**）；CPU 面只剩 `vab_*`，ECS 零消费者 |
| `bounding_radius` | ⚠️ GPU 形式是 `GeometryAABB`（32 B，`ShaderBufferSources.h:248-256`）但**尚无池** ⇒ 本批立池（见下） |
| `primitiveAsset*` / `variant_index` / `purpose` | ❌ 不可也不该取代：asset 明确「asset-only、by reference 可序列化、不持 SSBO 行」（`PrimitiveAsset.h:27-40`），变体只带 `material_recipe/purpose/lod_index`、**不带几何** |

**拍板记录（2026-10-02，用户逐条拍定）**

1. **先不扩 8ID** —— 变体不带几何 ⇒ `geometry_id` 粒度 = per asset/Geometry，一行足够。
2. **`data_*` 上 GPU** —— CPU 现在就用这套数据组 ICB（`PrimitiveBatchPipeline.cpp:719-720`），将来直接转 GPU 生成。
3. **`GeometryAABB` 独立一个 SSBO 表，与 `MeshDrawParams` 同 ID 索引（"一号双表"）**：
   - 收益：纯 AABB 剔除只读 **32 B/候选**（合并进行则 160 B/候选，**5×**）；池 16384×32 B = **512 KB**；地址侧只多**一个 BDA**。
   - **同构是结构保证，不是两个池碰巧一致**：靠 `ActiveRowPool::ActivateAllRows()`
     （`inc/hgl/vk/buffer/ActiveRowPool.h:97-106`，原话"用于行空间完全由调用方静态划分的池（如 CameraInfo）"）
     ⇒ **AABB 池不发号**，行号唯一来源 = MeshDrawParams 池。
   - **三条实现约束**：① 两行必须在**同一函数**成对写入（`Geometry::RegisterMeshDrawParams`，短路幂等 `geometry_id != 0`
     ⇒ 另开第二个写入点会让短路的几何永远漏写 AABB）；② 两池容量必须相等（断言 + 探针）；③ 释放只动主池（`~Geometry`），
     AABB 行留脏无害（ID 复用必然成对重写）。
   - **语义钉死**：AABB 是 **local/asset 空间**（池跨世界共享；`bounding_radius` 注释本就写明"不含实体缩放"）；
     剔除 pass 输入 = `{geometry_id 行号, transform_id}` = **4ID 的两项 ⇒ 不需要新 ID**。
     **零行/未写行必须"始终可见"**：哨兵 `radius < 0`（radius=0 会被判不相交 ⇒ 物体**静默消失**，违反 A7b 已立的安全侧哲学）。
   - **地址侧**：`GlobalAddresses` 加 `addr_geometry_aabb`（6 字段 48 B → 56 B，**步长 128 B 不变**；
     GLSL `GlobalAddressesRef` 同步加一行）。两道既有门兜底：`S.global-addresses-struct-parity`（`SchemaGate:2742`）、
     `S.global-addresses-field-ownership`（`:2900`）。
4. **三个小细节走推荐**：哨兵 = `radius < 0`；AABB 池 `reserve_rows = 1`（行 0 = 哨兵零行，与主池同构）；
   `GeometryData::bounding_radius` 改为**从 AABB 行反算**（消掉第二份数据源，注意它是 CPU 侧视锥剔除在用的量）。

**行结构定稿**

```
MeshDrawParams 行  112 B → 128 B
  index_base, vertex_base, is_indexed,
  index_count, vertex_count,             // total_vertices 拆开（该名与其语义不符：有索引时它存的是 index_count）
  data_index_count, data_vertex_count,   // ★ 上 GPU
  char_height, first_instance,  11×uint64 BDA 尾（不变）
GeometryAABB 行  32 B（独立池，同一 ID 索引；复用现有结构不改）
  center.xyz + radius(center.w), extents.xyz
GlobalSSBOType : + GeometryAABB（stride 32 B / cap 16384 / reserve 1）
```

**范围订正（据此，第一版"设备级 arena 宿主 + 24 B 值句柄 + 指针→行号注册表 + 173 调用点迁移"不再需要）**

`geometry_id` 是 **per `Geometry` 对象**、在 Geometry 创建期由 `EnsureMeshDrawParams` 取行（`VKGeometry.h:130`），
且**已经**由 4-ID 表逐 draw item 携带（`RenderItemDescriptor`，`PrimitiveState.cpp:78/215/235` 写入）⇒
实体行只需存/读这个 `uint32`，**不需要资源注册表**。剩下 `primitiveAsset*`/`variant_index`/`purpose` 是 authoring（CPU 侧），
本就**不该入行**。⇒ B3 的真实落点是下面四件事，而非"把 173 个调用点迁到新访问器"。
（"实体是否还需要一个 Geometry 行"留给 B4/B5 与 P-1 的设备级 arena 一并定。）

**静默失败面（必须先堵）**：`GetGlobalSSBOTypeStructStride`（`GlobalSSBOTypes.h:43-55`）是 `switch + default: return 0`
⇒ 新枚举漏 case **不报错**，而它有 **4 个消费者**（材质行寻址 / `RenderSceneUBOSystem` / ShaderGen 发射 / `MaterialSSBOLayout`）
⇒ 加 **constexpr 全枚举覆盖性断言**（for 遍历 + `RANGE_SIZE` 判定，不手列）。
反例好消息：`kGlobalSSBOConfigs[GlobalSSBOTypeCount]`（`GlobalSSBOBufferRegistry.cpp:13-20`）是定长数组 ⇒ 少一项**编译期报错**。

**B3-i 完成记录（2026-10-02）—— ✅ 已落地并验证**

改动面（5 文件）：

| 文件 | 改动 |
|---|---|
| `inc/hgl/graph/ssbo/SSBOTypes.h` | 枚举加 `GeometryAABB`（`ENUM_CLASS_RANGE` 尾项随之改）+ 类型注释写明"唯一不自发行号的类型" |
| `inc/hgl/graph/ssbo/GlobalSSBOTypes.h` | name 表补 case；`GetGlobalSSBOTypeName`/`StructVersion`/`StructStride` 三个表函数改 **`constexpr`**（否则覆盖性断言无法在编译期求值）；新增哨兵 `kUnknownGlobalSSBOTypeName` + **两条自动扩展断言**：名字表覆盖全枚举、`version != 0 ⇒ stride != 0` |
| `inc/hgl/graph/ubo/GlobalAddresses.h` | 加 `addr_geometry_aabb`（6 字段 48 B → 7 字段 **56 B**，槽步长 128 B 不变） |
| `ShaderLibrary/ubo/scene_ubo.glsl` | `GlobalAddressesRef` 同位置同字段（两门两端对表） |
| `src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp` | 配置表加 `GeometryAABB`（32 B / 16384 / **reserve 0**）+ 配置表覆盖性 `static_assert`（表序=枚举序、row_bytes/capacity 非零）；`InitializePools` 里 `ActivateAllRows()` 静态行空间 + **一号双表不变量 fail-fast**（两池容量一致、AABB 全行 Active）+ 探针日志；地址表写入 AABB 基址 |

**机制发现（实测，已修）**：`ActiveRowPool::ActivateAllRows()` 的前提是**行号空间为空**（其源码注释与行号连续断言都写明），
而 `reserve_rows = 1` 会让池先占 1 行 ⇒ 首批 512 号相对 `start` 整体偏移 ⇒ 只激活 **513** 行就 fail。
⇒ **AABB 池必须 `reserve_rows = 0`**；行 0 的"始终可见"哨兵改由写入侧显式写（B3-ii），不靠预留。
这是本批新加的不变量**当场抓到的真问题**（不是事后发现）。

**既有契约按新真值同步（两处，都不是放宽）**：
① `src/ecs/support/TestRenderItemDataStorage.cpp` 的 `static_assert(sizeof(GlobalAddresses) == 48)` 与逐字段偏移断言 → 改 56 B +
新字段 assert（偏移整体 +8）——正是它先把这次结构变化挡在编译期；
② 门 `kAddressOwnershipTable`（`ShaderResourceSchemaRegressionGate.cpp:2881` 附近）登记 `addr_geometry_aabb`（设备级全局），
否则"未分类字段"报红（门 §6 契约④）。

**验证（rc/数字均实测）**：build **rc=0 / 0 error**（`-j 1` 串行；见下方环境坑）；
门 **42 PASS / 0 FAIL**（含 `S.global-addresses-struct-parity`、`S.global-addresses-field-ownership` 两门都认了新字段）；
10 测试全 rc=0；4 示例（`PBRSpheres`/`CascadeShadowMap`/`GizmoUsage`/`LineRender`）各 8 秒：真 error 行 0、
`策略判定对拍` 不一致 **0**，且探针实测非空有牙：
`一号双表就位：GeometryAABB 与 MeshDrawParams 同 ID 索引 rows=16384（AABB 32 B/行）aabb_bda=0xffffb802023c0000`。

**反证（两处，均逐字节还原后转绿）**：
① 运行时——把 AABB 池容量改成 8192（与主表不等）⇒ 不变量必红：
`一号双表不变量失败：MeshDrawParams rows=16384 AABB rows=8192 active=8192` + fail-fast（APP_RC=127）；
`GlobalSSBOBufferRegistry.cpp` sha256 `d106936f4cd1c4de…` 还原。
② 编译期——去掉 `GetGlobalSSBOTypeName` 的 `GeometryAABB` case ⇒ `static_assert`（C2338）
`GetGlobalSSBOTypeName 必须覆盖 GlobalSSBOType 的全部枚举项`；`GlobalSSBOTypes.h` sha256 `8d985899f27c9dd9…` 还原。

**环境坑（留档，非代码问题）**：`vcpkg z-applocal` 的并发拷贝竞态本批表现为**单个目标持续失败**
（`BidirectionalMapStressTest_06_BatchOperations.vcxproj`，`-j 2/-j 4` 均复现，手动跑同条命令 rc=0）
⇒ `-j 1` 串行构建即 **rc=0 / 0 error**。判据仍以"构建 rc=0 且 C/LNK 错误为 0"为准。

**B3-ii 完成记录（2026-10-02）—— ✅ 已落地并验证**

一、行宽与字段（`MeshDrawParams` 112 → **128 B**）

| 层 | 改动 |
|---|---|
| X-macro 单源（`ShaderBufferSources.h:15-40`） | `total_vertices` 拆成 **`index_count` / `vertex_count`**，并补 **`data_index_count` / `data_vertex_count`**（用户拍板"data_* 上 GPU"）⇒ 头部 9×4B + 4B 对齐填充 + 11×uint64 = 128 B |
| 布局断言 | 头部计数 6→9、基址首偏移 24→40、`sizeof == 128`（三处漂移任一即编译失败） |
| GLSL 读侧（3 个 tmpl） | `char_quad` / `line_quad` 改读 `draw_params.vertex_count`；`vertex_passthrough` 改为**按 `is_indexed` 择一**：`(is_indexed != 0u) ? index_count : vertex_count` —— 与旧 `total_vertices` 的取值**逐位等价**，但 GPU 侧自辨 |
| 写侧（3 处） | 几何（`VKGeometry.cpp`）填 4 个计数；线段（`LineRenderPipeline.cpp`）`vertex_count = 线段数×2` + data_*；文本（`TextRenderPipeline.cpp`）`vertex_count = 字符数` + data_* |

二、AABB 行写入（真正的坑：**顺序**）

- 初版把 AABB 行写在 `RegisterMeshDrawParams` 里，实测**全部写成哨兵**（`radius=-1`）。
  真因：`GeometryCreater::CreateWithAABB` 是"**先 `Create()` 后 `SetBoundingVolumes`**"⇒ 行在包围体到位前就写完，
  而 `EnsureMeshDrawParams` 的 `geometry_id != 0` 短路让后续调用不会补写。
- 修法：AABB 行收敛为**唯一写入口** `Geometry::RefreshGeometryAABBRow()`，由
  ① `RegisterMeshDrawParams`（创建期）与 ② `SetBoundingVolumes`（来源变化）共同调用。
  ⇒ 实测每个几何写两行（创建期哨兵 + 真值），PBRSpheres **10/21 行为真值 AABB**，
  且 id=1 半径实测 **1.7321** 与 CPU 侧旧公式 `|(2,2,2)|×0.5` **逐位一致**（"同源同式"得证）。
- 行 0 哨兵（`radius = -1` = 始终可见）在 `InitializePools` 写入；`GeometryAABB` 池 `reserve_rows = 0`
  （B3-i 实测：`ActivateAllRows` 要求行号空间为空）。

三、`bounding_radius` 反算（消第二份数据源）

`GeometryData` 删掉缓存成员，改 `float GetBoundingRadius() const`（按需从 asset 几何的本地 AABB 反算，
与 AABB 行写入同式）。全仓 `bounding_radius` 残留 **0**。

四、验证（rc / 数字均实测）

- build **rc=0 / 0 error**（`-j 4` 若撞 applocal 竞态则 `-j 1` 收口）；
- 门 **42 PASS / 0 FAIL**；10 测试全 rc=0，新增 **Test 29**（B3 契约 25 条，含 6 条禁复活：
  `M(total_vertices,` / `draw_params.total_vertices` / AABB 池 `reserve 1` / 组件内两份 `bounding_radius`）；
- 示例（覆盖三条着色路径）`PBRSpheres` / `CascadeShadowMap`（网格）、`TextDraw`（文本）、`LineRender`（线段）：
  各 8 秒 0 真 error、策略判定对拍不一致 **0**；探针实测 `一号双表就位… rows=16384（AABB 32 B/行）` +
  `GeometryAABB 行写入`（网格示例 10 行真值 / 文本与线段示例 0 真值 ⇒ 全为"始终可见"哨兵）。

五、反证（均逐字节还原后转绿）

- 抽掉 `SetBoundingVolumes` 里的 `RefreshGeometryAABBRow()` ⇒ 真值 AABB 行数 **10 → 0**（全哨兵）；
  `VKGeometry.h` sha256 `ac8a46b3124f7e63…` 还原。

六、已知缺口（不粉饰，非本批引入）

- **文本 / 线段几何没有包围体** ⇒ 其 AABB 行是"始终可见"哨兵（CPU 侧同样 radius=0 ⇒ 从不剔除）——
  这是既有的 authoring 缺口，不是本批回归；要 GPU 剔除真生效需给这两类几何补包围体。
- `data_*` 字段已写好但**暂无消费者**（按你"CPU 先跑完 ICB 生成、再转 GPU 生成"的路线，属预留）。
- 读 AABB 行的 **GPU 剔除 pass 尚未接**（尾巴项：`ComputeFrustumCull` 改读引擎池）。

**B3-iii 侦察结论（2026-10-02，未动手 —— 规模订正 + 待拍板一点）**

上一版本方案把 B3-iii 记成"ECS 读 ID 化 + 删三指针（5 个消费点）"。实测**远不止**：那三个指针是
**批处理的主键**，删它们等于改批排序 / 聚簇语义：

| 耦合点 | 现状（实测） | 改写方向 |
|---|---|---|
| `RenderItem::Compare`（`src/ecs/core/RenderItem.cpp:8-52`） | **批排序主键** = `*GeometryDataBuffer <=> *…`（内容比较：geometry_id / vdm / vab / ibo）+ `*GeometryDrawRange <=>`（vertex_offset / first_index / counts）。注释明写"必须内容比较：指针比较会打散同几何项 ⇒ 失去实例化合并" | 改比较**行派生 POD 键**（`geometry_id` + `vertex_base`/`index_base`/`vertex_count`/`index_count`/`is_indexed`） |
| `DrawBatch::geom_data_buffer` / `geom_draw_range` + `Set(...)`（`PipelineMaterialRenderer.h:62-79`） | 每批持两个 CPU 对象指针；`Set()` 再由 `data_buffer->geometry_id` 定 `geometry_id` | 换成 POD `MeshDrawKey`；**4 个建批点**（`PrimitiveBatchPipeline.cpp:491/552/570/588`）改传键 |
| `BatchRequiresIndirectCommands`（`PrimitiveBatchPipeline.cpp:56-63`） | "任一 item 有 `data_buffer` ⇒ 需要 ICB" | "任一 item 的键有 `geometry_id`" |
| 聚簇循环（`PrimitiveBatchPipeline.cpp:408-470`） | 按 `*data_buffer != *next` / `*draw_range != *next` 断簇，并顺手修 `data_buffer->geometry_id` | 按键相等断簇（POD 比较） |
| ICB 组数（`PrimitiveBatchPipeline.cpp:714-728`） | `range->index_count > 0 ? index_count : vertex_count` | 键的 `DrawElementCount()`（= 行里 `is_indexed ? index_count : vertex_count`，**逐位等价**） |
| 4-ID 填充 + 列武装（`RenderPrimitiveCollectSystem.cpp:216-254` / `:2314-2341`） | `EnsureRuntimeGeometryFromAsset()` 每帧按程序解析几何缓冲；4ID 的 `geometry_id` 经 `geom_buf->geometry_id` 取（带三级回退） | 采集期**从行填 `item->draw_key`**；`geometry_id` 直读 `GeometryData::GetGeometryID()`；删该武装函数与其 **3 个调用点**（`:2187` / `:2228` / `:2280`） |

**需要拍板的一点**：排序/聚簇键改"行派生 POD 键"后，**每实体 `GeometryDrawRange` 的差异不再参与判定**
（同 asset ⇒ 同几何 ⇒ 同一行 ⇒ 必然同键）。对纯几何路径**等价**（现状同 asset 的两个 CPU 副本内容也相同）；
唯一例外是**文本 / 线段**那两条"每批重写同一个几何的行"的路径（`TextRenderPipeline.cpp:907` /
`LineRenderPipeline.cpp:655`）——它们的批不参与这条排序（单批），故实际影响为零，
但"共享行被每批改写"本身就是历史遗留，正是你那句"CPU 也以这套数据跑完 ICB 生成"要收敛的对象。
**建议**：按上表改（行键 = 唯一判据），并把"文本/线段每批改写共享行"记成独立欠账（不属 B3-iii）。

**为什么本批停在"侦察"**：该批会改**实例化合并的判定**（行为可见），按"先证前提再改"的规矩，
此规模与语义需你点头后再动手；当前树保持 B3-ii 已验证状态（本轮试改的 6 个文件已 `git checkout` 还原）。

**B3-iii 完成记录（2026-10-02）—— ✅ 已落地并验证（用户拍板：按建议改；文本/线段属"动态 GEOMETRY"，以后再谈）**

一、改动面（12 文件）

| 层 | 改动 |
|---|---|
| `RenderItem.h` / `.cpp` | 新增 **`MeshDrawKey`**（24 B POD：`geometry_id` / `vertex_base` / `index_base` / `vertex_count` / `index_count` / `is_indexed`，含 `HasGeometry()` / `DrawElementCount()` / defaulted `operator==`）与字段 `draw_key`；`Compare` 从"两份 CPU 副本逐字段比较"改为**行键 POD 比较**；删两个纯虚 `GetGeometryDataBuffer/GetGeometryDrawRange` |
| `PrimitiveRenderItem.h` / `.cpp` | 删两个 override 与实现 |
| `GeometryData.h` / `.cpp` | 删 `runtime_data_buffer` / `runtime_draw_range` / `runtime_geometry` 三指针 + `EnsureRuntimeGeometryBinding` / `ClearRuntimeGeometryBinding` 整组（−3.3 KB）；`GetGeometryID()` 改为**按需从 asset 的几何反算**（不缓存第二份 ⇒ 无"几何换行号、组件还是旧的"漂移） |
| `PipelineMaterialRenderer.h` | `DrawBatch` 去掉两个 CPU 指针字段，改持 `RenderItem::MeshDrawKey draw_key`；`Set(key, geom)` |
| `PrimitiveBatchPipeline.cpp` | 6 处：`BatchRequiresIndirectCommands`（判据 = 键有几何）、聚簇头/循环（**按行键相等断簇**）、4 个建批点、命令 `geometry_id`（直取行键，删三级回退与镜像回写）、ICB 组数（`db.draw_key.DrawElementCount()`） |
| `RenderPrimitiveCollectSystem.cpp` | 删 `EnsureRuntimeGeometryFromAsset`（每帧按材质程序武装 CPU 副本）与其 **3 个调用点**；新增 `EnsureGeometryRowReady`（判据 = **行就位**）；4ID 的 `geometry_id` 直读 `GeometryData::GetGeometryID()`；采集期**从行填 `item->draw_key`** |
| `GlobalSSBOBufferRegistry.h` / `.cpp` | 新增 `GetMeshDrawParamsRow(geometry_id)`（行 CPU 读取口，"CPU 也以这套数据跑"） |

二、等价性论证（为什么是等价重排，不是语义改动）

- **批排序/聚簇键**：旧 = `*GeometryDataBuffer <=>`（geometry_id/vdm/VAB 列表/ibo）+ `*GeometryDrawRange <=>`；新 = 行键。**同 asset ⇒ 同几何 ⇒ 同一行 ⇒ 必然同键**；旧实现下同 asset 的两个每实体副本内容也相同 ⇒ 判定等价，且新键是纯 POD 比较（比旧的比 VAB 列表更快）。
- **ICB 组数**：旧 = `range->index_count > 0 ? index_count : vertex_count`；新 = `is_indexed ? index_count : vertex_count`（行内两字段同源）⇒ **逐位等价**。
- **几何就绪判据**：旧 = 每帧能武装出 CPU 副本；新 = 行就位（`geometry_id != 0`）。无 asset/无几何时两者都返回 true。
- **命令几何行号**：旧三级回退（`db.geometry_id` → 副本镜像 → `geometry->GetGeometryID()`）；新 = 行键的 `geometry_id`（采集期从行填入，同源）。

三、验证（rc / 数字均实测，且**以清 obj 全量重编后的运行为准**）

- build **rc=0 / 0 error**（清整棵 Debug obj 树后 `-j 5` + `-j 1` 收口）；
- 门 **42 PASS / 0 FAIL**；10 测试全 rc=0，新增 **Test 30**（B3-iii 契约 18 条，含 6 条禁复活：三指针/武装函数/RenderItem 暴露副本/DrawBatch 两个字段）；
- 示例帧代理与基线一致（8 秒窗口）：`PBRSpheres` **287 vs 284**、`GizmoUsage` **75 vs 72**、`AutoMergeMaterialInstance` **33 vs 34**、`CascadeShadowMap` 446（基线 554，属机器负载浮动）、`LineRender/TextDraw` 无对该行（与基线同）；
- 指纹一致：`Skip primitive item` 计数 **223 / 0 / 0**… 与基线逐项相同；各示例 **0 真 error**、策略判定对拍无一条非零不一致；
- 三路径实跑覆盖：网格（PBR/CSM）、线段（LineRender）、文本（TextDraw、SDFTextEffects）。

四、⚠ 过程教训（本批踩到，已写进技能）：**构建失败留下的"半新半旧"二进制会伪装成性能回归**

首次清树后构建曾失败（`PipelineMaterialRenderer.h` 的 C2653、`GeometryData::GetBoundingRadius` 的 C2248、`GetMeshDrawParamsRow` 缺声明等）。修好并"rc=0"后运行，出现**每帧 8.5× 变慢、CPU 空闲（8 秒仅 0.06 s）、逐帧日志内容完全一致**的假象；用 `git stash` 做 A/B 时，**pre-change 侧是全新构建、new 侧却是半旧产物** ⇒ 得出"我的改动导致回归"的错误结论。清 obj 全量重编后，**同一份代码**帧率回到基线 ⇒ 判为构建产物不一致，非代码回归。
**规则**：① 结论一律以"清 obj 全量重编 + rc=0"之后的运行为准；② 做 A/B 时**两侧都必须是全新构建的二进制**（一侧半旧 ⇒ 结论无效）；③ 症状速查：CPU 几乎不耗、逐帧内容不变而墙钟变慢/变快 = 先怀疑二进制不纯，不要先怀疑算法。

五、遗留（用户明确"以后再说"）

**文本 / 线段 = 动态 GEOMETRY**：这两条路径仍是"每批改写同一个几何的行"（`TextRenderPipeline.cpp:907` / `LineRenderPipeline.cpp:655`），
行被当每批计数器用（谁最后写谁赢）。收敛方向 = 行每几何一份不可变、绘制量差异进每 draw item 的数据；本批不动。

**B3 尾巴（2026-10-02）—— ✅ `ComputeFrustumCull` 改读引擎 `GeometryAABB` 池（自建 AABB 表退役，二号表被 GPU 真正消费）**

一、改动（单文件 `example/ApiValidation/ComputeFrustumCull.cpp`，13 处）

- **删**：成员 `cpu_geometry_aabbs[64]`、成员 `geometry_aabbs_buffer`、其 `CreateBuffer` 调用、空指针检查项、清理分支、以及"把自己缓冲地址填进地址表"那行 ⇒ 示例不再自建 AABB 表；
- **改**：地址表 `geometry_aabbs` = `registry->GetGPUBase(GlobalSSBOType::GeometryAABB)`
  （与 `GlobalAddresses::addr_geometry_aabb` 同源）；候选 `geometry_id` = `geometry->GetGeometryID()`
  （MeshDrawParams 行号 = GeometryAABB 行号，一号双表）；**CPU 真值也从 `registry->GetGeometryAABB(id)` 取行**
  ⇒ 与 GPU 同源，配对校验的是剔除数学而非表的一致性；
- **加**：创建期校验该行存在并打印实测值（缺行即 fail-fast，不静默）。

二、验证（实测数字）

- 引擎行实测：`geometry_id=1 center=(0,0,0) r=0.8660 extents=(0.5,0.5,0.5)`（r = |(0.5,0.5,0.5)|）
  —— 与示例原先硬编码的 ±0.5 **逐位一致**；
- 示例自带每帧双面对拍（GPU 读回 visible 数 vs CPU 真值）：**PASS=8 / FAIL=0**（8 秒窗口）；
- **指纹等价**：可见数序列与基线仅 2/8 帧差 1 个元素；而**同一新二进制连跑两次**同样在基线之外互差（45/44）
  ⇒ 判定为运行间抖动（时间驱动相机 + 帧时差），非本次改动；
- **反证（证明 CS 真按 `item.geometry_id` 读引擎池）**：把地址临时 `+32B`（指向下一行 = 未初始化行）⇒
  `MISMATCH: Frame 1, GPU visible=28, CPU visible=36`、**7 FAIL / 1 PASS**（GPU 结果随该地址变化）；
  逐字节还原（sha256 相同）后回到 **PASS=8 / FAIL=0**；
- 构建 rc=0 / 0 error；仅示例文件改动、**引擎字节未动** ⇒ 门 42/0 与 10 测试沿用同一份引擎字节上一轮的结果。

三、遗留（已记入欠账清单 §2.7）

`example/ApiValidation/ComputeAsteroidBelt.cpp` 有同类自建 AABB 表（`cpu_geometry_aabbs[10]` + 10 个变体
`default_extents`），本批未动。障碍：其 `planet_geometry` 来自模型加载，**池行是否有真值未验证**（可能是"未设"哨兵）
⇒ 改造前须先确认该几何的 `SetBoundingVolumes` 路径。另外该示例的候选用变体下标 `geom_id` 索引自建表，
改造时需一并换成引擎行号。

**B3-iii 缺陷修正（2026-10-02，用户当场发现）—— GPU-Driven 批次被合并成多实例簇 ⇒ 屏幕上只剩 1 个实例**

一、现象与定位（全部实测）

- **现象（用户报告）**：运行 `ComputeFrustumCull` 只显示 **1 个立方体**（中心区全无）。截图确认后逐级二分：
  ① 把示例文件回退到 B3 尾巴之前 ⇒ **仍然只有 1 个**（排除 B3 尾巴）；
  ② 把 B3-iii 的 12 个引擎文件回退到 `758fb8b51` ⇒ **整片立方体恢复** ⇒ **B3-iii 引入**。
- **根因**：重写聚簇循环时**漏掉旧实现里的 `batch.gpu_driven_override ||` 判据**。该判据的语义是
  **GPU-Driven 批次必须每项自成簇**：这类批次的绘制指令 / L2W 索引槽 / mesh 命令行都由表**逐项**给出
  （第 i 项 ↔ 第 i 行/第 i 槽一一对应）；合并成多实例簇后整簇共用首实例 ⇒ 只剩 1 个实例的变换。
  旧代码：`if (batch.gpu_driven_override || *data_buffer != *next_data_buf || *draw_range != *next_draw_range) break;`

二、修法与验证

- **修法**：在聚簇循环恢复 `if (batch.gpu_driven_override) break;`（+ 注释说明为何必须逐项成簇）；
  并给 Test 30 加**契约 ④**（用 16 空格缩进那行作单行 needle —— 实测该行在文件内唯一 ⇒ 不受行尾影响）。
- **反证**：去掉该判据 ⇒ Test 30 **rc=30**（"GPU-Driven 批的逐项成簇判据丢失…"）；
  逐字节还原（sha256 `de6d5031…`）⇒ rc=0。
- **修复后验证（清 obj 全量重编）**：build rc=0 / 0 error、门 42 PASS / 0 FAIL、10 测试 rc=0；
  `ComputeFrustumCull` PASS=8 / FAIL=0 且**截图恢复整片立方体**；
  PBRSpheres / ComputeAsteroidBelt / AutoMergeMaterialInstance / CascadeShadowMap **四示例截图均正常**。

三、⚠ 教训（本批最重要的一条）

这次事故的**数字面全绿**：示例自身的每帧双面对拍 0 FAIL、`策略判定对拍` 不一致 0、`Skip primitive item`
计数与基线一致、build/门/测试全过 —— **只有像素能看出问题**（同类"数字全绿但画面错"在本轮出现两次：
另一次是构建产物不纯伪装成 8.5× 性能回归）。
⇒ **规则：凡改动批 / 绘制 / 实例化 / 存储布局语义，验收必须加一步"看屏幕"**（Windows 上直接
`computer_use capture` 截目标窗口，或 RenderDoc 截帧）；日志与计数只能证明"没崩、数量对"，
证明不了"画出来是对的"。

**B3 剩余分批**（原判据不变）

| 批 | 内容 | 判据 |
|---|---|---|
| **B3-ii 行定稿** | `MeshDrawParams` 112 → 128 B（拆 `total_vertices` + 补 `data_index_count`/`data_vertex_count`）；AABB 行**成对写入**（`RegisterMeshDrawParams` 内）+ 行 0 负半径哨兵；`bounding_radius` 改从 AABB 行反算 | 探针行宽 128/32、X-macro 单源同步、断言齐 |
| **B3-iii ECS 读 ID 化** | 采集系统改读 ID（`RenderPrimitiveCollectSystem.cpp:2318` 现经 `runtime_data_buffer->geometry_id` 绕道取 ID）；**删 `runtime_*` 三指针**（消费点 5 处：`GeometryData.h:92-95`、`PrimitiveRenderItem.cpp:91/98`、`RenderPrimitiveCollectSystem.cpp:253/2318`） | 10 测试 + 门 + 示例绿、对拍不一致 0 |
| 尾巴（可选） | 示例 `ComputeFrustumCull` 自建 AABB 表退役、改读引擎池（引擎级 GPU 剔除） | 剔除数一致 + 同图对比 |


### B4 —— MaterialData 句柄化（同 B3 范式）—— 侦察记录 + 分批定稿（2026-10-02）

#### B4-0 现状（实测，本机）

| 项 | 现状 | 差距 |
|---|---|---|
| 组件 | `inc/hgl/ecs/components/MaterialData.h`（161 行）：`has_recipe_override` + `recipe_override`（重类型）、`named_texture_resources`（`UnorderedMap<AnsiString,…>` 重类型 + `Texture*`/`Sampler*`）、`data_resource`、`authored_generation`（单调计数器） | GPU 面仍挂**每实体指针 + 派生数** |
| GPU 面 | `data_resource` 的基类 `GlobalSSBOBinding{ssbo_type, ssbo_id, data_index}`（`GlobalSSBOTypes.h:101-113`）**已经是「类型 + 行号」** ✓ | 派生类又加了 `DeviceBuffer *buffer` / `element_capacity` / `byte_stride`（`MaterialData.h:60-63`） |
| **死字段取证** | `buffer` / `element_capacity` / `byte_stride`：**外部读者 0**（唯一出现是 `MaterialData.cpp:34-36` 清零；`SetDataResource` 只整体赋值、不读它们；`GetDataResource` 外部消费者 **0 处**）；**示例侧赋值 0 处**（全部写法是 `named_struct = accessor.GetGlobalSSBOBinding(); SetDataResource(named_struct);`） | 容量/步长的权威真源另有其人：`SSBOBufferRegistry::RegisterBuffer(…, element_capacity)` 用「字节数 ÷ 容量」反算 stride（`src/SceneGraph/module/SSBOBufferRegistry.cpp:280-299`） |
| 材质行池 | **已存在且设备级**（`GlobalSSBOBufferRegistry` 配置表）：`PBRSurface` 32 B/行、`EmissiveSurface` 16 B/行、`TransmissionSurface` 16 B/行，各 **1024 行**、`reserve_rows = 1`（首个 `Acquire` = 1，行 0 = 哨兵） | registry 只有 GPU 行池，**没有 CPU 侧读口**（B3 已给 MeshDrawParams / GeometryAABB 补过，材质侧还没有） |
| 访问器 | `ECSContext::GetMaterialData` / `GetOrCreateMaterialData` **4 个重载返回裸指针**（`Context.h:461-472`、`:553-583`）；`Context.h` 注释已写明「stage B 会换成值类型句柄（与 TransformAccessor / BoundingBoxAccessor 同构）」 | 24 B 值句柄先例已在：`BoundingBoxAccessor`（`{row, generation}` + `IsValid()`，`inc/hgl/ecs/support/BoundingBoxAccessor.h:23-30`） |
| API 消费者规模 | `SetTextureResource` **72**、`SetDataResource` **47**、`SetRecipe` 3、`HasRecipeOverride` 3、`BuildResolvedRecipe` 1、`GetAuthoredGeneration` 1、`BumpAuthoredGeneration` 1、`GetTextureResource` 2；`GetDataResource` / `ClearAuthoredResources` / `GetRecipeOverride` **0** | 句柄化会触及全部 `GetMaterialData*` 调用点 |
| 渲染期读者 | `PrimitiveState.cpp:27-40`（`HasAnyMaterialSource`：裸指针 + `HasRecipeOverride()`）、解析链 `BuildResolvedRecipe`（1 处）、`GeometryData.cpp:43` | 「顺指针取组件」的路径要改成「句柄 → 行」 |
| 相邻层（**不属本批**） | `MaterialRuntimeRow` / `MaterialRuntimeSlot`（`support/MaterialRuntimeTable.h`：`l2w_buffer`/`l2w_index_buffer`/`mesh_draw_params_buffer`/`material_data_rows_buffer`/`indirect_*` 指针 + `refcount`/`generation` + 每实例字段）⇒ **B5**（P-5 已拍板指针归位） | — |

#### B4-1 分批草案

- **B4-i（死字段删除；无调用点 churn，可立即做）**：`MaterialDataAuthoringResource` 删 `buffer` / `element_capacity` / `byte_stride`
  ⇒ 组件侧 GPU 面只剩 `GlobalSSBOBinding`（类型 + 行号）+ `authored` ✓ 与 B3-iii「删三指针」同构。
  验收：全仓 grep 零残留 + 门/测试/示例全绿 + 反证（把字段加回去 ⇒ 契约红）。
- **B4-ii（材质行 CPU 读口 + 行内 revision）**：registry 加 `GetMaterialRow<T>(id)`（与 `GetMeshDrawParamsRow` / `GetGeometryAABB` 同形：
  读缓存、越界/未激活返回 `nullptr`），让渲染与解析路径读**行**而不是读组件里的副本；同批把「材质行号」的口径钉死（当前 = `ssbo_id` + `data_index`，行 0 = 哨兵）。
- **B4-iii（访问器句柄化；两条路，**需拍板**）**：
  - **路 A（组件槽句柄）**：不新建 arena；句柄 = `{组件槽号, generation}`（Entity 已有 `component_mask` / 组件槽），把 4 个裸指针重载改成
    `MaterialDataAccessor`（24 B）⇒ 立刻消灭裸指针，改动集中在 `Context` + 少量调用点。代价：句柄指向的是"组件槽"而不是 v2 §1 的 arena 行 ⇒ B8 时还要再迁一次。
  - **路 B（字面执行 v2 §1，推荐）**：新建 **(Global, MaterialData) 行 arena**（设备级，P-1 已拍板 Global 归设备级）：
    行内只放 POD（`GlobalSSBOBinding` + `generation` + 少量标量），`recipe_override` / `named_texture_resources`（重类型）落**旁表按行号索引**；
    `Entity` 槽里存 arena 行号（与 B8 的 `EntityGPU::row[16]` 对齐）。
  - **推荐**：B4-i / B4-ii 先做（无争议、机械、可独立验证）；B4-iii **走路 B**，但**分两步**：先建 arena + 行内 POD + 旁表（组件退化为"持有行号的薄壳"），再把调用点迁到句柄。

**B4-i 完成记录（2026-10-02）—— ✅ `MaterialData` 组件 GPU 面收敛为「类型 + 行号」（删三个每实体死字段）**

一、改动（2 文件）

- `inc/hgl/ecs/components/MaterialData.h`：`MaterialDataAuthoringResource` 删
  `DeviceBuffer *buffer` / `element_capacity` / `byte_stride`，只留基类 `GlobalSSBOBinding`
  （`ssbo_type` + `ssbo_id` + `data_index` = 类型 + 行号）+ `authored`；同时删已无用的
  `class DeviceBuffer;` 前置声明。
- `src/ecs/components/MaterialData.cpp`：`ResetDataAuthoringResource` 去掉对应三行复位。

**取证（为什么是死字段）**：① 三字段**外部读者 0**（唯一出现是 `ResetDataAuthoringResource` 的复位；
`SetDataResource` 只整体赋值、不读它们；`GetDataResource` 外部消费者 0 处）；
② 示例侧赋值 **0 处**（全部写法 = `named_struct = accessor.GetGlobalSSBOBinding(); SetDataResource(named_struct);`）；
③ 容量/步长的权威真源另有其人：`SSBOBufferRegistry::RegisterBuffer(…, element_capacity)` 用
「字节数 ÷ 容量」反算 stride（`src/SceneGraph/module/SSBOBufferRegistry.cpp:280-299`）。
⇒ 与 B3-iii「删几何三指针」同构：GPU 只按 **ID** 寻址，容量/步长由池配置反查。

二、验证（清 obj 全量重编）

- build rc=0 / 0 error（**零调用点改动** —— 编译通过本身即印证"死字段"）；
- `TestCSMIncrementalPass` 新增 **Test 31**（B4-i 契约 7 条：3 条"仍在" + 4 条禁复活）；rc=0；
- **反证**：把 `element_capacity` 加回组件头 ⇒ Test 31 **rc=31**（"材质组件又持每实体元素容量（真源在
  SSBOBufferRegistry）"）；逐字节还原（sha256 `7404a06b…`）⇒ rc=0；
- 契约首轮**抓到我自己**：注释里写了已删字段名 ⇒ 按"零残留"判红；改注释为不出现标识符后过
  （这条值得记住：禁复活 needle = 全名零残留，注释里提到也算残留）。

三、遗留（B4 后续）

- **B4-ii —— ❌ 取消（避免投机 API）**

侦察结论：`GetDataResource` 外部消费者 **0 处**（`ClearAuthoredResources` / `GetRecipeOverride` 同为 0）；
`GlobalSSBODataAccessor` 本身就**只有写口**（`Write` / `GetRowID` / `GetGlobalSSBOBinding`），没有读口——
正因为没有读需求。⇒ 给材质数据行加「CPU 读口」当前**无消费者**，加了就是投机 API，违背「有需求才建入口」
（对比：B3-iii 的 `GetMeshDrawParamsRow` 是因为批聚簇键**真的要用**才加）。将来若出现读需求（例如 GPU 剔除要读材质行），
随那个需求一起加。

**B4-iii-a 定稿（路 B 第一步）—— (Global, MaterialData) 行 arena**

一、归属（用户拍板口径）

**设备级**，由 `GraphicsContext` 拥有（与 `GlobalSSBOBufferRegistry` 同域）——用户原话「MaterialData 的数据本就是跨世界的」：
材质数据是**资源级、跨世界共享**的 ⇒ 行号全局可解析；与 P-1（Global 类型行 arena 归设备级）同一条规则。

二、与既有材质行池的分工（**不要混为一谈**）

| 池 | 角色 |
|---|---|
| 既有 `PBRSurface` / `EmissiveSurface` / `TransmissionSurface`（32/16/16 B，各 1024 行、设备级） | **材质数据内容行** —— shader 按 `MTL_ROW(…)` 读的字段行；是「被引用的资源行」 |
| 新增 `(Global, MaterialData)` arena（本批） | **组件行** —— 每份材质数据一行；行内只放「我引用哪一行内容 + 句柄世代 + 授权标记」 |

三、行布局（定稿，24 B）

```cpp
struct MaterialDataRow            // 与 BDA/std430 友好，24 B
{
    uint32_t surface_type;        // GlobalSSBOType 值（PBR / Emissive / Transmission）
    uint32_t surface_data_index;  // 内容行号（= 既有 binding.data_index）
    uint32_t surface_ssbo_id;     // 池 id（= 既有 binding.ssbo_id）
    uint32_t generation;          // 行世代（0 = 死；分配 +1、复用 +2）—— 句柄 revision 校验用
    uint32_t authored_flags;      // bit0 已授权数据行 / bit1 有配方覆盖 / bit2 有纹理绑定（余位预留）
    uint32_t reserved;            // 对齐；不解释为任何语义
};
static_assert(sizeof(MaterialDataRow) == 24);
```

- **留在组件**（authoring 输入，按行规则「选择器留组件」）：`recipe_override`（重类型）、
  `named_texture_resources`（map + 纹理/采样器指针）、`authored_generation`（CPU 侧单调计数器）。
- **搬进 arena**：`data_resource` 的 GPU 面（`GlobalSSBOBinding` + `authored`）⇒ 组件只持 `material_row_id`。

四、本批步骤与验收

1. `SSBOTypes.h` 加 `GlobalSSBOType::MaterialData`；`GlobalSSBOTypes.h` 的 name/version/stride 表随扩展
   （B3-i 加的 constexpr 全枚举覆盖断言会**强制**补齐——漏一项即 `C2338`）；
2. registry 配池（24 B/行、容量 1024 与材质内容池同口径、`reserve_rows = 1`、行 0 = 哨兵）+ `InitializePools`；
3. `MaterialData` 组件：`data_resource` 成员 → `material_row_id`；`SetDataResource` / `ClearAuthoredResources` /
   `ResetDataAuthoringResource` 改为**写 arena 行**（含 generation 前进）；`GetDataResource` 按「有消费者才加」原则
   与行内 `authored` 语义同步（零外部消费者 ⇒ 可随本批一并收敛）；
4. 验收：build 0 error、门 42 PASS/0 FAIL、10 测试 rc=0、示例 0 真 error + 对拍 0 + **像素复核（材质链路必须看屏幕）**、
   grep 零残留（禁复活 needle 收口）、反证至少一条（如停掉 arena 写入 ⇒ 行永远是哨兵 ⇒ 契约/门红）。

五、下一批 **B4-iii-b**（句柄化）

`MaterialDataAccessor`（24 B：`{material_row_id, generation}`，与 `BoundingBoxAccessor` 同构，含 `IsValid()` 世代校验）
替换 `GetMaterialData` / `GetOrCreateMaterialData` 的 4 个裸指针重载，并同批迁移全部调用点
（`PrimitiveState.cpp` 的 `HasAnyMaterialSource`、`GeometryData.cpp:43`、测试与示例）。
- **B4-iii**：访问器句柄化（`GetMaterialData` 4 个裸指针重载 → 24 B 值句柄，与 `BoundingBoxAccessor` 同构）；
  两条路见 B4-0 表 —— **路 A**（组件槽句柄）vs **路 B**（新建 (Global, MaterialData) 行 arena，
  重类型落旁表），推荐 **路 B 分两步**。**待用户拍板**。

**拍板点（用户）** —— ✅ **已拍板（2026-10-02，用户）：选路 B**。
**用户理由（原话）**：「MaterialData 的数据本就是跨世界的」⇒ 材质数据是**资源级、跨世界共享**的，
行 arena 落**设备级**（由 `GraphicsContext` 拥有）才是它的自然归属，行号全局可解析；与 P-1
（Global 类型行 arena 归设备级）同一条规则。⇒ B4-iii 按路 B **分两步**推进：
**B4-iii-a** 建 (Global, MaterialData) 行 arena（行内 POD：`GlobalSSBOBinding` + generation + authored；
重类型 `recipe_override` / `named_texture_resources` 落旁表按行号索引，组件退化为持有行号的薄壳）；
**B4-iii-b** `MaterialDataAccessor`（24 B：行号 + generation）替换 4 个裸指针重载 + 迁移全部调用点 + 契约/反证。

### B4-旧标题（保留供索引）—— MaterialData 句柄化（同 B3 范式）
**B4-iii-a 完成记录（2026-10-02）—— ✅ 材质数据落设备级 (Global, MaterialData) 组件行 arena**

一、改动（8 文件）：`SSBOTypes.h` 加类型 `MaterialData`；`MaterialDataRows.h` 加 `MaterialDataRow`（16 B：
surface_type / surface_ssbo_id / surface_data_index / authored_flags）+ 授权位常量 + static_assert；
`GlobalSSBOTypes.h` 名字表补齐（全枚举覆盖断言强制）；registry 配池（16 B×1024、`reserve_rows=1`
⇒ 行 0 = 「无材质数据」哨兵）+ 读口 `GetMaterialDataRow`；`MaterialData.h/.cpp`：`data_resource` →
`material_row_id`，写入口改写行、`BuildResolvedRecipe` 改读行，**析构不归还行**（定稿：句柄语义非租约），
删 `GetDataResource`（0 消费者）；测试 Test 31/32。

二、验证：清 obj 全量重编 RC1=RC2=0 / ERR=0；门 42 PASS/0 FAIL；10 测试 rc=0；示例 PBRSpheres 对拍=290、
CSM=531、ComputeFrustumCull、AutoMerge、PlaneGrid3D、IBLEnvironment 全部正常且 `组件行就位=1`；
反证：破坏组件取行 ⇒ Test 32 rc=32，逐字节还原 ⇒ 0；像素复核 PBRSpheres / IBLEnvironment 正常。

三、⚠ 复盘（本批最大的教训）：**混合 ABI 会伪造"我的改动必挂"**
`GlobalSSBOTypeCount` 6→7 改变了 `GlobalSSBOBufferRegistry::pools[GlobalSSBOTypeCount]` 的大小，而我只做
**增量构建** ⇒ 库/exe 成员偏移不一致 ⇒ 跨 TU 写越界 ⇒ 堆破坏。症状极具误导性：示例 rc=127、日志停在
初始化早期（其实是关闭期崩、丢失未 flush 输出）；cdb 断在 **0xC0000374**，命中点是
`GraphicsContext::Initialize → GeometryManager 构造` 里一次**普通分配**（堆破坏总在之后的分配才被检出）。
用它做的两次二分（"HEAD 正常 / 我的版本必挂"）因此**双双无效**。⇒ 铁律重申：**改结构大小或公共头，
必须清 obj 全量重编之后再下任何结论**；二分两侧都必须基于全量重编的产物。

**名单空洞安全（2026-10-02，用户实测崩溃驱动）—— ✅ 结构性修复**

- 崩溃栈（用户）：`TransformSystem::Update → TransformDataStorage::IsDirty(4294967295) → ValueArray::operator[](-1)`；
- 根因：名单空洞是设计（`Context.h`：「空洞记 INVALID_TRANSFORM_ID，注册优先复用空洞」）⇒ 遍历必须跳洞；
  `TransformSystem` 有**四处漏跳**（Update 的 movable/static 两条 + `SubmitTransformUpdates` 的
  `ShouldUpdateTransform(id)` 与 `ArmWrite(id)`）；`IsDirty` 还是唯一没做越界检查的行存储入口；
- 三层防护：① `IsDirty` 补越界/空洞守卫（返回 false + 一次性报错，不再越界）；② `ECSContext::ForEach*Transform`
  结构性遍历（内部跳洞，标注为唯一正确方式）+ `TransformSystem` 四处改走它；③ Test 15（运行期：空洞跳过后
  计数 == 活槽）+ Test 33（源码：结构性入口在位、`id` 形式裸遍历禁复发、入口守卫在位）；
- 验证：清 obj 全量重编；门 42/0；10 测试 rc=0；**用户崩的 PlaneGrid3D / IBLEnvironment rc=124 正常**、
  PBRSpheres 对拍=282；反证：恢复一处裸遍历 ⇒ Test 33 **rc=33**，逐字节还原 ⇒ 0；像素复核正常。

**元素数语义修正（2026-10-02，用户 RenderDoc + 像素实测驱动）—— ✅ 盒线框只画出 4 条直线（少 3 倍）**

- **现象（用户）**：`RenderBoundBox` / `LoadGeometry` 的包围盒线框画出来是 **4 条直线的 rectangle**，而正确是
  **12 条直线的空心 Cube**；每条线 2 三角形 6 顶点 ⇒ 应有 **72** 顶点输出，实测只有 **24**（= 4 条线）。
- **根因**：`ShaderLibrary/mesh/line_quad.glsl.tmpl` 里 `total_lines = draw_params.vertex_count >> 1u`。
  B3-ii 把旧 `MeshDrawParams::total_vertices`（**索引路径下存的是索引数**）拆成 `index_count`/`vertex_count`，
  而这条路径仍按旧语义直读 `vertex_count` ⇒ 对「8 顶点 / 24 索引」的盒几何：`8>>1 = 4` 条线（正确 `24>>1 = 12`）。
  `char_quad` 同理（索引几何会少画），一并改为按**元素数**取（与 `vertex_passthrough` 同口径）。
- **修法**：两个 mesh 着色器统一 `total_elements = is_indexed ? index_count : vertex_count`。
- **契约**：新增 **Test 34**（三处着色器一律按 `is_indexed` 择 `index_count`/`vertex_count`；
  禁复活 needle = 旧的 `vertex_count >> 1u` 与 `total_chars = draw_params.vertex_count`）；
  并把 **Test 29 里那条"line_quad 必须直读 vertex_count"的契约删掉** —— 它在 B3-ii 时把**错的口径**写成了契约，
  与 Test 34 的禁复活相冲突（教训：契约会把错误假设一起固化，改口径时必须同批更正契约）。
- **验证**：清 shader 缓存 + 重编；门 42 PASS/0 FAIL；10 测试 rc=0（含 Test 29 更正 + Test 34）；
  **像素复核**：`RenderBoundBox` 画满 12 条线的空心 Cube ✓、`LoadGeometry` 盒线框 ✓、
  `TextDraw`（char_quad 消费者）整页文本正常 ✓、`LineRender`（line_quad 消费者）彩色线段场正常 ✓；
  其余示例 rc=124/err=0；反证：把 line_quad 改回直读 `vertex_count` ⇒ **Test 34 rc=34**，逐字节还原 ⇒ 0。

### B5 —— MaterialRuntime 句柄化（含 **P-5** 的每实例指针字段归位；行宽定稿）
### B6 —— Texture（**当前无组件载体**：若仍无载体则本批不做，只在类型表里登记"已就位、无消费者"）
### B7 —— 相机终态收口（按 **P-6** 拍板执行；含 `CameraComponent` 的终态去向）
### B8 —— T11 `EntityGPU` 128 B + `.ulrescene` 直载（**延后**；触发条件见 0.1-6；本方案只保留位置）

---

## 3. 验证矩阵（每批照跑）

| 项 | 判据 |
|---|---|
| 构建 | 清 obj 全量重编；`error C…` / `error LNK…` / `error MSB…` 计数 = 0；仅允许 4 条既有 `C4715`（`src/InlineGeometry/{Arch,HollowGear,SolidGear,Teardrop}.cpp`） |
| 单元测试 | 10 个测试 rc=0（`TestCSMIncrementalPass` / `TestRenderStrategyTable` / `TestVisibilityStorage` / `TestComponentMask` / `TestMaterialRuntimeTable` / `TestMaterialVariantTable` / `TestBoundingBoxStorage` / `TestTransformFlatStorage` / `TestRenderItemDataStorage` / `TestBoundingVolumeCull`） |
| 门 | `ShaderResourceSchemaRegressionGate` rc=0（42 PASS / 0 FAIL） |
| 示例 | 抽跑（`PBRSpheres` / `CascadeShadowMap` / `GizmoUsage` + 本批相关者）8 秒：真 error 行 0、`策略判定对拍` 不一致 0 |
| 探针 | `ProbeTransformDiagnostics`：`sizeof` / 每行字节 / 堆分配；本批有结构改动时与 B0 基线逐项对比 |
| 残留 | 本批取代的旧符号/旧路径 `grep` 零残留（禁复活契约另计） |
| 行尾/BOM | 逐文件对 HEAD blob 比对（判行尾只用 Python 字节计数） |
| 反证 | 每批至少一条：破坏本批不变量 ⇒ 对应用例精确失败（rc 与文案），还原后全绿 |

---

## 4. 风险与默认选择

| # | 风险 | 默认选择 |
|---|---|---|
| R1 | **预算数值拍错** ⇒ 大场景直接超限 | 先按现有示例实测峰值 ×4 取整（正式值由 P-2 拍板）；超限报**一次性明确告警**并保留"预算没调够"的可判据文案 |
| R2 | 固定容量与"行复用 + 世代 +2"必须**同批**落地 | 否则旧句柄读到复用后的新行（静默错读）⇒ B1 内一次做完，配反证 |
| R3 | **P-1 选 B 后改 A = 二次迁移** | 因此 P-1 必须先拍板再开 B3 |
| R4 | 行字节瘦身动到 gizmo 的 fixed-pixel 路径 | 单独成批（B2）+ `GizmoUsage` 实跑，不与行空间冻结混批 |
| R5 | 本机 `build/` 是**跨改名点**配置（`build/example/Basic/` 与 `ApiValidation/` 并存），且从未构建过测试目标 | B0 先跑通验证集；必要时重配置 build |
| R6 | 陈旧 obj/半新半旧 TU 造成假绿/假红 | 结论一律以"清 obj 全量重编"为准；改结构头后按 `purge-stale-deps.sh` 或清整棵 `*.dir/Debug` |

---

## 5. 变更记录

| 日期 | 变更 |
|---|---|
| 2026-10-02 | 本方案建立（v1）：批次 B0–B8、待拍板 P-1…P-7、验证矩阵与风险默认。 |
| 2026-10-02 | **B0 完成**：本机跑通固定验证集（10 测试 rc=0 / 门 42 PASS 0 FAIL / build 0 error），探针行字节与 `sizeof` 基线钉下；行字节 189↔279 口径闭合。 |
| 2026-10-02 | **P-1…P-7 全部拍板**：P-1=A（Global arena 归设备级）／P-2 = static 4096 + dynamic 4096／P-3=A（类型固定位次）／P-6 = 相机参数并入既有世界相机行空间／P-4、P-5、P-7 按推荐。⇒ B1 可立即开工；B3 起按 P-1 的 A 方案落地设备级 arena。 |
| 2026-10-02 | **B1 侦察结论**：L2W 行号 = "名单排名 + 帧槽"的函数（非仅缓冲区重建）⇒ B1 切成 **B1a 预算与一次性分配** / **B1b 行号冻结（稳定槽位）**，见 §2 B1 节。 |
| 2026-10-02 | **B1a 完成**：声明式行预算（4096/4096）+ 预算期一次性分配、删 `StatTransform`/`EnsureCapacity`/`L2W recreated`、超限拒绝分配 + 一次性告警；全量 rc=0、10 测试 + 门全绿、示例 `L2W recreated`=0、反证 rc=13；新增 Test 11/12。`sizeof(TransformDataStorage)` 872→888；L2W 固定 2.36 MB/世界（待你定 dynamic 预算是否收小）。 |
| 2026-10-02 | **B1b 完成**：L2W 行号改为**稳定槽位**（下标即槽位 / 空洞复用 / 换边换段并计数）；全量 rc=0、10 测试 + 门全绿、6 示例 `L2W recreated`=0 与 `map failed`=0、反证 rc=15；新增 Test 13 + 7 条禁复活契约。发现全局 index 行表为死路（待 B1b-2 删除）。 |
| 2026-10-02 | **B1b-2 完成**：删除只写不读的全局 index 行表（`ECS:LocalToWorldIndex`，取证无消费者）；全量 rc=0、10 测试 + 门全绿、6 示例 0 异常/对拍 0；每帧省一次 vector 分配 + 一次 GPU 写 + 32 KB 设备缓冲。 |
| 2026-10-02 | **B2 完成**：行字节 **279 → 223 B/行**（children→CSR 8 B/行、fixed_pixel→稀疏 map）；全量 rc=0、10 测试 + 门全绿、6 示例 0 异常/对拍 0、反证 rc=16；新增 Test 14；删零调用者 `TransformAccessor::GetChildren()`。 |
| 2026-10-02 | **B3 设计第一版（已作废，见下条）**：侦察发现 `GeometryData`/`MaterialData` **都不是纯数据**（4 指针 / 重类型+指针），且全仓**无 asset 注册表**（示例自持栈 asset）⇒ 曾拟"行内只放 POD、资源引用走行号、运行期产物复用 slot 缓存、重类型落旁表"+ B3 拆 B3a 机制 / B3b 迁移 + 惰性指针→行号映射。**用户订正语义后作废**（不是"变纯数据"，是"把 GPU 可用部分抽成固定 ID 结构"）。 |
| 2026-10-02 | **B3 定稿（用户拍板）**：Geometry 的 GPU 可用部分 ID 化 —— 先不扩 8ID、`data_*` 上 GPU、`GeometryAABB` 独立池与 MeshDrawParams **同 ID 索引**（`ActivateAllRows` 结构保证 + 成对写入 + 负半径哨兵）；行 112→128 B；`GlobalAddresses` 加 `addr_geometry_aabb`（步长不变）；**范围订正**：不需要 arena 宿主/值句柄/173 调用点迁移。 |
| 2026-10-02 | **B3-i 完成**：`GlobalSSBOType::GeometryAABB` + 池（`ActivateAllRows` 静态行空间、`reserve 0`）+ `GlobalAddresses::addr_geometry_aabb`（48→56 B，步长不变）+ GLSL 同步 + 两条自动扩展覆盖性断言；build rc=0/0 error、门 42 PASS/0 FAIL、10 测试 rc=0、4 示例对拍 0（探针 `rows=16384 AABB 32 B/行`）；反证两处（容量不等 ⇒ 运行期不变量红；缺 name case ⇒ 编译期断言红）；**实测修掉 `ActivateAllRows` 与 `reserve_rows` 的机制冲突**。 |
| 2026-10-02 | **B3-ii 完成**：`MeshDrawParams` 112→**128 B**（拆 `total_vertices` + 补 `data_*`，X-macro 单源 + 三处 GLSL 读侧 + 三处写侧）；AABB 行收敛为唯一写入口 `RefreshGeometryAABBRow()`（实测修掉"Create 先于 SetBoundingVolumes"的顺序坑，PBRSpheres 10/21 行真值、半径 1.7321 与 CPU 同式逐位一致）；`bounding_radius` 改按需反算；门 42/0、10 测试 + Test 29（25 条契约含 6 条禁复活）、四示例 0 error/对拍 0；反证：抽掉刷新 ⇒ 真值 10→0。 |
| 2026-10-02 | **B3-iii 侦察（未动手）**：规模订正 —— 三指针是**批排序/聚簇主键**（`RenderItem::Compare` + `DrawBatch::Set` 的 4 个建批点 + 采集侧 4ID 填充/列武装 3 处），删它们等于改实例化合并判定；方向 = 行派生 POD 键（ICB 组数逐位等价）；待拍板：同 asset 的每实体 draw range 差异是否退出判定；本轮试改已还原，树保持 B3-ii 已验证状态。 |
| 2026-10-02 | **B3-iii 缺陷修正**：漏掉 `batch.gpu_driven_override` 逐项成簇判据 ⇒ GPU-Driven 批被合并成多实例簇、屏幕上只剩 1 个实例（界面上可见的错）；修复 + Test 30 契约 ④（去掉判据必 rc=30、逐字节还原必绿）+ 截图复核；教训：**改批/绘制语义必须看像素，数字全绿证明不了画得对**。 |
| 2026-10-02 | **B4-i 完成**：`MaterialData` 组件删 `buffer`/`element_capacity`/`byte_stride` 三个每实体死字段（外部读者 0、示例赋值 0 处，真源在 `SSBOBufferRegistry` 反算）⇒ GPU 面 = 类型 + 行号；零调用点改动、build rc=0、Test 31（7 契约）+ 反证（加回字段 ⇒ rc=31）。 |
| 2026-10-02 | **B4-ii 取消 + B4-iii-a 定稿**：材质行 CPU 读口**无消费者**（`GetDataResource` 0 处、`GlobalSSBODataAccessor` 本就只有写口）⇒ 不加投机 API；定稿 (Global, MaterialData) 组件行 arena（设备级、24 B/行：内容行引用 + generation + authored_flags），组件只留 authoring 输入 + `material_row_id`。 |
| 2026-10-02 | **B4-iii-a 完成**：材质数据落设备级 (Global, MaterialData) 组件行 arena（16 B/行、行 0 哨兵、组件只持行号、析构不归还行）；含 **混合 ABI 假象复盘**（改 `GlobalSSBOTypeCount` 后未清 obj ⇒ 堆破坏 ⇒ 两次二分无效）。 |
| 2026-10-02 | **名单空洞安全（结构性）**：用户实测 `IsDirty(0xFFFFFFFF)` 越界崩溃 ⇒ `ForEach*Transform` 结构性遍历 + 存储入口守卫 + Test 15/33；四处漏跳遍历全收；反证 rc=33；PlaneGrid3D / IBLEnvironment 复跑正常。 |
| 2026-10-02 | **元素数语义修正**：`line_quad`/`char_quad` 直读 `vertex_count` ⇒ 盒线框只画 4/12 条线（24 vs 72 顶点）；改为按 `is_indexed` 择 `index_count`/`vertex_count`；Test 34 钉口径 + **更正 Test 29 里被固化的错口径**；像素复核 RenderBoundBox/LoadGeometry/TextDraw/LineRender；反证 rc=34。 |
| 2026-10-02 | **B4 侦察 + 分批定稿**：材质行池**已就位**（PBR/Emissive/Transmission 各 1024 行、设备级）；组件 GPU 面已是「类型 + 行号」（`GlobalSSBOBinding`），但 `buffer`/`element_capacity`/`byte_stride` 是**死字段**（外部读者 0、示例赋值 0 处，容量/步长真源在 `SSBOBufferRegistry::RegisterBuffer` 反算）；访问器仍 4 个裸指针重载。分批：B4-i 删死字段 / B4-ii 材质行 CPU 读口 + revision / B4-iii 访问器句柄化（路 A 组件槽 vs **路 B arena**，待拍板）。 |
| 2026-10-02 | **B3 尾巴完成**：`ComputeFrustumCull` 自建 AABB 表退役、改读引擎 `GeometryAABB` 池（一号双表的二号表被 GPU 真正消费）；每帧双面对拍 PASS=8/FAIL=0、反证（地址 +32B ⇒ 7 FAIL、GPU visible 36→28）逐字节还原；遗留 `ComputeAsteroidBelt` 同类表（池行真值未验证）。 |
| 2026-10-02 | **B3-iii 完成**：批排序/聚簇/ICB/命令四处改读**行派生 POD 键**（`RenderItem::MeshDrawKey`），删 `GeometryData` 三指针 + 逐帧武装函数（12 文件）；build rc=0/0 error、门 42/0、10 测试 + Test 30（18 条契约/6 禁复活）、示例帧代理与基线一致；**过程教训：构建失败留下的半新半旧二进制伪装成 8.5× 性能回归**（A/B 两侧必须全新构建）。 |
