# 材质数据全局池化 + 4-ID Draw Item 大计划（技术文档 · 详细版）

> 状态：**总纲**（不直接执行）。每个阶段由后续会话据此拆小计划、逐任务 build+run 验证。
> 目的：子会话拿到本文即可拆任务，无需重新 grep 侦察——所有结构定义、数据流、改动点、ID 语义均在此。
> 前置：vertex-bda-v2 主线（7 表 BDA + pc_root + 合批统一）已闭环；A6-2b 终态 Scene(0)/Bindless(1) 两集。

---

## 1. 目标与终态

把渲染数据从「每帧 CPU 重建 per-batch 行表 + 内嵌 BDA 地址」改造为「创建期全局池化 + 渲染期只给 4 个 ID」，并建立「BDA 传递方式 = 数据生命周期」的分层。

**一句话**：数据全局池化、基址分层传递（UBO / push constants）、每 primitive 一个 4-ID draw item，CPU 只写 ID。

---

## 2. 数据结构速查（本文所有改造的基座）

### 2.1 MeshDrawParams 行（88B）— `ShaderBufferSources.h:16-85`
X-macro 单源 `HGL_MESH_DRAW_PARAMS_FIELD_LIST`：
```
头部 6×4B（offset 0..20）：index_base, vertex_base, is_indexed,
                          total_vertices, char_height, first_instance
地址尾 8×uint64（offset 24 起 8B 步进）：addr_position, addr_uv, addr_ntb,
                          addr_color, addr_luminance, addr_transform_id,
                          addr_size, addr_index
```
- CPU struct `MeshDrawParams` + `kMeshDrawParamsFieldNames`/`GLSLTypes` + `MeshDrawParamsLayoutValid()`（offsetof 断言 + sizeof==88）。
- **注意 `addr_transform_id`**：这是「per-vertex transform id 流」的地址（顶点级变换 id），**不是** 4-ID 的实例级 TransformID。两者勿混（见 §4 ID 语义）。

### 2.2 材质字段行（每类型一行 = 字段 + 10 槽 bindless 句柄尾）— `MaterialDataRows.h`
| 行结构 | 大小 | 字段 |
|---|---|---|
| `PBRSurfaceRow` | 80B | base_color(16) + metallic/roughness/normal_scale/fresnel(各4) + `tex_tail`(40) + reserved(8) |
| `EmissiveSurfaceRow` | 64B | color(16) + tex_tail(40) + reserved(8) |
| `TextureRectArraySurfaceRow` | 64B | id[4](16) + tex_tail(40) + reserved(8) |
| `TransmissionSurfaceRow` | 48B | trans_color(4) + tex_tail(40) + reserved(4) |
| `TextureLayerRow` | 48B | **仅** tex_tail(40) + reserved(8)——纯纹理材质行，无字段 payload |

- `tex_tail` = `MaterialDataRowTexTail`（`uint32 tex[RANGE_SIZE]`，索引=TextureSlot 枚举值）。
- `static_assert(sizeof(Row)%16==0)` 全强制；`offsetof(PBRSurfaceRow,tex_tail)==32`。
- **关键语义**：字段行**内嵌** tex_tail——有字段数据的材质（PBR 等）其纹理句柄随材质数据行走；`TextureLayerRow` 只服务「无字段 payload 的纯纹理材质」（TextureQuad/TextDrawTest 等）。

### 2.3 RootAddresses（56B = 7×uint64）— `ShaderBufferSources.h:93-146`
X-macro `HGL_ROOT_ADDRESSES_FIELD_LIST`：
```
addr_mesh_draw_params, addr_l2w, addr_l2w_index, addr_mtl_data_addrs,
addr_text_char_info, addr_text_char_style, addr_text_char_instance
```
- 现全 push constant；阶段 5 拆分（§7.5）。

### 2.4 SSBO 域注册（SSBOBufferRegistry）— `SSBOBufferRegistry.h`
- `domain_map`: `(ssbo_type, ssbo_id)` → `SSBOBufferBinding{buffer, element_capacity, element_stride}`。
- `row_segments`: `ssbo_id` → `RowSegmentInfo{cpu_base, gpu_base, row_bytes}`（Arena+BDA 行段注册）。
- `next_ssbo_id` 自增计数器。
- `AllocateArrayAccessor<T>(ssbo_type, name, count)`（`:143-188`）：`AllocateSSBOId()` → `CreateArenaBuffer` → `Map`（HOST_COHERENT）→ `memset` → 包 `SSBOArrayAccessor<T>`（`OwnBuffer` + `ssbo_id`/`ssbo_type`）→ `row_segments.emplace(id, seg)`。
- `EnsureArrayAccessor<T>(address, ...)`（`:217-242`）：内部固定 ID 路径（`EnsureBuffer`）。
- `null_row_buffer`（64B 零填充空行，`GetNullRowAddress`）——地址表「无有效行」安全缺省。
- `SSBOArrayAccessor<T>`：`cpu_base/count/stride/ssbo_id/ssbo_type`，`OwnBuffer` 持 buffer 生命周期。

### 2.5 材质数据寻址（现状）
- `material_row_gpu = seg.gpu_base + data_index × row_bytes`（`RenderPrimitiveCollectSystem.cpp:929-935`）。
- 行表 `mtl_data_addrs`：每实例一行，存 `material_row_gpu`（8B BDA 地址）。
- FS：`MTL_ROW(i)` 宏 = `MaterialDataAddressesRef(pc_root.addr_mtl_data_addrs).values[i]`（`MaterialShaderEmitter.cpp:188-190`）；`fragDataIndexID` = draw item 序号（行表下标）。

---

## 3. 现状数据面（扫描结论，精确到符号）

### 3.1 材质专用 SSBO 类型 — `SSBOTypes.h:10-50`
- 引擎内部：`MeshDrawParams/LocalToWorld/LocalToWorldIndex/MaterialPrivateDataIndex`
- 材质字段：`PBRSurface/EmissiveSurface/TextureRectArraySurface/TransmissionSurface`
- 纹理句柄：`TextureLayer`；通配：`UserDefined`
- `IsMaterialSSBOType`（`:37-50`）混收字段 + TextureLayer + UserDefined——**阶段 1 拆枚举**。
- `DefaultMaterialPrivateDataSlot=0`、`MaxMaterialPrivateDataSlotsPerMaterial=1`、`MaterialPrivateDataIndexRowStride=1`（`:31-35`，已单槽）。
- `GetSSBOTypeStructStride`（`:95-96` 起）给每类型行距（如 TextureLayer=48）。

### 3.2 buffer 粒度与实例行 — `SSBOBufferRegistry.h:143-188`
- 每次 `AllocateArrayAccessor` 都 `AllocateSSBOId`（自增）+ 独立 `CreateArenaBuffer` → **同类型可多块 buffer**（`ssbo_id` 维度）。
- `shared_across_instances`（`RecipeSSBOAssetBinding:46`）：多实例共享同一 `data_index` 行。

### 3.3 每 primitive 的资产绑定链（多键残留，阶段 6 全删）
- `PrimitiveComponent.materialPrivateDataSlotResources`（vector，`.h:95`）+ `(slot_name,slot)` 键 Set/Get/Clear（`.cpp:359-434`）。
- `recipe.ssbo_assets`（vector，`MaterialRecipe.h:280`）+ `RecipeSSBOAssetBinding`（`:38-47`，slot_name/slot/ssbo_type/ssbo_id/data_index/use_data_index/shared_across_instances）。
- `MaterialComponent.resolved_ssbo_bindings`（vector，`.h:52`）+ `ResolvedSSBOBinding`（`:21-24`）。
- `RenderPrimitiveCollectSystem.cpp`：`ResolveRecipeSSBOBindingId`（`:75-87`）、物化 `:826-845`、`ssbo_assets` 遍历 `:873-900`、行地址计算 `:919-945`。
- 编译侧已单槽：`ResolveEffectiveMaterialPrivateData`（definition ⊕ manifest → 单 `SSBOType`，`MaterialShaderCompiler.cpp`）。

### 3.4 每帧重建的行表
- `MeshDrawParams` 行：`PrimitiveBatchPipeline::WriteMeshDrawCommands` 每帧按 draw 序写 88B 行。
- `mtl_data_addrs` 行：RPC 物化时每帧填行地址。

### 3.5 示例侧 accessor 创建（阶段 1 调用点）
- `PBRSpheres::InitMaterialDataSSBO()`（`:449`）：建 `material_data_ssbo_accessor`（PBRSurface）。
- `UpsertRecipeSSBOAssetBinding(sphere_recipe, name, accessor->GetSSBOBinding())`（`:452-454`）。
- 每球 `SetMaterialPrivateDataSlotResource({name,slot,type,ssbo_id,data_index=sphere_slot_rows[row][col]})`（`:495-501`）。
- 另两例同型：`AutoMergeMaterialInstance.cpp:175-181`、`BasicLitMeshes.cpp:405/443`。

---

## 4. ID 语义词典（4-ID 精确定义 + 现状对照）

| ID | 含义（终态） | 现状对应 | 备注 |
|---|---|---|---|
| `TransformID` | L2W 池行号（实例变换） | `l2w_index[gl_InstanceIndex]` 查行（经 `ResolveTransformID`） | 与 `MeshDrawParams.addr_transform_id`（顶点级 id 流）**不同**，勿混 |
| `GeometryID` | MeshDrawParams 池行号（几何+绘制参数） | ICB 命令序 = 行序，`gl_DrawID` 查行 | 阶段 2 池化后变持久 ID |
| `MaterialID` | mtl_data_addrs 池行号（材质参数寻址中间层） | `fragDataIndexID`（draw item 序号）查行表 | 该行存「类型池内 index」；**两级寻址**（见 §7.3） |
| `TextureID` | TextureLayerRow 池行号（纯纹理材质句柄） | TextureLayerRow 行 | 仅服务无字段 payload 材质；字段材质句柄在行内 tex_tail |

**寻址链（终态）**：
- 几何：`GeometryID` → MeshDrawParams 池行（`UBO.addr_mesh_draw_params` + id×88）→ 顶点流地址/段偏移。
- 材质：`MaterialID` → mtl_data_addrs 池行 → 该行存「类型池内 index」→ `UBO[type].base + index×row_bytes` → 字段。
- 纹理：`TextureID` → TextureLayerRow 池行 → tex_tail。
- 变换：`TransformID` → L2W 池（静态段/动态段）。

---

## 5. 终态数据模型

```
┌─ 全局 UBO（一次写，永久）─────────────────────────────┐
│  addr_mesh_draw_params      MeshDrawParams 池基址     │
│  addr_pbr_surface           PBRSurface 池基址         │
│  addr_emissive_surface      EmissiveSurface 池基址    │
│  addr_transmission_surface  TransmissionSurface 池基址│
│  addr_texture_rect_array    TextureRectArraySurface 池基址│
└────────────────────────────────────────────────────────┘

┌─ push constants（随材质/场景）────────────────────────┐
│  addr_mtl_data_addrs        每材质 mtl_data_addrs 池基址 │
│  addr_texture_layer_rows    每材质 TextureLayerRow 池基址│
│  addr_l2w                   per-world L2W 池基址（暂）   │
└────────────────────────────────────────────────────────┘

┌─ 4-ID draw item（SSBO，per primitive）───────────────┐
│  { TransformID, GeometryID, MaterialID, TextureID }  │
│  （现 ReBAR CPU 直写；未来 ComputeShader 写）         │
└────────────────────────────────────────────────────────┘
```

---

## 6. 阶段依赖图

```
阶段 1（类型池化）───┐
                      ├──→ 阶段 3（index 化 + TextureLayerRow 池化）──┐
阶段 2（MeshDrawParams 池化）─┘                                        ├──→ 阶段 4（4-ID）──→ 阶段 5（UBO 分层）──→ 阶段 6（删链）
                                                                       │
（阶段 1 与 2 可并行；3 依赖 1、2；4 依赖 1-3；5 依赖 4；6 依赖 1-5）
```

---

## 7. 分阶段改造（每段：目标 / 现状数据流 / 终态数据流 / 改动点 / 涉及文件 / 验收 / 待澄清）

### 7.1 阶段 1 — 材质字段类型池化

**目标**：每字段类型一个全局唯一大 SSBO；`SSBOBufferRegistry` 一次性创建（默认 1024 行，超限 fail-fast）；去 `ssbo_id` 维度。

**现状数据流**：
```
InitMaterialDataSSBO() → AllocateArrayAccessor<PBRSurfaceRow>(name,N)
  → AllocateSSBOId()=X → CreateArenaBuffer(80×N) → row_segments[X]={cpu_base,gpu_base,80}
每球 SetMaterialPrivateDataSlotResource({ssbo_id=X, data_index=row})
RPC 物化 → TryGetRowSegment(X) → row_gpu = gpu_base + row×80
```

**终态数据流**：
```
引擎启动 → SSBOBufferRegistry 一次性创建类型池 PBRSurface[1024]（base 固定）
  → row_pool[PBRSurface] = {cpu_base, gpu_base, row_bytes=80}
  → UBO.addr_pbr_surface = gpu_base（阶段 5 才挂 UBO，本阶段先存 registry）
每球：分配全局行号 data_index = AllocateRow(PBRSurface)
  → 只写 {data_index}（不再有 ssbo_id）
RPC 物化 → row_gpu = row_pool[type].gpu_base + data_index×80（类型即身份）
```

**改动点（函数/符号级）**：
1. `SSBOTypes.h`：拆出「材质字段专用枚举」（如 `MaterialFieldType` 或收窄 `SSBOType` 取值范围）；`IsMaterialSSBOType` 只含字段类型（`TextureLayer`/`UserDefined` 移出）。
2. `SSBOBufferRegistry`：
   - 去 `AllocateSSBOId`/`next_ssbo_id`；`row_segments`（`ssbo_id`→seg）改为 `row_pool`（`type`→seg）。
   - `AllocateArrayAccessor<T>` 改「按类型池」：首次建池（1024 行 `CreateArenaBuffer`）+ `Map` + 登记 `row_pool[type]`；后续同类型复用。
   - 新增 `AllocateRow(type) → uint32 index`（全局行分配器，1024 上限 fail-fast）与 `ReleaseRow(type, index)`（只释放行号，池 buffer 永不重建）。
   - `SSBOArrayAccessor<T>` 的 `ssbo_id` 语义改为「行号段」或直接废弃（改由行号表达）。
3. 示例侧（3 例）：`AllocateArrayAccessor` 调用点改为「分配行号」；`UpsertRecipeSSBOAssetBinding` 传行号而非 ssbo_id。

**涉及文件**：`SSBOTypes.h`、`SSBOBufferRegistry.h/.cpp`、`SSBOArrayAccessor.h`、`PBRSpheres.cpp`、`BasicLitMeshes.cpp`、`AutoMergeMaterialInstance.cpp`、`MaterialRecipe.h`（`RecipeSSBOAssetBinding.ssbo_id` → 行号语义）、`RenderPrimitiveCollectSystem.cpp`（`TryGetRowSegment` → `row_pool` 直查）。

**验收**：编译 + 全部示例视觉一致；同类型 accessor 共块（`domain_map`/`row_segments` 不再 per-id）；1024 超限触发 fail-fast。

**待澄清**：全局行分配器放 `SSBOBufferRegistry` 还是独立模块？`ReleaseRow` 是否本轮做（还是只增不减、后续再回收）？

---

### 7.2 阶段 2 — MeshDrawParams 池化

**目标**：所有 primitive 几何绘制参数创建期一次写入全局池，`GeometryID` 引用；池永久固定（预分配上限、只增不减、删除留空洞、基址不变）。

**现状数据流**：
```
PrimitiveBatchPipeline::WriteMeshDrawCommands：每帧按 draw 序写 MeshDrawParams 行（88B）
  + 写 ICB 间接命令（命令序=行序，gl_DrawID 查行）
```

**终态数据流**：
```
primitive 创建期（geometry 绑定）→ EnsureMeshDrawParams 写一次池行
  → GeometryID = 池行号（分配一次，永久）
渲染期 → 命令面只引用 GeometryID；gl_DrawID 不再直接=行序（改经 4-ID，见阶段 4）
```

**改动点**：
1. 建 `MeshDrawParamsPool`（或复用 `SSBOBufferRegistry` 池化机制）：`EnsureMeshDrawParams(geometry) → GeometryID`，创建期写 8 地址尾 + 段偏移。
2. `PrimitiveBatchPipeline::WriteMeshDrawCommands`：删每帧写行逻辑；`EnsureMeshDrawParams` 移创建期。
3. geometry 生命周期：`GeometryDataBuffer` 创建点（geometry 资产/运行时 geometry）处分配 `GeometryID`。
4. `MeshDrawParams` 行的 `addr_transform_id` 等顶点流地址：从「每帧填」改「geometry 创建期填」。

**涉及文件**：`PrimitiveBatchPipeline.cpp`（写命令/建池）、`ShaderBufferSources.h`（`MeshDrawParams` 结构不变，加池访问）、geometry 资产创建点（`GeometryDataBuffer`/`PrimitiveAsset` 相关）、`PipelineMaterialRenderer.cpp`（渲染期引用）。

**验收**：编译 + 全部示例；确认 MeshDrawParams 行只在创建期写一次（日志/断点）；几何不变时池内容零重写。

**待澄清**：`GeometryID` 分配粒度（per primitive asset vs per 运行时 geometry 实例）；池容量上限（与类型池同 1024？）；`addr_transform_id`（顶点级 id 流）是否也在本阶段一并池化。

---

### 7.3 阶段 3 — mtl_data_addrs 改 index + TextureLayerRow 池化

**目标**：行表存 **index**（4B，非 8B 地址）；`TextureLayerRow` 每材质池化（行号 `TextureID`）。核心是「地址 → 类型池基址 + index」两级寻址。

**现状数据流（行地址）**：
```
RPC 物化 :929-935 → material_row_gpu = seg.gpu_base + data_index×row_bytes
  → 填 mtl_data_addrs[实例] = material_row_gpu（8B 地址）
FS → MTL_ROW(fragDataIndexID) → values[i]（地址）→ 行
```

**终态数据流（行 index）**：
```
RPC 物化 → 填 mtl_data_addrs[实例] = data_index（4B 全局行号）
FS → UBO[type].base + values[i]×row_bytes → 行
```

**改动点**：
1. `mtl_data_addrs` 列：`uint64_t` → `uint32_t`（行号）；`MaterialDataAddressesRef` 元素类型改 `uint`（`MaterialShaderEmitter.cpp:390`）。
2. `MTL_ROW(i)` 宏（`:188-190`）改两级解引用：`(row_struct(UBO.addr_<type> + values[i]×row_bytes))`——需要类型→UBO 字段的映射（shader 侧按编译期固定类型名）。
3. `RenderPrimitiveCollectSystem.cpp:919-945`：行地址计算改「填 index」；`TryGetRowSegment` 查询改为 `row_pool[type]`（阶段 1 产物）。
4. `TextureLayerRow` 池化：Text 数据路径（`TextRenderPipeline.cpp:415-419` 每字体建 `texture_layer_buffer`）改为每材质池（行号 `TextureID`）。
5. `MaterialShaderCompiler`/`MaterialShaderEmitter`：FS 发射需新增「类型→UBO 字段名」的固定配表（如 `PBRSurface → addr_pbr_surface`），随材质行声明一起发射。

**涉及文件**：`MaterialShaderEmitter.cpp`（`:91-193` 行声明/MTL_ROW/:390 地址表）、`RenderPrimitiveCollectSystem.cpp`（物化）、`TextRenderPipeline.cpp`（texture_layer_buffer）、`MaterialShaderCompiler.cpp`（发射配表）。

**验收**：编译 + 数据槽示例（SimpleCube/BasicLitMeshes/PBRSpheres）+ 文本；grow 场景下 index 稳定（换池内容不改 ID）；`MTL_ROW` 两步解引用视觉正确。

**待澄清**：`MaterialID` 层级——「mtl_data_addrs 行存类型池 index」还是「直接存类型池行号」？按既定「mtl_data_addrs 存 index」，是**两级**（MaterialID → mtl_data_addrs 行 → 类型池 index），字段布局在此钉死；`TextureLayerRow` 与字段行内嵌 tex_tail 的分工边界（纯纹理材质 vs 字段材质）在此厘清。

---

### 7.4 阶段 4 — 4-ID draw item SSBO

**目标**：每 primitive 一个 `{ TransformID, GeometryID, MaterialID, TextureID }`（16B）紧凑结构，替代 88B 内嵌地址行。SSBO 形态，ReBAR CPU 直写（预留 ComputeShader 写）。

**现状**：`MeshDrawParams` 行（88B）内嵌 8 地址 + 段偏移，per-batch 每帧写，ICB 命令序=行序。

**终态**：
```
4-ID draw item 表（SSBO，per primitive，ReBAR 直写）
  { TransformID, GeometryID, MaterialID, TextureID }
ICB 命令面 → gl_DrawID → 4-ID 行 → 4 个 ID → 各池
```

**改动点**：
1. 新 draw item 结构（X-macro 单源 + offsetof 断言，仿 `MeshDrawParams`/`RootAddresses`）：`struct DrawItem4ID { uint32_t transform_id, geometry_id, material_id, texture_id; }`（16B，scalar 无 padding）。
2. 4-ID 表建 SSBO（ReBAR 直写，未来 CS 写）；`MeshDrawParams` 行瘦身为 `GeometryID` 引用（阶段 2 池化后，行内容已在池里，命令面只留 4-ID）。
3. `PipelineMaterialRenderer`/`PrimitiveBatchPipeline`：draw 提交改走 4-ID；ICB 多命令保留（合批不变）。
4. 换数据 = 换 ID（4-ID 表写 4B/字段，池内容不动）。

**涉及文件**：`ShaderBufferSources.h`（新 `DrawItem4ID` X-macro）、`PrimitiveBatchPipeline.cpp`（4-ID 表 + ICB）、`PipelineMaterialRenderer.cpp`（draw 引用）、`SSBOBufferRegistry`（4-ID 表 SSBO 创建，ReBAR）。

**验收**：编译 + 全部示例；合批（indirect multi-draw）命令数不变，命令面只引用 ID；换 ID 视觉正确。

**待澄清**：`TransformID` 与现有 `l2w_index`/`ResolveTransformID` 链的关系——是替代（4-ID 直接带实例 L2W 行号，删 l2w_index 表）还是并存（TransformID 指向 l2w_index 行）？4-ID 表是否也全局池化（per primitive 常驻）还是 per-batch 每帧填？

---

### 7.5 阶段 5 — UBO 分层

**目标**：类型池基址 + MeshDrawParams 基址挪入全局 UBO（一次写永久）；`mtl_data_addrs`/`TextureLayerRow`/`L2W` 基址保持 push constants。

**现状**：`RootAddresses`（7×uint64，`:93-146`）全 push；渲染路径每 MaterialBatch push 一次。

**终态**：
```
全局 UBO（Scene 集或独立全局集）：addr_mesh_draw_params + 每类型池基址
push constants：addr_mtl_data_addrs + addr_texture_layer_rows + addr_l2w
```

**改动点**：
1. 新全局 UBO 结构（X-macro + offsetof 断言）：`GlobalPoolAddresses { addr_mesh_draw_params, addr_pbr_surface, addr_emissive_surface, addr_transmission_surface, addr_texture_rect_array }`。
2. `RootAddresses` 收敛：去掉挪走的字段（`addr_mesh_draw_params`；类型池基址现不在 pc_root，是新增 UBO）。
3. `VKPipelineLayoutData.cpp`/`RootAddressPush.h`：UBO 绑定 + push 字段收缩；三渲染路径（PMR/Line/Text）push 调用更新。
4. GLSL 发射：`pc_root` 减字段 + 新 UBO 的 buffer_reference/block 声明（mesh+FS 双侧一致，同 X-macro）。

**涉及文件**：`ShaderBufferSources.h`（UBO 结构 + RootAddresses 收敛）、`VKPipelineLayoutData.cpp`、`RootAddressPush.h`、`MeshShaderHeaderGen.h`/`MaterialShaderEmitter.cpp`（pc_root + UBO 发射）、三渲染器 push 调用。

**验收**：编译 + 全部示例；永久地址（MeshDrawParams/类型池）确为一次写、不再随批 push；push 面只含随上下文变的地址。

**待澄清**：新 UBO 放 Scene 集（binding 4）还是独立全局集；L2W 是否本阶段也挪 UBO（per-world）。

---

### 7.6 阶段 6 — 删 asset 绑定链（多 SSBO 假能力）

**目标**：删除「类型即身份」后冗余的多键结构，收敛为单槽 + 类型直指。

**删除面（已扫描，全在此）**：
1. `PrimitiveComponent`：`materialPrivateDataSlotResources` vector（`.h:95`）、`(slot_name,slot)` 键 Set/Get/Clear（`.cpp:359-434`）、`MaterialPrivateDataSlotAuthoringResource` 键字段（`.h:66-79` 的 slot_name/slot）。
2. `MaterialRecipe`：`ssbo_assets` vector（`.h:280`）、`RecipeSSBOAssetBinding` 键字段（`:38-47`）、`Find/Resolve/UpsertRecipeSSBOAssetBinding` 按键辅助（`:345-400`）。
3. `MaterialComponent`：`resolved_ssbo_bindings` vector（`.h:52`）、`ResolvedSSBOBinding`（`:21-24`）、`Set/Find/ClearResolvedSSBOBindings`（`.h:95-102`）。
4. `RenderPrimitiveCollectSystem`：`ResolveRecipeSSBOBindingId`（`:75-87`）、物化循环（`:826-845`）、`ssbo_assets` 遍历（`:873-900`）。
5. `material_private_data_slot` 字段（schema 107 处 / 14 文件）：契约/反射/回归门收敛（`BindingTableBuilder` 键排序、`DescriptorContract:247-255` 的 `MakeRecipeSSBOId(slot)`）。
6. `MakeRecipeSSBOId(slot)` 槽参（`SSBOBufferRegistry.cpp:41`）。

**验收**：编译 + gate + 全仓 grep 零残留（`ssbo_id`/`material_private_data_slot`/多键 vector/slot 键全部消失）。

---

## 8. 已拍板决策清单

1. 一个 SSBOType 只会有一个 SSBO（仅材质字段专用类型）。
2. 材质字段专用类型**另立枚举**，与引擎内部类型彻底分开；`TextureLayerRow` 排除在全局字段池外（per-material 池）。
3. 任意材质可访问所有材质字段 SSBO（固定配表：`PBRSurface` → `pbr_surface` 等固定名，shader 同写）。
4. BDA 分层：**类型池基址 + MeshDrawParams 基址 → UBO**；**mtl_data_addrs / TextureLayerRow / L2W 基址 → push constants**（L2W 暂 push）。
5. MeshDrawParams 池**永久固定**（预分配上限、只增不减），故基址放 UBO 一次写。
6. 4-ID draw buffer 做 **SSBO**：现 ReBAR CPU 写，未来 ComputeShader 写（GPU-driven 预留）。
7. TransformDataBuffer 沿用现有 **static/dynamic 两段**（静态不动、动态每帧）。
8. 材质字段类型池 **SSBOBufferRegistry 一次性创建，默认 1024 行**，超限 fail-fast。

---

## 9. 风险与注意事项

1. **全局行分配器**：阶段 1 引入；行号生命周期 = 分配/释放 ID（池 buffer 永不重建、基址不变）。释放复用行号前确认无 in-flight 引用（ReBAR 直写无 stage 问题，CS 写阶段需 ring/围栏）。
2. **grow 语义**：类型池 1024 不扩容（fail-fast）；若未来扩容，grow 换 buffer → 基址变 → UBO 刷新（阶段 5 一次性成本，帧边界执行）。
3. **TextureLayerRow 归路**：per-material 池，非全局字段池；`TextureID` 只在材质内有效（与 `MaterialID` 层级一致）。字段材质句柄在行内 tex_tail，纯纹理材质才用 TextureLayerRow——阶段 3 厘清分工边界。
4. **MaterialID 层级**：两级（MaterialID → mtl_data_addrs 行 → 类型池 index），阶段 3 钉死字段布局。
5. **TransformID vs addr_transform_id**：前者实例级 L2W 行号，后者顶点级 id 流（`MeshDrawParams` 内）——阶段 4 澄清是否替代 l2w_index 链。
6. **ReBAR → CS 写**：阶段 4 的 4-ID SSBO 现 CPU 写，未来 CS 写需同步机制（不阻塞，仅预留形态）。
7. **L2W 归属**：暂 push；若迁 UBO（per-world），阶段 5 之外单独评估。

---

## 10. 现状关键文件清单（供拆小计划定位）

| 层 | 文件 | 关键符号/行 |
|---|---|---|
| 枚举 | `inc/hgl/graph/ssbo/SSBOTypes.h` | `SSBOType`(:10-28)、`IsMaterialSSBOType`(:37-50)、`DefaultMaterialPrivateDataSlot`(:31) |
| 行结构 | `inc/hgl/graph/ssbo/MaterialDataRows.h` | 4 字段行 + `TextureLayerRow`(:34-84)、tex_tail(:26-32) |
| 布局 | `inc/hgl/graph/ssbo/MaterialSSBOLayout.h` | `GetMaterialSSBOStructGLSL/RowName` |
| 池/域 | `inc/hgl/graph/module/SSBOBufferRegistry.h` | `AllocateArrayAccessor`(:143-188)、`AllocateSSBOId`(:120)、`row_segments`(:48)、`EnsureArrayAccessor`(:217) |
| 访问器 | `inc/hgl/vk/SSBOArrayAccessor.h` | `SSBOArrayAccessor<T>`(ssbo_id/ssbo_type/OwnBuffer) |
| MeshDrawParams | `inc/hgl/graph/ShaderBufferSources.h` | `HGL_MESH_DRAW_PARAMS_FIELD_LIST`(:16-30)、88B 断言(:84) |
| RootAddresses | 同上 | `HGL_ROOT_ADDRESSES_FIELD_LIST`(:93-100)、56B 断言(:145) |
| auth | `inc/hgl/ecs/components/PrimitiveComponent.h/.cpp` | `materialPrivateDataSlotResources`(:95)、`Set/Get/Clear`(.cpp:359-434) |
| recipe | `inc/hgl/mtl/MaterialRecipe.h` | `RecipeSSBOAssetBinding`(:38-47)、`ssbo_assets`(:280)、`Find/Resolve/Upsert`(:345-400) |
| 解析缓存 | `inc/hgl/ecs/components/MaterialComponent.h` | `ResolvedSSBOBinding`(:21-24)、`resolved_ssbo_bindings`(:52) |
| 收集/物化 | `src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp` | `ResolveRecipeSSBOBindingId`(:75-87)、物化(:826-845)、行地址(:919-945) |
| 批/命令 | `src/ecs/support/PrimitiveBatchPipeline.cpp` | `WriteMeshDrawCommands`（MeshDrawParams 行 + ICB 命令） |
| 渲染器 | `src/ecs/support/PipelineMaterialRenderer.cpp` | push 4 表、indirect flush |
| 发射 | `src/ShaderGen/compile/MaterialShaderEmitter.cpp` | `MTL_ROW`(:188-190)、`MaterialDataAddressesRef`(:390)、`BuildMaterialSSBODeclarations`(:91-193) |
| 变换 | `inc/hgl/ecs/support/TransformAssignmentBuffer.h` | static/dynamic 段(:34)、`EnsureCapacity`(:74)、`WriteStatic/DynamicDirtyIndices`(:77-81) |
| 文本数据 | `src/ecs/support/text/TextRenderPipeline.cpp` | `texture_layer_buffer`(:415-419) |
| 示例 | `example/Basic/PBRSpheres.cpp` | `InitMaterialDataSSBO`(:449)、`UpsertRecipeSSBOAssetBinding`(:452)、`SetMaterialPrivateDataSlotResource`(:501) |
| 示例 | `example/Basic/AutoMergeMaterialInstance.cpp` / `BasicLitMeshes.cpp` | 同型 accessor + slot 资源 |
