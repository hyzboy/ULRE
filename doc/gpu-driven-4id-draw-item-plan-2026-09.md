# 材质数据全局池化 + 4-ID Draw Item 大计划（技术文档 · 详细版）

> 状态：**总纲**（不直接执行）。每个阶段由后续会话据此拆小计划、逐任务 build+run 验证。
> 目的：子会话拿到本文即可拆任务，无需重新 grep 侦察——所有结构定义、数据流、改动点、ID 语义均在此。
> 前置：vertex-bda-v2 主线（7 表 BDA + pc_root + 合批统一）已闭环；A6-2b 终态 Scene(0)/Bindless(1) 两集。
> **修订（2026-09-11）**：**纹理引用重构已落地**（分支 `GPUDriven4ID_1_MaterialDataBuffer`，提交 `c7c413cbb..ecb89b71b`）——
> `TextureSlot`/`tex_tail`/`TextureLayerRow`/`TextureRectArraySurfaceRow` 已删除，纹理引用改由
> `MaterialTextureReferencePool`（per-definition）承载 `uvec2(descriptor_index, array_layer)`；逐实例材质表
> 由 8B 单地址变为 **16B 双地址 `MaterialInstanceAddresses`**；GLSL 新增 `MTL_TEX(i)`。§2.2/§2.4/§2.5/§3.1/
> §3.4/§4/§5/§6/§7.1/§7.3/§7.4/§7.5/§8/§9/§10 已同步，**新增 §2.6** 记录已落地的纹理引用 ABI。
> 数据槽绑定链（`ssbo_assets`/`ssbo_id`/`materialPrivateDataSlotResources`）与 `AllocateArrayAccessor` **未变**——阶段 1/2/6 的现状描述仍有效。

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

### 2.2 材质字段行（每类型一行 = **纯业务 payload**，已无纹理句柄）— `MaterialDataRows.h`
| 行结构 | 大小 | 字段 |
|---|---|---|
| `PBRSurfaceRow` | 32B | base_color(16) + metallic/roughness/normal_scale/fresnel(各4) |
| `EmissiveSurfaceRow` | 16B | color(16) |
| `TransmissionSurfaceRow` | 16B | trans_color(4) + reserved0[3](12) |

- `static_assert(sizeof(Row)%16==0)` 全强制（`MaterialDataRows.h:40-45`，逐行 sizeof 断言）。
- ⚠️ **2026-09-11 重构后行内不再有纹理句柄**：`MaterialDataRowTexTail`/`tex_tail`/`TextureLayerRow`/`TextureRectArraySurfaceRow` **已删除**；纹理引用改由 `MaterialTextureReferencePool` 独立承载（§2.6）。
- `MaterialSSBOLayout.h` 现只有 3 行（`GetMaterialSSBOStructName/RowName/StructGLSL` + `MaterialRowTypeTraits` 3 条）；`GetMaterialSSBORowTexTailOffset` 已随 tex_tail 删除。
- 行距表 `GetSSBOTypeStructStride`（`SSBOTypes.h:81-102`）已统一为**整行 sizeof**：PBR=32 / Emissive=16 / Transmission=16（旧「PBR=32 仅字段宽 vs TextureLayer=48 整行」的口径不一已消除）。

### 2.3 RootAddresses（56B = 7×uint64）— `ShaderBufferSources.h:105-112`
X-macro `HGL_ROOT_ADDRESSES_FIELD_LIST`：
```
addr_mesh_draw_params, addr_l2w, addr_l2w_index, addr_mtl_data_addrs,
addr_text_char_info, addr_text_char_style, addr_text_char_instance
```
- 现全 push constant；阶段 5 拆分（§7.5）。
- ⚠️ `addr_mtl_data_addrs` 指向的表**元素已从 8B 地址改为 16B `MaterialInstanceAddresses`**（§2.5/§2.6）。
- `MaterialInstanceAddresses` 就定义在同文件 `:89-97`（`{payload_address, texture_reference_address}` + 16B/offsetof 断言）。

### 2.4 SSBO 域注册（SSBOBufferRegistry）— `SSBOBufferRegistry.h`
- `domain_map`: `(ssbo_type, ssbo_id)` → `SSBOBufferBinding{buffer, element_capacity, element_stride}`（:33）。
- `row_segments`: `ssbo_id` → `RowSegmentInfo{cpu_base, gpu_base, row_bytes}`（Arena+BDA 行段注册，:51）。
- `next_ssbo_id` 自增计数器；`AllocateSSBOId()`（:167）。
- `AllocateArrayAccessor<T>(ssbo_type, name, count)`（:191）：`AllocateSSBOId()` → `CreateArenaBuffer` → `Map`（HOST_COHERENT）→ `memset` → 包 `SSBOArrayAccessor<T>`（`OwnBuffer` + `ssbo_id`/`ssbo_type`）→ `row_segments.emplace(id, seg)`（:232）。
- **新增（2026-09-11）**：注册表同时托管 **`ManagedArray<MaterialTextureReferencePool> material_texture_reference_pools`**（:52）+ `FindMaterialTextureReferencePool(...)`（:71/74）+ `GetMaterialTextureReferencePoolCount()`（:136）——纹理引用池按 material-definition 建/查（§2.6）。
- `null_row_buffer`（64B 零填充空行，`GetNullRowAddress`）——地址表「无有效行」安全缺省。
- `SSBOArrayAccessor<T>`：`cpu_base/count/stride/ssbo_id/ssbo_type`，`OwnBuffer` 持 buffer 生命周期。

### 2.5 材质数据寻址（现状，2026-09-11 后）

- 材质数据行地址 = `seg.gpu_base + data_index × row_bytes`（`RenderPrimitiveCollectSystem.cpp` 物化；`TryGetRowSegment(ssbo_id)` :951）。
- 逐实例表 **`mtl_data_addrs` 每实例一行 = `MaterialInstanceAddresses`（16B）**：
  - `payload_address` → 材质**字段行**（PBR/Emissive/Transmission 行）；FS 经 `MTL_ROW(i)` 解引用。
  - `texture_reference_address` → 该实例的**纹理引用行**（`MaterialTextureReferencePool` 行）；FS 经 `MTL_TEX(i)` 解引用。
- FS 宏（`MaterialShaderEmitter.cpp`）：
  - `MTL_ROW(i)`（:167-169）= `行结构(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].payload_address)`。
  - `MTL_TEX(i)`（:194-196）= `MaterialTextureReferencesRef(MaterialInstanceAddressesRef(...).values[(i)].texture_reference_address)`。
  - `fragDataIndexID` = draw item 序号（表下标）。
- 表本体写入点：`PrimitiveBatchPipeline.cpp:746/768/829`（每批 `MaterialInstanceAddresses` 行）；文本路径 `TextRenderPipeline.cpp:519-535`（单行表）。

### 2.6 纹理引用 ABI（**已落地**，2026-09-11）

> 旧的「每槽 uint32 bindless handle + Custom0 槽挪用存 array layer（tex_tail）」已被彻底替换。

- **`MaterialTextureReference`**（`MaterialRecipe.h:194-200`）= `{ uint32 descriptor_index; uint32 array_layer; }`（8B，`static_assert`）；非数组纹理 `array_layer` 恒 0。
- **`MaterialTextureReferenceLayout`**（`MaterialRecipe.h:218-230`）= `{ layout_hash, reference_count, row_stride(16B 对齐), max_configuration_count }`；由 `.material.toml` 的 `resources.textures` 声明顺序 + sampler/policy 推导（`BuildMaterialTextureReferenceLayout` :447-453）。
- **`MaterialTextureReferencePool`**（`inc/hgl/graph/module/MaterialTextureReferencePool.h` + `.cpp`）：**per material-definition** 一块 arena SSBO（`CreateArenaBuffer`，"MaterialTextureReferences:<definition_id>"）；行 = `uvec2(descriptor_index, array_layer)`；容量 = `resources.texture_configurations.max_count`（默认 `DefaultMaterialTextureConfigurationCapacity = 1024`，`MaterialRecipe.h:80`），**行 0 保留为零行**；分配 `Acquire()` / 写入 `Write()` / 归还 `Retire()`+`CollectRetired()`（自研 `free_rows` 栈 + `row_generations` 代际 + `MaterialTextureConfigurationRetirement`，`retire_epoch` 延迟 `MaterialTextureConfigurationRetireEpochDelay = 3` 才回收）。⚠️ **未用 `ActiveIDManager`**。
- **authoring 名称化**：`PrimitiveComponent::SetMaterialTextureResource(name, texture, sampler)`（`PrimitiveComponent.h:180`）与 `SetMaterialTextureLayer(name, layer)`——名称按当前 `MaterialDefinition` 的 TOML layout 校验；旧的 `TextureSlot::Custom0` layer 通道与 `SetMaterialTextureValue` 已删除。
- **GLSL**：`MTL_TEX(dataIndex).tex_<texture_name>`（`.x` = descriptor index，`.y` = array layer）；消费点已全量切换（`pbr_surface_source.glsl` / `texture_source.glsl` / `text_source_gpu.glsl` / `ntb_*_normalmap.glsl`）。
- **2D / Texture2DArray 统一**：都注册进 `texture2DArray[]`（普通 2D 资源 layer=0）→ 专用数组材质/provider（`lit_texture_array`、`texture_2d_array`、`pbr_texturearray_source`、`texture_array_source`、`ntb_texturearray_normalmap`）已删除。
- ⚠️ **未完成**：sampler/filter/wrap/swizzle/compare 的 TOML 配置只到「解析 + 布局 hash + 契约传递」，**运行时 sampler 创建未接入**。

---

## 3. 现状数据面（扫描结论，精确到符号）

### 3.1 材质专用 SSBO 类型 — `SSBOTypes.h:9-45`
- 引擎内部：`MeshDrawParams` / `LocalToWorld` / `LocalToWorldIndex` / `MaterialPrivateDataIndex`
- 材质字段：`PBRSurface` / `EmissiveSurface` / `TransmissionSurface`（**3 个**）
- 通用/无槽哨兵：`UserDefined`
- ⚠️ `TextureLayer` 与 `TextureRectArraySurface` **已在 2026-09-11 重构中从枚举删除**；`IsMaterialSSBOType`（:33-45）现只含 `PBRSurface/EmissiveSurface/TransmissionSurface/UserDefined`——**阶段 1 的「拆枚举」已部分完成**（TextureLayer 已移出，仅剩 UserDefined 哨兵未处理）。
- `DefaultMaterialPrivateDataSlot=0`、`MaxMaterialPrivateDataSlotsPerMaterial=1`、`MaterialPrivateDataIndexRowStride=1`（:27-31，已单槽）。
- `GetSSBOTypeStructStride`（:81-102）已整行口径（见 §2.2）。
- `SSBOIdNamespaceBit` / `MakeRecipeSSBOId` / `SSBOAddress` / `SSBOBinding` 仍在（:104-154）——阶段 6 的删除面未变。

### 3.2 buffer 粒度与实例行 — `SSBOBufferRegistry.h:191`
- 每次 `AllocateArrayAccessor` 都 `AllocateSSBOId`（自增）+ 独立 `CreateArenaBuffer` → **同类型可多块 buffer**（`ssbo_id` 维度）。
- `shared_across_instances`（`RecipeSSBOAssetBinding`）：多实例共享同一 `data_index` 行。

### 3.3 每 primitive 的资产绑定链（多键残留，阶段 6 全删）
- `PrimitiveComponent.materialPrivateDataSlotResources`（vector，`.h:96`）+ `(slot_name,slot)` 键 Set/Get/Clear。
- `recipe.ssbo_assets`（vector）+ `RecipeSSBOAssetBinding`（`MaterialRecipe.h:38-47`，slot_name/slot/ssbo_type/`ssbo_id`(:39)/data_index/use_data_index/shared_across_instances）。
- `MaterialComponent.resolved_ssbo_bindings`（vector）+ `ResolvedSSBOBinding`。
- `RenderPrimitiveCollectSystem.cpp`：`ResolveRecipeSSBOBindingId`、物化循环、`ssbo_assets` 遍历、行地址计算（`TryGetRowSegment` :951）。
- 编译侧已单槽：`ResolveEffectiveMaterialPrivateData`（definition ⊕ manifest → 单 `SSBOType`）。
- ⚠️ 注意区分：**纹理** authoring 已名称化（§2.6），但**数据槽**仍是上述多键链——阶段 6 删的是后者。

### 3.4 每帧重建的行表
- `MeshDrawParams` 行：`PrimitiveBatchPipeline::WriteMeshDrawCommands` 每帧按 draw 序写 88B 行。
- `MaterialInstanceAddresses` 行（16B）：`PrimitiveBatchPipeline.cpp:746/768/829` 每批填 `{payload_address, texture_reference_address}`；文本路径单行表 `TextRenderPipeline.cpp:519-535`。

### 3.5 示例侧 accessor 创建（阶段 1 调用点）
- `PBRSpheres::InitMaterialDataSSBO()`：建 `material_data_ssbo_accessor`（PBRSurface）+ `UpsertRecipeSSBOAssetBinding(sphere_recipe, name, accessor->GetSSBOBinding())` + 每球 `SetMaterialPrivateDataSlotResource({name,slot,type,ssbo_id,data_index=sphere_slot_rows[row][col]})`；纹理改为 `SetMaterialTextureResource("base_color"/"normal", …)` + `SetMaterialTextureLayer("base_color", row)`。
- 同型另有 `AutoMergeMaterialInstance.cpp`、`BasicLitMeshes.cpp`（多行共享块形态）。
- ⚠️ 示例行号随 2026-09-11 重构变动，拆小计划时以函数名/grep 定位。

---

## 4. ID 语义词典（4-ID 精确定义 + 现状对照）

| ID | 含义（终态） | 现状对应 | 备注 |
|---|---|---|---|
| `TransformID` | L2W 池行号（实例变换） | `l2w_index[gl_InstanceIndex]` 查行（经 `ResolveTransformID`） | 与 `MeshDrawParams.addr_transform_id`（顶点级 id 流）**不同**，勿混 |
| `GeometryID` | MeshDrawParams 池行号（几何+绘制参数） | ICB 命令序 = 行序，`gl_DrawID` 查行 | 阶段 2 池化后变持久 ID |
| `MaterialID` | 材质参数寻址中间层（终态：`mtl_data_addrs` 行号 / index） | `fragDataIndexID` 查 `MaterialInstanceAddresses` 行（16B） | 该行现存**双地址**（payload + texture ref）；阶段 3 才改 index |
| `TextureID` | 纹理引用行号 | **已落地**：`MaterialTextureReferencePool` 行（per-definition `uvec2` 行） | 池按 material-definition 建；普通 2D 资源 `array_layer=0` |

**寻址链（终态）**：
- 几何：`GeometryID` → MeshDrawParams 池行（`UBO.addr_mesh_draw_params` + id×88）→ 顶点流地址/段偏移。
- 材质：`MaterialID` → `mtl_data_addrs` 池行（终态存 index）→ `UBO[type].base + index×row_bytes` → 字段；该行另带纹理引用行地址（现形态）。
- 纹理：`TextureID` → `MaterialTextureReferencePool` 行 → `uvec2(descriptor_index, array_layer)` → bindless 采样。
- 变换：`TransformID` → L2W 池（静态段/动态段）。

---

## 5. 终态数据模型

```
┌─ 全局 UBO（一次写，永久）─────────────────────────────┐
│  addr_mesh_draw_params      MeshDrawParams 池基址     │
│  addr_pbr_surface           PBRSurface 池基址         │
│  addr_emissive_surface      EmissiveSurface 池基址    │
│  addr_transmission_surface  TransmissionSurface 池基址│
└────────────────────────────────────────────────────────┘

┌─ push constants（随材质/场景）────────────────────────┐
│  addr_mtl_data_addrs        每材质 mtl_data_addrs 池基址 │
│  addr_texture_references    每材质纹理引用池基址（MaterialTextureReferencePool）│
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
                      ├──→ 阶段 3（mtl_data_addrs index 化；纹理引用池化**已完成**）──┐
阶段 2（MeshDrawParams 池化）─┘                                                      ├──→ 阶段 4（4-ID）──→ 阶段 5（UBO 分层）──→ 阶段 6（删链）
                                                                                    │
（阶段 1 与 2 可并行；3 依赖 1、2；4 依赖 1-3；5 依赖 4；6 依赖 1-5）
```

---

## 7. 分阶段改造（每段：目标 / 现状数据流 / 终态数据流 / 改动点 / 涉及文件 / 验收 / 待澄清）

### 7.1 阶段 1 — 材质字段类型池化

**目标**：每字段类型一个全局唯一大 SSBO（现为 PBR/Emissive/Transmission 三型）；`SSBOBufferRegistry` 一次性创建（默认 1024 行，超限 fail-fast）；去 `ssbo_id` 维度。

**现状数据流**：
```
InitMaterialDataSSBO() → AllocateArrayAccessor<PBRSurfaceRow>(name,N)
  → AllocateSSBOId()=X → CreateArenaBuffer(32×N) → row_segments[X]={cpu_base,gpu_base,32}
每球 SetMaterialPrivateDataSlotResource({ssbo_id=X, data_index=row})
RPC 物化 → TryGetRowSegment(X) → row_gpu = gpu_base + row×32
```

**终态数据流**：
```
引擎启动 → SSBOBufferRegistry 一次性创建类型池 PBRSurface[1024]（base 固定）
  → row_pool[PBRSurface] = {cpu_base, gpu_base, row_bytes=32}
  → UBO.addr_pbr_surface = gpu_base（阶段 5 才挂 UBO，本阶段先存 registry）
每球：分配全局行号 data_index = AllocateRow(PBRSurface)
  → 只写 {data_index}（不再有 ssbo_id）
RPC 物化 → row_gpu = row_pool[type].gpu_base + data_index×32（类型即身份）
```

**改动点（函数/符号级）**：
1. `SSBOTypes.h` 拆枚举：**部分已完成**——`TextureLayer`/`TextureRectArraySurface` 已删、`IsMaterialSSBOType` 已只含 3 字段类型（+UserDefined）。剩余：是否另立 `MaterialFieldType` 与 `UserDefined` 哨兵处理（可与阶段 6 合并评估）。
2. `SSBOBufferRegistry`：
   - 去 `AllocateSSBOId`/`next_ssbo_id`；`row_segments`（`ssbo_id`→seg）改为 `row_pool`（`type`→seg）。
   - `AllocateArrayAccessor<T>` 改「按类型池」：首次建池（1024 行 `CreateArenaBuffer`）+ `Map` + 登记 `row_pool[type]`；后续同类型复用。
   - 新增 `AllocateRow(type) → uint32 index`（全局行分配器，1024 上限 fail-fast）与 `ReleaseRow(type, index)`（只释放行号，池 buffer 永不重建）——**`ActiveIDManager` 是候选实现**（纹理池当前用自研 free-list/retire，可对齐）。
   - `SSBOArrayAccessor<T>` 的 `ssbo_id` 语义改为「行号段」或直接废弃（改由行号表达）。
3. 示例侧：`AllocateArrayAccessor` 调用点改为「分配行号」；`UpsertRecipeSSBOAssetBinding` 传行号而非 ssbo_id。（注意：纹理 authoring 已名称化，不受本阶段影响。）

**涉及文件**：`SSBOTypes.h`、`SSBOBufferRegistry.h/.cpp`、`SSBOArrayAccessor.h`、示例 accessor 创建点、`MaterialRecipe.h`（`RecipeSSBOAssetBinding.ssbo_id` → 行号语义）、`RenderPrimitiveCollectSystem.cpp`（`TryGetRowSegment` → `row_pool` 直查）。

**验收**：编译 + 全部示例视觉一致；同类型 accessor 共块（`domain_map`/`row_segments` 不再 per-id）；1024 超限触发 fail-fast。

**待澄清**：全局行分配器放 `SSBOBufferRegistry` 还是独立模块？是否复用 `ActiveIDManager`（与纹理池统一回收机制）？`ReleaseRow` 是否本轮做（还是只增不减、后续再回收）？

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

### 7.3 阶段 3 — `mtl_data_addrs` index 化（**纹理引用池化已完成**）

**目标**：`mtl_data_addrs` 行由「BDA 地址」改为 **index**（4B，非地址）；核心是「地址 → 类型池基址 + index」两级寻址。

> ⚠️ **2026-09-11 变更**：原「TextureLayerRow 池化」已随纹理引用重构**完成**（以 `MaterialTextureReferencePool` per-definition 形态，见 §2.6）；且 `mtl_data_addrs` 行已不再是 8B 单地址，而是 **16B `MaterialInstanceAddresses{payload_address, texture_reference_address}`**——本阶段的起点因此变了。

**现状数据流（行地址，16B 双地址）**：
```
RPC 物化 → material_row_gpu = seg.gpu_base + data_index×row_bytes
  → 填 mtl_data_addrs[实例] = { payload_address = material_row_gpu,
                                texture_reference_address = 纹理引用行地址 }
FS → MTL_ROW(i) = 行结构(values[i].payload_address)
     MTL_TEX(i) = 纹理引用行(values[i].texture_reference_address)
```

**终态数据流（行 index）**：
```
RPC 物化 → 填 mtl_data_addrs[实例] = { payload_index, texture_reference_index }（4B×2）
FS → UBO[type].base + index×row_bytes → 行
```

**改动点**：
1. `MaterialInstanceAddresses` 的两个 `uint64_t` 地址 → `uint32_t` index；`MaterialInstanceAddressesRef` / `MaterialTextureReferencesRef` 元素类型随之改。
2. `MTL_ROW(i)` / `MTL_TEX(i)` 宏（`MaterialShaderEmitter.cpp:167-169` / `:194-196`）改两级解引用：类型池基址（UBO）+ index×row_bytes。
3. 物化（`RenderPrimitiveCollectSystem.cpp` :951/:958 及纹理引用段 :985-1224）：由「算地址」改「填 index」；`TryGetRowSegment` 查询改 `row_pool[type]`（阶段 1 产物）。
4. FS 发射需新增「类型→UBO 字段名」固定配表（如 `PBRSurface → addr_pbr_surface`），随材质行声明一起发射。
5. 纹理侧：`MaterialTextureReferencePool` 行号即 `TextureID`，其基址（per-definition）进 UBO/push（阶段 5 定）。

**涉及文件**：`ShaderBufferSources.h`（`MaterialInstanceAddresses` 字段类型）、`MaterialShaderEmitter.cpp`（行声明/MTL_ROW/MTL_TEX）、`RenderPrimitiveCollectSystem.cpp`（物化）、`PrimitiveBatchPipeline.cpp`（行表填充）、`TextRenderPipeline.cpp`（单行表）、`MaterialShaderCompiler.cpp`（发射配表）。

**验收**：编译 + 数据槽示例（SimpleCube/BasicLitMeshes/PBRSpheres）+ 文本 + Texture2DArray layer；grow 场景下 index 稳定（换池内容不改 ID）；`MTL_ROW`/`MTL_TEX` 两步解引用视觉正确。

**待澄清**：`MaterialID` 层级——`mtl_data_addrs` 行存 index 后仍是**两级**（MaterialID → 行 → 类型池 index）；`payload_index` 与 `texture_reference_index` 是否统一进同一 index 空间需在此钉死。

---

### 7.4 阶段 4 — 4-ID draw item SSBO

**目标**：每 primitive 一个 `{ TransformID, GeometryID, MaterialID, TextureID }`（16B）紧凑结构，替代 88B 内嵌地址行。SSBO 形态，ReBAR CPU 直写（预留 ComputeShader 写）。

**现状**：`MeshDrawParams` 行（88B）内嵌 8 地址 + 段偏移，per-batch 每帧写，ICB 命令序=行序；材质侧已是 16B 双地址（§2.6）。

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

**待澄清**：`TransformID` 与现有 `l2w_index`/`ResolveTransformID` 链的关系——是替代（4-ID 直接带实例 L2W 行号，删 l2w_index 表）还是并存（TransformID 指向 l2w_index 行）？4-ID 表是否也全局池化（per primitive 常驻）还是 per-batch 每帧填？`TextureID` 与 `MaterialTextureReferencePool`（per-definition）的对应关系（同一行 = payload+纹理引用？）在此钉死。

---

### 7.5 阶段 5 — UBO 分层

**目标**：类型池基址 + MeshDrawParams 基址挪入全局 UBO（一次写永久）；`mtl_data_addrs` / 纹理引用池 / `L2W` 基址保持 push constants。

**现状**：`RootAddresses`（7×uint64，`:105-112`）全 push；渲染路径每 MaterialBatch push 一次。

**终态**：
```
全局 UBO（Scene 集或独立全局集）：addr_mesh_draw_params + 每类型池基址
push constants：addr_mtl_data_addrs + addr_texture_references + addr_l2w
```

**改动点**：
1. 新全局 UBO 结构（X-macro + offsetof 断言）：`GlobalPoolAddresses { addr_mesh_draw_params, addr_pbr_surface, addr_emissive_surface, addr_transmission_surface }`。
2. `RootAddresses` 收敛：去掉挪走的字段（`addr_mesh_draw_params`；类型池基址现不在 pc_root，是新增 UBO）。
3. `VKPipelineLayoutData.cpp`/`RootAddressPush.h`：UBO 绑定 + push 字段收缩；三渲染路径（PMR/Line/Text）push 调用更新。
4. GLSL 发射：`pc_root` 减字段 + 新 UBO 的 buffer_reference/block 声明（mesh+FS 双侧一致，同 X-macro）。

**涉及文件**：`ShaderBufferSources.h`（UBO 结构 + RootAddresses 收敛）、`VKPipelineLayoutData.cpp`、`RootAddressPush.h`、`MeshShaderHeaderGen.h`/`MaterialShaderEmitter.cpp`（pc_root + UBO 发射）、三渲染器 push 调用。

**验收**：编译 + 全部示例；永久地址（MeshDrawParams/类型池）确为一次写、不再随批 push；push 面只含随上下文变的地址。

**待澄清**：新 UBO 放 Scene 集（binding 4）还是独立全局集；L2W 是否本阶段也挪 UBO（per-world）；纹理引用池基址（per-definition）是否也进 UBO（否则按 definition 切换 push）。

---

### 7.6 阶段 6 — 删 asset 绑定链（多 SSBO 假能力）

**目标**：删除「类型即身份」后冗余的多键结构，收敛为单槽 + 类型直指。

**删除面（已扫描，全在此；纹理 authoring 已名称化、不在本阶段范围）**：
1. `PrimitiveComponent`：`materialPrivateDataSlotResources` vector（`.h:96`）、`(slot_name,slot)` 键 Set/Get/Clear、`MaterialPrivateDataSlotAuthoringResource` 键字段（slot_name/slot/`ssbo_id`(:71)）。
2. `MaterialRecipe`：`ssbo_assets` vector、`RecipeSSBOAssetBinding` 键字段（`:38-47`）、`Find/Resolve/UpsertRecipeSSBOAssetBinding` 按键辅助。
3. `MaterialComponent`：`resolved_ssbo_bindings` vector、`ResolvedSSBOBinding`、`Set/Find/ClearResolvedSSBOBindings`。
4. `RenderPrimitiveCollectSystem`：`ResolveRecipeSSBOBindingId`、物化循环、`ssbo_assets` 遍历。
5. `material_private_data_slot` 字段（schema 107 处 / 14 文件）：契约/反射/回归门收敛（`BindingTableBuilder` 键排序、`DescriptorContract` 的 `MakeRecipeSSBOId(slot)`）。
6. `MakeRecipeSSBOId(slot)` 槽参（`SSBOTypes.h:107`）。

**验收**：编译 + gate + 全仓 grep 零残留（`ssbo_id`/`material_private_data_slot`/多键 vector/slot 键全部消失）。

---

## 8. 已拍板决策清单

1. 一个 SSBOType 只会有一个 SSBO（仅材质字段专用类型）。
2. 材质字段专用类型**另立枚举**，与引擎内部类型彻底分开；纹理引用**已独立成 `MaterialTextureReferencePool`（per-definition）**，不再与字段行混排（原「TextureLayerRow 排除在全局字段池外」已由重构兑现）。
3. 任意材质可访问所有材质字段 SSBO（固定配表：`PBRSurface` → `pbr_surface` 等固定名，shader 同写）。
4. BDA 分层：**类型池基址 + MeshDrawParams 基址 → UBO**；**mtl_data_addrs / 纹理引用池 / L2W 基址 → push constants**（L2W 暂 push）。
5. MeshDrawParams 池**永久固定**（预分配上限、只增不减），故基址放 UBO 一次写。
6. 4-ID draw buffer 做 **SSBO**：现 ReBAR CPU 写，未来 ComputeShader 写（GPU-driven 预留）。
7. TransformDataBuffer 沿用现有 **static/dynamic 两段**（静态不动、动态每帧）。
8. 材质字段类型池 **SSBOBufferRegistry 一次性创建，默认 1024 行**，超限 fail-fast。

---

## 9. 风险与注意事项

1. **全局行分配器**：阶段 1 引入；行号生命周期 = 分配/释放 ID（池 buffer 永不重建、基址不变）。释放复用行号前确认无 in-flight 引用（ReBAR 直写无 stage 问题，CS 写阶段需 ring/围栏）。⚠️ **可参考已落地的纹理引用池**（`free_rows` + `row_generations` + `retire_epoch` 延迟 3）——同一套「代际 + 延迟回收」机制可复用，避免各行一套。
2. **grow 语义**：类型池 1024 不扩容（fail-fast）；若未来扩容，grow 换 buffer → 基址变 → UBO 刷新（阶段 5 一次性成本，帧边界执行）。
3. **纹理引用归路（已落地）**：`MaterialTextureReferencePool` 按 **material-definition** 建（非 per-material 实例），行 = `uvec2(descriptor_index, array_layer)`；容量由 TOML `resources.texture_configurations.max_count`（默认 1024）控制；行 0 保留为零行；回收走 retire epoch。**运行时 sampler 创建尚未接入**（只到解析/布局 hash/契约）。
4. **MaterialID 层级**：两级（MaterialID → `mtl_data_addrs` 行 → 类型池 index），阶段 3 钉死字段布局；现形态该行是 16B 双地址。
5. **TransformID vs addr_transform_id**：前者实例级 L2W 行号，后者顶点级 id 流（`MeshDrawParams` 内）——阶段 4 澄清是否替代 l2w_index 链。
6. **ReBAR → CS 写**：阶段 4 的 4-ID SSBO 现 CPU 写，未来 CS 写需同步机制（不阻塞，仅预留形态）。
7. **L2W 归属**：暂 push；若迁 UBO（per-world），阶段 5 之外单独评估。

---

## 10. 现状关键文件清单（供拆小计划定位）

| 层 | 文件 | 关键符号/行 |
|---|---|---|
| 枚举 | `inc/hgl/graph/ssbo/SSBOTypes.h` | `SSBOType`(:9-24，已删 TextureLayer/TextureRectArraySurface)、`IsMaterialSSBOType`(:33-45)、stride 表(:81-102)、`ssbo_id` 工具(:104-154) |
| 行结构 | `inc/hgl/graph/ssbo/MaterialDataRows.h` | `PBRSurfaceRow`(32B)/`EmissiveSurfaceRow`(16B)/`TransmissionSurfaceRow`(16B) + sizeof 断言(:40-45)；tex_tail/TextureLayerRow/TextureRectArraySurfaceRow **已删** |
| 布局 | `inc/hgl/graph/ssbo/MaterialSSBOLayout.h` | `GetMaterialSSBOStructName/RowName/StructGLSL`、`MaterialRowTypeTraits`（3 条） |
| 池/域 | `inc/hgl/graph/module/SSBOBufferRegistry.h` | `AllocateArrayAccessor`(:191)、`AllocateSSBOId`(:167)、`row_segments`(:51)、`material_texture_reference_pools`(:52) |
| **纹理引用池** | `inc/hgl/graph/module/MaterialTextureReferencePool.h` + `src/SceneGraph/module/MaterialTextureReferencePool.cpp` | `MaterialTextureReference`/`Layout`（`MaterialRecipe.h:194-230`）、`Initialize`/`Acquire`/`Write`/`Retire`/`CollectRetired`、`MakePoolKey`(:86) |
| 访问器 | `inc/hgl/vk/SSBOArrayAccessor.h` | `SSBOArrayAccessor<T>`(ssbo_id/ssbo_type/OwnBuffer) |
| MeshDrawParams | `inc/hgl/graph/ShaderBufferSources.h` | `HGL_MESH_DRAW_PARAMS_FIELD_LIST`(:16-30)、88B 断言(:84)、`MaterialInstanceAddresses`(:89-97) |
| RootAddresses | 同上 | `HGL_ROOT_ADDRESSES_FIELD_LIST`(:105-112)、56B 断言 |
| auth | `inc/hgl/ecs/components/PrimitiveComponent.h/.cpp` | `materialPrivateDataSlotResources`(:96)、`ssbo_id`(:71)、`SetMaterialTextureResource`(:180)、`SetMaterialTextureLayer` |
| recipe | `inc/hgl/mtl/MaterialRecipe.h` | `RecipeSSBOAssetBinding`(ssbo_id :39)、`MaterialTextureReference`(:194)、`MaterialTextureReferenceLayout`(:218)、`BuildMaterialTextureReferenceLayout`(:447)、`DefaultMaterialTextureConfigurationCapacity`(:80) |
| 解析缓存 | `inc/hgl/ecs/components/MaterialComponent.h` | `ResolvedSSBOBinding`、`resolved_ssbo_bindings` |
| 收集/物化 | `src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp` | `ResolveRecipeSSBOBindingId`、`TryGetRowSegment`(:951)、`material_row_gpu`(:958)、纹理引用物化(:985-1224) |
| 批/命令 | `src/ecs/support/PrimitiveBatchPipeline.cpp` | `WriteMeshDrawCommands`、`MaterialInstanceAddresses` 行(:746/768/829) |
| 渲染器 | `src/ecs/support/PipelineMaterialRenderer.cpp` | push 表、indirect flush |
| 发射 | `src/ShaderGen/compile/MaterialShaderEmitter.cpp` | `BuildMaterialSSBODeclarations`(:91)、`MTL_ROW`(:167-169)、`MaterialTextureReferencesRef`(:183)、`MTL_TEX`(:194-196)、`MaterialInstanceAddressesRef`(:415) |
| 变换 | `inc/hgl/ecs/support/TransformAssignmentBuffer.h` | static/dynamic 段、`EnsureCapacity`、`WriteStatic/DynamicDirtyIndices` |
| 文本数据 | `src/ecs/support/text/TextRenderPipeline.cpp` | `MaterialInstanceAddresses` 单行表(:519-535)、`MTL_TEX(0)`(:281) |
| 示例 | `example/Basic/PBRSpheres.cpp` | `InitMaterialDataSSBO`、accessor + slot 资源、`SetMaterialTextureLayer("base_color", row)` |
| 示例 | `example/Basic/AutoMergeMaterialInstance.cpp` / `BasicLitMeshes.cpp` | 同型 accessor + slot 资源（多行共享块） |
| GLSL 消费 | `ShaderLibrary/material/pbr_surface_source.glsl`、`texture_source.glsl`、`text_source_gpu.glsl`、`ShaderLibrary/ntb/ntb_*_normalmap.glsl` | 均改 `MTL_TEX(dataIndex).tex_<name>`（`.x`=句柄 `.y`=layer） |
