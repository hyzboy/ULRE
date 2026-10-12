# Meshlet 渲染路径实现方案（T2 详细设计 + T3..T5 接线面）

> **⚠ 本档已废止（2026-10-07 复核）**：2026-10-05 用户拍板**本轮不支持 meshlet**，
> `entity-rows-gpu-cull-plan.md` §8.7 **A1** 已把该欠账判为 **won't-do 并引擎侧整体删除**：
> 池行 3 个 `addr_meshlet*` 字段删（池行 128 → 104 B）、GLSL 镜像与门步长断言同步、
> `VKGeometry::SetMeshlets` / `MeshletDescriptor` / `MeshShaderVertexAdapter` / `PrimitiveBatchPipeline` 相关路径全删，
> **含 meshlet 表的资产加载即被拒绝**（`[ERROR] 本引擎不支持 meshlet 几何`）。
> 代码侧复核：全仓已无 `addr_meshlet` / `SetMeshlets`（只剩 `src/Tools/GLTFConvert` 工具侧的 `pure::Meshlet`）。
> ⇒ 本档 T2..T5 规划的落点**在当前代码里已不存在**，仅作历史记录；**若要重启 meshlet，须先翻案 §8.7 A1**。

> **版次说明**：首版（2026-10-05）。范围 = 让 meshlet 几何能被**正确渲染**（集群展开）、把组数改成**单源**、
> 最后**放开** GPU 剔除对 meshlet 批的排除。不覆盖 meshlet 级剔除 / LOD / task shader。
> 本档给的是**设计与接线面**（可直接照做），任务台账仍以 `entity-rows-gpu-cull-plan.md` §8.7/§8.8 为准。
> **关联文档**：
> - 真源（任务台账 / 侦察事实）：`doc/future/entity-rows-gpu-cull-plan.md` §8.7 A1 + §8.8（含 2026-10-05 晚补记的任务重排）
> - **被本档修正**：`doc/meshlet-geometry-pipeline-implementation.md`（其「已落地实现」结论只对数据层/调度层成立，渲染层不存在）
> - 外部实现对照：`doc/future/niagara-meshlet-drawcull-clustercull-comparison.md`
> - guard 口径表：`doc/render-flow-cull-sort-drawcall.md` §2.3

---

## 0. 设计输入

### 0.1 已定输入 —— 用户决策

| # | 决策 | 落点 |
|---|---|---|
| 1 | ② meshlet 是 A1 欠账，按 §8.8 补记重排的 T1..T5 推进；**T1 已完成**（资产链路 + `LS_*` 台架） | §8.8 补记 |
| 2 | **零兼容**：guard 放开 = 直接删代理条件与死字段，不留 fallback / 兼容开关 | 本档 §3.5 / §4.2 |
| 3 | 测试床两条：代码造 meshlet 几何（`SetMeshlets`）+ GLTFConvert 资产（`res/model/ChessMeshlet` vs `ChessClassic`，均不入库） | §5 T2 |
| 4 | 硬件上限由主程序从物理设备**实测传入**，生成侧不硬编码默认值 | 本档 §3.1 / R6 |
| 5 | **不碰 glTF 导入链**（未来重写）⇒ 本方案不改 meshlet 数据格式（不动 baseVertex / 引用压缩 / u8 三角形布局） | §1 非目标 |

### 0.2 已定输入 —— 引擎不变量（违反即编译失败或渲染崩，逐条带出处）

| # | 不变量 | 出处 |
|---|---|---|
| 1 | `MeshDrawParams` 行 = **128B**（头 9×4B + 4B 对齐填充 + 11×uint64），改字段只改 X 列表 `HGL_MESH_DRAW_PARAMS_FIELD_LIST` | `inc/hgl/graph/ShaderBufferSources.h`（`MeshDrawParamsLayoutValid` + `static_assert`） |
| 2 | GLSL 侧 `MeshDrawParamsHeadData` 是**手写镜像**（不由 X 列表生成）⇒ 加头字段必须同步 `_pad[92] → _pad[88]` | `ShaderLibrary/ecs/entity_gpu_cull.glsl` |
| 3 | 门 `E.mesh-draw-params-head-parity` 解析 X 列表**头字段**并与镜像逐字段比 + 断言步长 128 | `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` |
| 4 | 门钉死 GLSL 模块总数：`file_count != 70` / `expected 70 file modules`（两处 + 诊断串）⇒ 新增 **`.glsl`** 模块必须三处 +1〔2026-10-11：原值 71 —— 删除 `material/shadow_receiver_source.glsl`（ShadowMap 示例退场）后同步为 70；口径不变〕 | 同上 |
| 5 | `.glsl.tmpl` **不在**门扫描范围（只认 `.glsl`）⇒ 新模板不占模块数 | `src/ShaderGen/glsl_module/ShaderCodeModuleRegistry.cpp:19` |
| 6 | 顶点数据读取唯一接口 = `#define VertexIndexID (MeshVertexIndex)` + `LoadVertexData()`；mesh shader 无 `gl_VertexIndex` | `src/ShaderGen/meshgen/MeshShaderVertexAdapter.h` + `ShaderLibrary/vertex/s1_*.glsl` |
| 7 | mesh 实例号 = `draw_params.first_instance + gl_WorkGroupID.y` | `MeshShaderHeaderGen.h`（`EmitGlInstanceIndexMacro`） |
| 8 | mesh 输出拓扑与容量由 `layout(triangles, max_vertices=V, max_primitives=P) out` **编译期固定**，V/P 来自 `MeshModeDescriptor::resolve_topology` | `src/ShaderGen/meshgen/MeshTemplateEmitter.h` |
| 9 | 设备上限 `max_mesh_output_vertices` / `max_mesh_output_primitives` **已在物理设备 profile 里**（可实测下发） | `inc/hgl/mtl/contract/ShaderGenContract.h:40-41` + `ShaderGenPhysicalDeviceProfileJson.h:173-174` + `src/Vulkan/ShaderGenPhysicalDeviceProfileAdapter.cpp:73-74` |
| 10 | `mesh_shader_mode` 来自**材质定义**（TOML `[mesh_shader] mode`），经 `mtl_def_id` 间接进 recipe 哈希 ⇒ 模式变化天然产生不同的程序/批 | `inc/hgl/mtl/MaterialRecipe.h:351` + `MaterialDefinitionFile.cpp:1054` + `HashMaterialRecipe` |

### 0.3 待拍板项（每项：选项 + **推荐** + 影响）

**P1 模式落点** —— `MeshShaderMode::Meshlet`（推荐）vs `VertexShaderNodeConfig` 新增一档。
- **推荐 mode**。理由：`VertexShaderNodeConfig` 的五轴（input / position_mapping / orientation / scale / projection，`VertexShaderNodeConfig.h:64-71`）描述的是「**单个顶点**怎么取、怎么变换」——
  meshlet 路径**完全复用它**（一个 s1/s2/s3 模块都不换）。meshlet 改的是「**工作组 → 元素映射**与**图元装配**」，
  而这正是 `MeshShaderMode` 三个既有模式（VertexPassthrough / LineQuad / CharQuad）描述的东西。
- 影响：走 mode ⇒ 加枚举 + 描述符 + 模板 + varying 下标模型 + 门用例（见 §4.2）；
  走 node config ⇒ 上述之外还要动 `VertexNodeConfigResolver::GetHash`（**进 recipe 哈希**）与三个既有 2D 工厂函数的语义。
- ⚠ §8.8 补记原写字样是「`VertexShaderNodeConfig` 一档」，本档修正为 mode——见 §8 变更记录。

**P2 每工作组 meshlet 数** —— **1（推荐，首发）** vs 4/8（后续调优）。
- 1 的理由：与 meshlet 上限 64 顶点对齐（64 线程 ↔ ≤64 局部顶点，单趟即可）；实现最简；剔除粒度最细（将来做 meshlet cull 时直接受益）。
- 代价：dispatch 组数 = meshlet 总数（可达 10^4~10^5 量级），工作组固定开销乘以组数 ⇒ 见 R5。

**P3 组数字段名** —— **`mesh_group_count`（推荐）** vs `meshlet_count`。
- 推荐前者（§8.8 补记已用此名）。它是「本几何发出本批 X 组」的**唯一 X**，对 VP / meshlet 两种准入模式都成立。

**P4 三角形索引读取写法** —— `uint8_t data[]` 标量数组（推荐）vs 现状声明的 `u8vec3 data[]`。
- `u8vec3` 的 array stride 在 scalar layout 下**必须实测**（见 R4）；标量数组写法把歧义彻底消掉。
- 若走标量数组，索引式 = `data[(triangle_offset + t) * 3 + k]`。

**P5 `triangle_offset` 单位** —— **保持「三角形单位」**（现状：`om.triangle_offset / 3`，`CreateGeometry.cpp:238`）+ 加断言。
- 推荐保持：不动磁盘格式（符合决策 5）。GLSL 侧一律 `(triangle_offset + t) * 3`，并在 CPU 侧加注释 + 门用例钉死该换算。

---

## 1. 目标与非目标

### 目标（每条可自动判定）
1. **meshlet 几何能被正确渲染**（集群展开）：与 `--no-meshlet` 版资产**像素逐字节全等**（§5 T2 判据）。
2. **组数单源化**：X 组数只有一处产生（CPU 填池行），GPU 侧只读不算；公式不再有第二处。
3. **放开 GPU 剔除 guard** 对 meshlet 几何的排除，meshlet 批能被接管（§5 T4 判据）。

### 非目标（写下来防止架构走死）
1. **meshlet 级剔除**（cone / frustum / 遮挡）——独立后续方案（需要 MeshletBounds 通路，见 R2）。
2. **LOD / HLOD**——ULRE 目前无 LOD 概念（`inc/hgl/graph/geo` 与 GLTFConvert 全无 lod）。
3. **task shader**——首发只有 compute/无剔除路线（与 niagara 把 `taskShadingEnabled` 默认关的实测结论同向）。
4. **meshlet 数据格式改动**（baseVertex 化 / 16 位引用双打包 / cone 量化）——属 glTF 导入链重写线（决策 5）。
5. **~~④ 实例化欠账~~**（gather 的 Y=1）——**2026-10-05 won't-do**：查实无真实使用者，按零兼容整体删除（见 `entity-rows-gpu-cull-plan.md` §8.7 A2）。
6. **阴影侧的 meshlet 特化**——只保证同 mode、同 guard、同池行口径（§3.6）。

---

## 2. 现状（逐条实测，全部本轮复核）

| # | 层 | 状态 | 落点 |
|---|---|---|---|
| 1 | 数据层 | **已落地**：GLTFConvert 产 `meshlets` / `meshlet_vertices` / `meshlet_triangles` / `meshlet_bounds`；loader 装载四表；`SetMeshlets`；`Geometry` 持有 | `src/Tools/GLTFConvert/gltf/convert/CreateGeometry.cpp:157-230`、`example/Geometry/Shared/GeometryLoader.cpp:526-575`、`inc/hgl/graph/geo/VKGeometry.h:103-125` |
| 2 | 调度层 | **半落地**：`mesh_cmd->groupCountX = db.geometry->GetMeshletCount()` —— X 的语义**没有对应的着色器** | `src/ecs/support/PrimitiveBatchPipeline.cpp:791-799` |
| 3 | 渲染层 | **缺失**：全仓 `sbo_meshlets` 只出现在**声明处**（零消费者）；mesh 体只有 `vertex_passthrough` / `line_quad` / `char_quad` 三个模板 | `src/ShaderGen/meshgen/MeshShaderVertexAdapter.h:104-118`、`ShaderLibrary/mesh/` |
| 4 | 渲染层 | ⇒（2）+（3）= **活的错误**：meshlet 资产按 `meshlet_count` 发组，而 mesh shader 把 `gl_WorkGroupID.x` 当**顶点组号**（`group_base_vertex = x * 192`）。实测像素差 **138 px / 0.015%** | §8.8 补记；`ShaderLibrary/mesh/vertex_passthrough.glsl.tmpl:6` |
| 5 | 剔除层 | guard 用**代理信号** `addr_meshlets != 0` 排除 meshlet 几何（另用 `is_lines` 排除线模式）⇒ 排除的**真实原因**是「X 的语义随模式变」，不是「公式不同源」 | `src/ecs/support/PrimitiveBatchPipeline.cpp:1337-1339`、`inc/hgl/ecs/support/GPUCullSystem.h:154` |
| 6 | 结构一致性 | **缺**：CPU `MeshletDescriptor` = `{u32,u32,u8,u8,u16,u32}`(16B)，GLSL 声明 = `{uint,uint,counts,reserved}` —— 字段名/类型都不同，靠位兼容巧合；**无门用例对拍** | `inc/hgl/graph/geo/VKGeometry.h:17-26` vs `MeshShaderVertexAdapter.h:104` |
| 7 | bounds | `meshlet_bounds` **有装载无通路**：`MeshDrawParams` 尾只有 3 个 meshlet 地址，没有 bounds 地址 | `VKGeometry.cpp:194-202`、`ShaderBufferSources.h:34-36` |
| 8 | 组数 | **两处**：CPU `CalcMeshGroupCount` / `GetMeshletCount` vs GPU `ceil(element_count / mesh_group_size)`；且 GPU 侧的 `mesh_group_size` 是 push constant（CPU 下发 `kMeshVertexPassthroughMaxVertices = 192`） | `inc/hgl/ecs/support/PipelineMaterialRenderer.h:40-49` vs `ShaderLibrary/ecs/entity_gpu_cull.glsl:337-341` |

---

## 3. 设计

### 3.1 新增 `MeshShaderMode::Meshlet`（P1）

`inc/hgl/mtl/MeshShaderMode.h`：
- 枚举加 `Meshlet`
- `ParseMeshShaderMode` 加 `"Meshlet"`（未知值仍返回 false = 显式解析失败，不静默降级）
- `GetMeshModeVerticesPerInvocation` / `GetMeshModePrimitivesPerInvocation` 加分支

> ⚠ **本方案最容易埋雷的一处**：这两个函数对 VertexPassthrough 返回「每线程 3 顶点 / 1 图元」，是**invocations 的派生量**；
> 而 meshlet 的 V/P 是**集群数据的属性**（≤64 / ≤124），**不是** invocations 的函数。
> 处置：Meshlet 分支返回值只用于「非 0 占位」，并在函数注释与调用点写明
> 「**Meshlet 的容量必须走 `resolve_topology`，不得用本函数**」；若要更硬，把两者改名为
> `…PerInvocationForStridedModes`（会连带改 2 处调用点）。

`src/ShaderGen/meshgen/MeshModeDescriptor.h`：
- 新增 `ResolveMeshletTopology(invocations, out)`：`out.max_vertices = 64`、`out.max_primitives = 124`
  （常量出处见 §3.2；**如何与设备上限汇合见下**）
- 注册表加一行（mode / name / topology / defines / custom / stage1..3 / body）
- 新增 `EmitMeshletModeBody`（转发到新的 `MeshShaderModeMeshlet.h`）

**设备上限怎么汇合（决策 4 的落法）**：`resolve_topology` 的签名只有 `(max_invocations, out_capacity)`，拿不到设备 profile。
两条路：
1. 扩展签名（要同步改三个既有 resolver 的调用点）；
2. **在 `EmitMeshTemplateDocument` 出口统一 clamp**——拿到 `capacity` 后与 profile 的
   `max_mesh_output_vertices` / `max_mesh_output_primitives` 比对。
**推荐 2**：一处改动、不动既有签名；且判定策略明确——
- `profile_max >= 64/124` ⇒ 正常；
- 否则 ⇒ **硬失败**（不静默缩小：meshlet 数据是按 64/124 剖分的，缩小 V/P 会让超限集群的管线创建失败或输出被截断）。

### 3.2 meshlet 模式主体（模板内容）

新文件 `ShaderLibrary/mesh/meshlet.glsl.tmpl`（**不占门模块数**，不占 `#version`——由 `EmitMeshShaderVersion` 发）：

```glsl
    // 1 工作组 = 1 集群（X = 集群号）
    const MeshletDescriptor md = sbo_meshlets.data[gl_WorkGroupID.x];
    const uint vcount = uint(md.vertex_count);
    const uint tcount = uint(md.triangle_count);

    SetMeshOutputsEXT(vcount, tcount);
    if (vcount == 0u)
        return;

    // 顶点：局部顶点号 = 线程号（64 线程 ↔ ≤64 局部顶点 ⇒ 单趟；
    //       若设备 max_mesh_work_group_size 被 clamp 到 <64，则本循环自动退化为跨步）
    for (uint vid = gl_LocalInvocationIndex; vid < vcount; vid += {{local_size}}u)
    {
        MeshVertexIndex = sbo_meshlet_vertices.data[md.vertex_offset + vid];
        LoadVertexData();
{{varying_outputs}}
    }

    // 三角形：图元下标 = 循环变量（与顶点号**无** vid/3 关系，见 §3.3）
    for (uint t = gl_LocalInvocationIndex; t < tcount; t += {{local_size}}u)
    {
        const uint base = (md.triangle_offset + t) * 3u;
        gl_PrimitiveTriangleIndicesEXT[t] = uvec3(
            sbo_meshlet_triangles.data[base + 0u],
            sbo_meshlet_triangles.data[base + 1u],
            sbo_meshlet_triangles.data[base + 2u]);
    }
```

与 VertexPassthrough 的差别**只在「外层映射」与「图元装配」**：`LoadVertexData()`、s1/s2/s3 模块、varying 发射**全复用** ⇒ 顶点变换结果的一致性是**结构性保证**而非巧合。

**`MeshVertexIndex` 取值正确性论证（关键，必须留在文档里）**：
- `meshlet_vertices` 存的是**几何局部顶点号**——GLTFConvert 用 `pg.indicesData`（与它导出成 IBO 的是同一份索引）调 `meshopt_buildMeshlets`（`CreateGeometry.cpp:177-186`）。
- VertexPassthrough 的索引路径也把 IBO 值赋给 `MeshVertexIndex`（`sbo_vertex_index.data[index_base + …]`，`vertex_passthrough.glsl.tmpl:20-21`）。
- ⇒ 两条路读到的 `MeshVertexIndex` **同量纲**，`draw_params.vertex_base` 由 `LoadVertexData()` 统一再加 ⇒ **读同一个顶点**。

**集群/三角形索引的两种写法**（P4/P5）：
| 写法 | 顶点 | 三角形 | 备注 |
|---|---|---|---|
| 现状声明 | `sbo_meshlet_vertices.data[i]`（`uint[]`） | `sbo_meshlet_triangles.data[i]`（`u8vec3[]`，i = 三角形号） | `u8vec3` 的 stride **必须实测**（R4） |
| 推荐（消歧义） | 同左 | `uint8_t[]` 标量，`data[(triangle_offset + t)*3 + k]` | stride 恒 1，无歧义 |

### 3.3 varying 下标模型：**必须新增 `MeshVaryingIndexModel::Meshlet`**

现状（`src/ShaderGen/meshgen/MeshShaderVaryingGen.h`，`PerVertex` 分支）：

```glsl
fragDataIndexID[vid / 3u] = gl_InstanceIndex;
```

`vid / 3u` **只**在「三角形汤、每 3 连续顶点 1 图元」的 VertexPassthrough 下成立（`vertex_passthrough.glsl.tmpl:28-29`）。
meshlet 路径：顶点下标 = 局部顶点号、图元下标 = 三角形循环变量 ⇒ 必须新增模型：
- 顶点侧与 `PerVertex` 同（`fragXxx[vid]`）；
- **per-primitive 侧挪到三角形循环里**：`fragDataIndexID[t] = gl_InstanceIndex;`

漏了这处 ⇒ per-primitive 的材质索引错位 ⇒ **着色错但不崩**，属最难查的一类。门里要加 ABI 用例（§5 T2 判据 1）。

### 3.4 组数单源化（T3）

**字段**：X 列表头部加第 10 项 `mesh_group_count`（`uint`）。
它**正好吃掉那个 4B 对齐填充**：

```
头 9×4B = 36B + 4B 填充               ⇒ 11×uint64 从 offset 40 起，总 128B
头 10×4B = 40B（无需填充，40 已 8 对齐）⇒ 11×uint64 从 offset 40 起，总 128B   ← 行距不变
```

⇒ `MeshDrawParamsLayoutValid()` 的「头字段 9→10」循环上界改一处；「首地址 offset == 40」与 `sizeof == 128` 两条断言**值不变**。

**谁填**：唯一写 `addr_meshlets` 的地方就在 `VKGeometry.cpp` 的 `if (HasMeshlets())` 分支里，`meshlet_count` 成员近在咫尺 ⇒ 同一函数填。

**值**：
- meshlet 几何：`mesh_group_count = meshlet_count`（集群总数 = X）
- 非 meshlet：`mesh_group_count = ceil(element_count / kMeshVertexPassthroughMaxVertices)`（= 现行 GPU 公式的值，**逐值等价** ⇒ 非 meshlet 示例像素不可变）

**公式放哪（依赖方向）**：`CalcMeshGroupCount` 现在在 ECS 侧（`PipelineMaterialRenderer.h`），而 `VKGeometry.cpp` 在 SceneGraph 侧。
⇒ 把**顶点直通公式**下沉到 `inc/hgl/mtl/MeshShaderLimits.h`（与 `kMeshVertexPassthroughMaxVertices` 同处 = 单一真源），两侧共用。
**LineQuad 的 `/2` 分支不进池行**（guard 不准入线模式，见 §3.5）。

**GPU 侧**：cull 读 `row.mesh_group_count`，**删掉** `ceil(element_count / mesh_group_size)` 与 `mesh_group_size` push 字段及其 CPU 下发（`GPUCullSystem.cpp`）——零死字段。

**CPU 侧**：`PrimitiveBatchPipeline::WriteMeshDrawCommands` 的 `HasMeshlets() ? GetMeshletCount() : CalcMeshGroupCount(...)`
改为读**同一处**（参数行的语义值），⇒ CPU 账 / GPU 账 / 参数行三者同源。

### 3.5 guard 的改法：从「代理信号」到「显式模式」（T4）

现状 guard 用两个**代理**排除：`addr_meshlets != 0`（meshlet）、`is_lines`。

**结论**：排除的理由不是「公式不同源」，而是「**X 的语义随模式变**」。所以正解是按**批的实际 mesh 模式**准入：

| 模式 | 准入 | 理由 |
|---|---|---|
| `VertexPassthrough`（几何无 meshlet） | ✅ | 池行字段 = 顶点直通组数，语义唯一 |
| `Meshlet`（几何有 meshlet） | ✅ | 池行字段 = 集群总数，语义唯一 |
| `LineQuad` / `CharQuad` | ❌ | X 语义是「元素/2 与 192」/「字符数」，池行字段不承载 |

并断言**等价式**：

```
geometry->HasMeshlets()  ⇔  (batch.mode == MeshShaderMode::Meshlet)
```

**这是本方案的关键不变量**：它保证「一个几何一行参数 + 一个 `mesh_group_count`」永远只有**一个含义**——
同一几何不可能同时被两种准入模式绘制。违反 ⇒ fail-fast 报错（沿用既有 `override_reject_reported_` 的一次性报错风格，不静默退回 CPU 账）。

为什么必须显式化：如果继续只用 `addr_meshlets != 0` 当代理，一旦将来出现「同一几何既走 meshlet 又走非 meshlet 模式」的装配
（例如线框叠加、或同一网格两种材质），单个字段就会被两个含义争夺 ⇒ 静默画错。

### 3.6 阴影侧（T5）

caster 由 `ResolveShadowCasterProgram` 形成独立批，走**同一 guard / 同一池行**（§8.8 侦察事实 11）。
⇒ 本档只要求：**阴影用的 mesh 程序与 forward 同 mode**（否则等价式在阴影批被违反，fail-fast 会咬）。
判据：阴影图 dump 对拍（沿 CSM 既有口径）。

---

## 4. 接线面

### 4.1 新增文件

| 文件 | 作用 | 是否占门模块数（71） |
|---|---|---|
| `ShaderLibrary/mesh/meshlet.glsl.tmpl` | meshlet 模式主循环体（生成器拼装，**不自带 `#version`**） | **否**（门只扫 `.glsl`） |
| `src/ShaderGen/meshgen/MeshShaderModeMeshlet.h` | 该模式的发射器（对齐既有 `MeshShaderMode*.h` 命名与结构） | 否（C++） |

> 若将来把 meshlet 的展开/剔除拆成独立 `.glsl` 模块 ⇒ 每加一个模块，门的 `file_count` / `expected_count` / 诊断串**三处同步 +1**（见 §0.2 不变量 4）。

### 4.2 改动触点（逐处「文件 → 改动 → 为什么缺一不可」）

| # | 文件 | 改动 | 为什么缺一不可 |
|---|---|---|---|
| 1 | `inc/hgl/mtl/MeshShaderMode.h` | 枚举 + `Parse` + 两个 PerInvocation 分支 | 模式入口；漏 `Parse` ⇒ TOML 解析失败（硬失败，非静默降级） |
| 2 | `src/ShaderGen/meshgen/MeshModeDescriptor.h` | `ResolveMeshletTopology` + 注册表一行 + body 转发 | 无描述符 ⇒ `GetMeshModeDescriptor` 返回 null ⇒ 生成失败 |
| 3 | `src/ShaderGen/meshgen/MeshShaderModeMeshlet.h`（新） | 拼模板 + 注入 `{{local_size}}` / `{{varying_outputs}}` | 与 `EmitVertexPassthroughBody` 同构，保持模式插件化 |
| 4 | `src/ShaderGen/meshgen/MeshShaderVaryingGen.h` | `MeshVaryingIndexModel::Meshlet` + 分支 | 漏 ⇒ `fragDataIndexID` 用 `vid/3` ⇒ 材质索引错位（不崩，最难查） |
| 5 | `src/ShaderGen/meshgen/MeshTemplateEmitter.h` | 出口设备上限 clamp + Meshlet 模式的 `draw_params` 取行分支 | clamp 漏 ⇒ 设备输出上限 <64/124 时管线创建失败；取行分支漏 ⇒ 读错参数行 |
| 6 | `inc/hgl/graph/ShaderBufferSources.h` | X 列表头部加 `mesh_group_count` | 单一真源；不加 ⇒ GPU 侧没有 X 来源 |
| 7 | `src/SceneGraph/VKGeometry.cpp` | 填 `mesh_group_count`（meshlet 分支 + 通用分支） | 唯一写口；不填 = 0 ⇒ X 恒 0 ⇒ **画空**（0 VUID、门全绿） |
| 8 | `inc/hgl/graph/geo/VKGeometry.h` + `inc/hgl/mtl/MeshShaderLimits.h` | `GetMeshGroupCount()` 读取口 + 公式下沉 | CPU 账与池行同源的读取口；公式单源 |
| 9 | `ShaderLibrary/ecs/entity_gpu_cull.glsl` | 镜像加字段 + `_pad[92] → [88]`；icb 的 X 改读该字段 | 镜像不同步 ⇒ 字段错位读（**静默剔错**） |
| 10 | `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` | `E.mesh-draw-params-head-parity` 头字段 9→10 + 新增 `X.meshlet-mode-source` | 门是唯一能抓结构/ABI/源码漂移的地方 |
| 11 | `src/ecs/support/PrimitiveBatchPipeline.cpp` | 组数来源收敛到一处；guard 用显式模式 + 加等价式断言 | 两账同源；等价式是单字段语义的唯一保障 |
| 12 | `src/ecs/support/GPUCullSystem.cpp` | 删 `mesh_group_size` push 字段与下发 | 零死字段 |
| 13 | `example/Geometry/LoadScene/LoadScene.cpp` | 补 `LS_TX/TY/TZ`（取景覆盖） | T2 的「逐字节全等」判据需要把模型框满（§8.8 待查项） |
| 14 | `example/ApiValidation/`（或新示例） | 代码造 meshlet 几何的床（`SetMeshlets`，1~K 集群） | §8.8 T1 的床；用于最小可判定场景 |

---

## 5. 分阶段实施（每阶段：内容 / 判据 / 回滚 tag）

### T2 —— meshlet 模式与集群展开（首个能出图的里程碑）
**内容**：触点 1..5（§3.1/§3.2/§3.3）。
**判据（全自动）**：
1. **门**：新增 `X.meshlet-mode-source` —— 断言生成的 mesh GLSL 含 `max_vertices = 64` / `max_primitives = 124`、含 `sbo_meshlets`、且**不含** `max_group_verts`（证明未落 VertexPassthrough 分支）；并断言 per-primitive 写的是 `fragDataIndexID[<循环变量>]` 形态而非 `[vid / 3u]`（§3.3 的 ABI 用例）。门自身 0 FAIL。
2. **编译**：`glslangValidator -V -g`（或引擎 GLSLCompiler）零 error + `spirv-val` 通过；产物体积 / capability / `ArrayStride` 实测值记入本档附录。
3. **像素逐字节全等**：`LS_SCENE=…/ChessMeshlet/*.scene` 的 `LS_DUMP` vs `ChessClassic` 的 dump，**同相机同帧号**，差值 == 0（前置：触点 13 的取景补齐）。
4. 运行时 `[ERROR]` = 0、VUID = 0、设备不丢。
**反例控制（必做）**：把模板里 `triangle_offset` 故意 `+1` ⇒ 判据 3 必须变红（证明判据非空转）。
**回滚 tag**：`meshlet-T2`。

### T3 —— 组数单源化（触点 6..10、12）
**内容**：§3.4 全部。
**判据**：
1. `static_assert(MeshDrawParamsLayoutValid())` 通过、`sizeof(MeshDrawParams) == 128`（「加字段吃掉填充」的验证点）。
2. 门 `E.mesh-draw-params-head-parity` 头字段 **10/10** 对拍过。
3. **非 meshlet 示例像素逐字节不变**（字段值等价 ⇒ 行为等价）：用既有台架 `CV_*` / `AM_*` 三档（CPU 账 / 仅 dispatch / 接管）对比。
4. **组数逐值对照**：台架打印「CPU 账 X / 池行 X」，差值 == 0（meshlet 床 544 集群、非 meshlet 床 N 组）。
**落地纪律**：改共享池行结构 ⇒ **清整棵 Debug obj 树 + 清 shader 缓存**（§8.8 风险①：陈旧 TU 不会初始化新字段）。
**回滚 tag**：`meshlet-T3`。

### T4 —— 放开 guard（触点 11）
**内容**：§3.5（显式模式准入 + 等价式断言 + 删两个代理条件）。
**判据**：
1. meshlet 床：`[T3i] GPU cull draw override ENGAGED: batches=k/n`（正向日志，缺这条不得宣称接管成立）；
   `Parity mismatch=0`；`Σcount ≤ global`；`overflow=0`；值链满。
2. **像素三方逐字节全等**（CPU 账 / 仅 dispatch / 接管）。
3. 非 meshlet 示例回归不变；门 0 FAIL。
4. **负控**：临时把 `mesh_group_count` 写错 1 ⇒ 画面/对拍必须变红（证明「X 错会画漏/画重」这条判据有效）。
**回滚 tag**：`meshlet-T4`。

### T5 —— 阴影侧 + 文档收口
**内容**：阴影批同口径验证；§8.7 A1 关闭；`doc/render-flow-cull-sort-drawcall.md` §2.3 guard 表口径更新；本档与被修正档的指路牌核对。
**判据**：阴影图 dump 对拍（CSM 口径）+ VUID=0 + 全仓 grep 无残留旧口径（以 `addr_meshlets != 0` 作为 guard 理由的表述必须清零，改成「模式准入」）。

---

## 6. 验证矩阵

| # | 判据 | 载体（可运行 target） | 判定 |
|---|---|---|---|
| 1 | 结构/源码漂移 | `ShaderResourceSchemaRegressionGate.exe` | `E.mesh-draw-params-head-parity` 10/10；`X.meshlet-mode-source` 命中；0 FAIL |
| 2 | 生成 GLSL 可编译 | 门内 GLSLCompiler + `glslangValidator -V -g` + `spirv-val` | 零 error；产物指标记档 |
| 3 | 集群展开正确 | `LoadScene` + `LS_SCENE/ LS_TX/TY/TZ / LS_FRAMES / LS_DUMP` | meshlet 版 vs `--no-meshlet` 版像素差 == 0 |
| 4 | 组数单源 | 台架打印 + 门 | CPU X == 池行 X，逐值差 0 |
| 5 | 非 meshlet 无回归 | `CoverageVariants`(`CV_*`) / `AlphaModes`(`AM_*`) | 三档像素逐字节不变 |
| 6 | 接管成立 | `LoadScene` + `LS_CULL` / `LS_FORM_B` | `ENGAGED batches=k/n`、Parity 0、`Σcount ≤ global`、`overflow=0`、像素三方全等 |
| 7 | 判据非空转 | 反例控制（改错 1 处） | 对应判据变红 |
| 8 | 无残留旧口径 | 全仓 `grep` | 命中项均带「已弃用/已删除」标注 |

---

## 7. 风险 R1..R7（每条给默认选择）

| # | 风险 | 默认选择 |
|---|---|---|
| R1 | **单字段被两种模式争夺**（同一几何被两种准入模式绘制） | 等价式断言（§3.5），违反即 fail-fast；不引入第二字段 |
| R2 | **`GetMeshMode{Vertices,Primitives}PerInvocation` 语义污染**（它们对 strided 模式是「每线程量」，对 meshlet 不是） | 注释规则 + 调用点声明「Meshlet 走 resolve_topology」；更硬则改名（连带 2 处调用点） |
| R3 | **「逐字节全等」可能不成立** | 先补取景（触点 13），再定位差异范围：顺序无关的不透明 / `alpha_test` / dither 恒可硬判；**混合排不可**（绘制顺序相关，见 §8.7 B1） |
| R4 | **u8 三角形读取的 stride 歧义**（scalar layout 下 `u8vec3[]` 的 ArrayStride 规范上不是显然值） | T2 前先实测：`glslangValidator -V -g` + `spirv-dis` 读 `ArrayStride`；不为 3 ⇒ 换标量 `uint8_t[]` 写法（P4） |
| R5 | **组数暴增导致 GPU 时间倒退**（每集群一个工作组） | T2 一并测 GPU 时间（`PBR_PERF` 口径）；显著倒退则进 P2（1 组多集群）。**不牺牲正确性换性能** |
| R6 | **设备 `max_mesh_output_*` < 64/124** | 硬失败（资产按 64/124 剖分，静默缩小 = 输出被截断/管线创建失败） |
| R7 | **陈旧 TU / 陈旧 shader 缓存**导致新字段未初始化（「布局没变 ≠ 安全」） | 改池行后清整棵 Debug obj 树 + 清 shader 缓存 |

**额外待查（本轮暴露，非本线阻塞）**：
- ULRE 的设备 profile **没有收集** `maxMeshWorkGroupCount[3]` / `maxMeshWorkGroupTotalCount`（只有 `maxMeshWorkGroupSize*` 与 `maxMeshOutput*`，
  见 `inc/hgl/mtl/contract/ShaderGenContract.h`）。若要对 meshlet 组数做「按设备实际上限」的校验，需先补这两个 profile 字段。
- 间接绘制形式的组数上限**不是 65535**（见 §8 变更记录最后一行）；实现时按设备上报值校验即可。

---

## 8. 变更记录

| 旧说法 | 新结论 | 原因 |
|---|---|---|
| `meshlet-geometry-pipeline-implementation.md`：「Meshlet 双轨…**已落地实现 (Production Implementation)**」 | 只落地了**数据层**与**调度层**；**渲染层不存在**，且当前是**活的错误**（X 按 `meshlet_count` 发，shader 按顶点组解释） | 全仓 `sbo_meshlets` 零消费者；mesh 体只有 3 个模板；§8.8 补记实测像素差 138 px |
| 该档 §6.1：`MeshDrawParams` = **112B** | **128B** | `MeshDrawParamsLayoutValid()` 断言 128（头 9×4 + 4B 填充 + 11×8） |
| 该档 §6.2：GLSL `MeshletDescriptor { uint vertex_offset; uint triangle_offset; uint counts; uint reserved; }` | 应与 CPU **逐字段同名同类型**（`u32,u32,u8,u8,u16,u32`）；且需要门用例对拍 | 现声明靠位兼容巧合；ULRE 规约要求两端同源 + 门抓漂移（§0.2 不变量 3 的同族做法） |
| §8.7 A1：guard 排除 meshlet 的理由 = 「X 组数公式与 CPU 分支**不同源**」 | 真实原因是「**X 的语义随模式变**」（shader 把 X 当顶点组号）⇒ 正解是**模式准入**而非补公式 | §8.8 补记实测（meshlet 资产画错 138 px）；§3.5 |
| §8.8 补记 T2：「`VertexShaderNodeConfig` 一档」 | 落点应是 **`MeshShaderMode::Meshlet`** | node config 五轴是「单顶点取数与变换」，meshlet 路径**完全复用它**；meshlet 改的是「工作组→元素映射与图元装配」= mode 的职责（§0.3 P1） |
| 本方案初稿（同会话前文）曾推断「可见集群数驱动 X 会撞 **65535** 上限 ⇒ 需要 submit 阶段 + 2D tiling」 | **更正**：`VkDrawMeshTasksIndirectCommandEXT` 的三个 `groupCount` 分量**都是 uint32**（Vulkan SDK 1.4.357 `vulkan_core.h:27001-27005` 实测）；65535 只约束 **task shader 的 `EmitMeshTasksEXT`**（`maxTaskWorkGroupCount`）与 NV 的直接形式（`maxDrawMeshTasksCount`）。EXT 下 mesh 绘制的真实上限是设备上报的 `maxMeshWorkGroupCount[0]` / `maxMeshWorkGroupTotalCount` | 2D tiling 是 **task/EmitMeshTasks 路线**的产物；ULRE 首发**无 task shader**，走间接命令 ⇒ 首发不需要 submit 阶段 |
| 同上（对照档） | 同步更正 `doc/future/niagara-meshlet-drawcull-clustercull-comparison.md` §8.3 的同一处推断 | 结论翻转必须回扫同题档，只改一处会留下旧口径 |

---

## 附录 A：关键文件索引

| 关注点 | 文件 |
|---|---|
| 模式枚举 / 解析 | `inc/hgl/mtl/MeshShaderMode.h` |
| 模式描述符表 / 拓扑解析 | `src/ShaderGen/meshgen/MeshModeDescriptor.h` |
| mesh GLSL 骨架发射 | `src/ShaderGen/meshgen/MeshTemplateEmitter.h`、`MeshShaderHeaderGen.h` |
| 顶点适配 / meshlet BDA 声明 | `src/ShaderGen/meshgen/MeshShaderVertexAdapter.h` |
| varying 下标模型 | `src/ShaderGen/meshgen/MeshShaderVaryingGen.h` |
| 既有三个模式体 | `src/ShaderGen/meshgen/MeshShaderMode{VertexPassthrough,LineQuad,CharQuad}.h` + `ShaderLibrary/mesh/*.glsl.tmpl` |
| mesh 容量常量（单一真源） | `inc/hgl/mtl/MeshShaderLimits.h` |
| 池行单源 X 列表 | `inc/hgl/graph/ShaderBufferSources.h` |
| 池行写入（唯一写口） | `src/SceneGraph/VKGeometry.cpp`（`RegisterMeshDrawParams`） |
| meshlet 数据持有 | `inc/hgl/graph/geo/VKGeometry.h`（`MeshletDescriptor` / `MeshletBounds` / `SetMeshlets`） |
| 双轨调度 / guard | `src/ecs/support/PrimitiveBatchPipeline.cpp` |
| GPU 剔除（池行镜像 / 组数） | `ShaderLibrary/ecs/entity_gpu_cull.glsl` + `src/ecs/support/GPUCullSystem.cpp` |
| 门 | `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` |
| 资产产出 | `src/Tools/GLTFConvert/gltf/convert/CreateGeometry.cpp` + `export/ExportGeometry.cpp` |
| 资产装载 | `example/Geometry/Shared/GeometryLoader.cpp` |
| 台架 | `example/Geometry/LoadScene/LoadScene.cpp`（`LS_*`）、`example/Material/CoverageVariants`（`CV_*`）、`example/Material/AlphaModes`（`AM_*`） |

## 附录 B：公式速查

| 量 | 公式 |
|---|---|
| meshlet 集群数 | GLTFConvert 输出（`meshlets` 条目字节数 ÷ 16） |
| mesh 输出容量（meshlet） | `max_vertices = 64`、`max_primitives = 124`（**与 invocations 无关**） |
| mesh 输出容量（VertexPassthrough） | `max_vertices = invocations × 3`、`max_primitives = invocations` |
| 组数 X（统一） | meshlet 几何：`meshlet_count`；否则 `ceil(element_count / 192)`；`element_count = is_indexed ? index_count : vertex_count` |
| meshlet 局部顶点号 → 全局 | `sbo_meshlet_vertices.data[md.vertex_offset + lv]` |
| meshlet 三角形第 k 角 | `sbo_meshlet_triangles.data[(md.triangle_offset + t) * 3 + k]` |
| 参数行总长 | 头 10×4B(=40) + 11×uint64(=88) = **128B** |