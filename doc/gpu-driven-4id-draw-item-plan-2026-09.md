# 材质数据全局池化 + 4-ID Draw Item 大计划（技术文档）

> 基线：**2026-09-11 纹理引用重构后现状**（tex_tail 拆除、`MaterialTextureReferencePool` 落地、`MaterialInstanceAddresses` 16B 双地址）。
> 状态：总纲（不直接执行）。每个阶段由后续会话据此拆小计划、逐任务 build+run 验证。
> 目的：子会话拿到本文即可拆任务，无需重新 grep——结构、数据流、改动点、行号均以本基线核实。

---

## 1. 目标与终态

把渲染数据从「每帧 CPU 重建 per-batch 行表 + 内嵌 BDA 地址」改造为「创建期全局池化 + 渲染期只给 4 个 ID」，并建立「BDA 传递方式 = 数据生命周期」的分层。

**一句话**：数据全局池化、基址分层传递（UBO / push constants）、每 primitive 一个 4-ID draw item，CPU 只写 ID。

---

## 2. 现状（唯一基线，2026-09-11 后）

### 2.1 MeshDrawParams 行（88B）— `ShaderBufferSources.h:16-85`
X-macro 单源 `HGL_MESH_DRAW_PARAMS_FIELD_LIST`：
```
头部 6×4B（offset 0..20）：index_base, vertex_base, is_indexed,
                          total_vertices, char_height, first_instance
地址尾 8×uint64（offset 24 起 8B 步进）：addr_position, addr_uv, addr_ntb,
                          addr_color, addr_luminance, addr_transform_id,
                          addr_size, addr_index
```
- `addr_transform_id` 是「per-vertex transform id 流」地址（顶点级），与 4-ID 的实例级 `TransformID` **不同**（§4）。

### 2.2 材质字段行（纯业务 payload，无纹理句柄）— `MaterialDataRows.h`
| 行结构 | 大小 | 字段 |
|---|---|---|
| `PBRSurfaceRow` | 32B | base_color(16) + metallic/roughness/normal_scale/fresnel(各4) |
| `EmissiveSurfaceRow` | 16B | color(16) |
| `TransmissionSurfaceRow` | 16B | trans_color(4) + reserved0[3](12) |

- `static_assert(sizeof(Row)%16==0)` 全强制（:40-45）。
- 纹理引用已不在行内——由 `MaterialTextureReferencePool` 独立承载（§2.4）。
- `GetMaterialSSBOTypeStructStride`（`SSBOTypes.h`）= 整行 sizeof：
  PBR=32 / Emissive=16 / Transmission=16。

### 2.3 材质数据寻址 — `MaterialInstanceAddresses`（16B 双地址）
- `mtl_data_addrs` 每实例一行 = `MaterialInstanceAddresses`（`ShaderBufferSources.h:89-97`）：
  - `payload_address` → 材质**字段行**（PBR/Emissive/Transmission 行）；FS `MTL_ROW(i)` 解引用。
  - `texture_reference_address` → 该实例的**纹理引用行**；FS `MTL_TEX(i)` 解引用。
- FS 宏（`MaterialShaderEmitter.cpp`）：
  - `MTL_ROW(i)`（:167-169）= `行结构(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[i].payload_address)`。
  - `MTL_TEX(i)`（:194-196）= `MaterialTextureReferencesRef(...values[i].texture_reference_address)`。
  - `fragDataIndexID` = draw item 序号（表下标）。
- 表写入：`PrimitiveBatchPipeline.cpp:746/768/829`（每批）；文本路径单行表 `TextRenderPipeline.cpp:519-535`。
- 字段行地址 = `gpu_base + data_index × row_bytes`（`RenderPrimitiveCollectSystem.cpp`
  物化，`MaterialSSBOBufferRegistry::TryGetRowBuffer(ssbo_id)` 提供共享材质行段）。

### 2.4 纹理引用（已落地，per-definition 池）
- `MaterialTextureReference`（`MaterialRecipe.h:194-200`）= `{uint32 descriptor_index, array_layer}`（8B，非数组 layer=0）。
- `MaterialTextureReferenceLayout`（`MaterialRecipe.h:218-230`）= `{layout_hash, reference_count, row_stride(16B 对齐), max_configuration_count}`，由 `.material.toml` `resources.textures` 声明顺序 + sampler/policy 推导（`BuildMaterialTextureReferenceLayout` :447-453）。
- `MaterialTextureReferencePool`（`inc/hgl/graph/module/MaterialTextureReferencePool.h` + `.cpp`）：per material-definition arena SSBO（`CreateArenaBuffer`）；行 = `uvec2(descriptor_index, array_layer)`；容量 = `resources.texture_configurations.max_count`（默认 1024，`DefaultMaterialTextureConfigurationCapacity`，`MaterialRecipe.h:80`）；行 0 保留零行；`Acquire`/`Write`/`Retire`/`CollectRetired`（自研 `free_rows` 栈 + `row_generations` 代际 + retire epoch 延迟 3）。
- authoring 名称化：`SetMaterialTextureResource(name, texture, sampler, kind, resource_id, array_layer)`；
  `SetMaterialTextureArrayLayer(name, layer)` 只作为已绑定资源的更新 helper，名称按 TOML
  layout 校验。
- GLSL：`MTL_TEX(dataIndex).tex_<texture_name>`（`.x`=descriptor index，`.y`=array layer）。
- 2D 与 Texture2DArray 统一注册进 `texture2DArray[]`（2D 用 layer 0）。

### 2.5 RootAddresses（56B = 7×uint64）— `ShaderBufferSources.h:105-112`
```
addr_mesh_draw_params, addr_l2w, addr_l2w_index, addr_mtl_data_addrs,
addr_text_char_info, addr_text_char_style, addr_text_char_instance
```
- 现全 push constant；阶段 5 拆分。
- `addr_mtl_data_addrs` 指向的表元素已是 16B `MaterialInstanceAddresses`。

### 2.6 SSBO 域注册 — `SSBOBufferRegistry.h`
- `domain_map`: `(ssbo_type, ssbo_id)` → `SSBOBufferBinding`；`row_segments`: `ssbo_id` → `RowSegmentInfo`（:51）。
- `next_ssbo_id` 自增 + `AllocateSSBOId()`（:167）。
- `AllocateArrayAccessor<T>(ssbo_type, name, count)`（:191）：`AllocateSSBOId` → `CreateArenaBuffer` → `Map`（HOST_COHERENT）→ `memset` → 包 `ArrayView<T>` → `row_segments.emplace(id, seg)`（:232）。
- 该 registry 仅服务通用非材质 SSBO；通用资源仍允许按 `ssbo_id` 使用多块 buffer。
- 材质 payload 不进入此路径，改由 `MaterialSSBOBufferRegistry` 按
  `MaterialSSBOType` 预创建共享 buffer。
- 同时托管 `material_texture_reference_pools`（:52）+ `FindMaterialTextureReferencePool`（:71/74）——纹理引用池按 material-definition 建/查。
- `null_row_buffer`（64B 零填充空行，`GetNullRowAddress`）——地址表「无有效行」安全缺省。

### 2.7 资产绑定链（阶段 6 已收口）
- `PrimitiveComponent` 只保留一个 `MaterialDataAuthoringResource`，通过
  `SetMaterialDataResource()` 写入材质数据资源。
- `MaterialRecipe` 只保留一个 `MaterialSSBOBinding`，由
  `MaterialSSBOType`、共享 `ssbo_id` 和实例 `data_index` 标识。
- `MaterialComponent` 不再缓存 resolved binding；渲染收集阶段直接校验并消费
  `cached_effective_recipe` 的材质 binding。
- `RenderPrimitiveCollectSystem.cpp` 直接完成 recipe 校验、共享 SSBO 行物化和 BDA
  地址表写入，不再维护多键 asset 绑定过渡层。
- 编译侧仍由 `ResolveEffectiveMaterialPrivateData` 合并 definition 与 provider
  manifest，得到单一材质数据类型。

### 2.8 示例侧 accessor 创建（阶段 1 调用点）
- `PBRSpheres::InitMaterialDataSSBO()`：创建
  `material_data_ssbo_accessor`（PBRSurface），将 accessor 的 binding 写入 recipe，
  再通过 `SetMaterialDataResource()` 关联到每个 primitive；纹理使用
  `SetMaterialTextureResource("base_color"/"normal", …, Texture2DArray, "", row)`。
- 同型：`AutoMergeMaterialInstance.cpp`、`BasicLitMeshes.cpp`。

---

## 3. 终态数据模型

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

## 4. ID 语义词典

| ID | 含义（终态） | 现状对应 | 备注 |
|---|---|---|---|
| `TransformID` | L2W 池行号（实例变换） | `l2w_index[gl_InstanceIndex]` 查行 | 与 `addr_transform_id`（顶点级）**不同** |
| `GeometryID` | MeshDrawParams 池行号（几何+绘制参数） | ICB 命令序 = 行序，`gl_DrawID` 查行 | 阶段 2 池化后变持久 ID |
| `MaterialID` | 材质参数寻址中间层（终态 index） | `fragDataIndexID` 查 `MaterialInstanceAddresses` 行 | 该行现 16B 双地址，阶段 3 改双 index |
| `TextureID` | 纹理引用行号 | `MaterialTextureReferencePool` 行（per-definition） | **已落地**（§2.4） |

**寻址链（终态）**：
- 几何：`GeometryID` → MeshDrawParams 池行 → 顶点流地址/段偏移。
- 材质：`MaterialID` → `mtl_data_addrs` 行（终态存 index）→ `UBO[type].base + index×row_bytes` → 字段。
- 纹理：`TextureID` → `MaterialTextureReferencePool` 行 → `uvec2(descriptor_index, array_layer)` → bindless 采样。
- 变换：`TransformID` → L2W 池（静态段/动态段）。

---

## 5. 阶段依赖图

```
阶段 1（类型池化）───┐
                      ├──→ 阶段 3（mtl_data_addrs 双地址 → 双 index）──┐
阶段 2（MeshDrawParams 池化）─┘                                         ├──→ 阶段 4（4-ID）──→ 阶段 5（UBO 分层）──→ 阶段 6（删链）
（阶段 1 与 2 可并行；3 依赖 1（payload index）、纹理 index 部分独立；
 4 依赖 1-3；5 依赖 4；6 依赖 1-5）
```

---

## 6. 分阶段改造

### 6.1 阶段 1 — 材质字段类型池化（已完成）

**结果**：每字段类型（PBR/Emissive/Transmission）由
`MaterialSSBOBufferRegistry` 预创建一个共享 SSBO（默认 1024 行），每类由
`ActiveIDManager` 管理行 ID；`MaterialSSBODataAccessor<T>` 负责 RAII 申请/释放。
`ssbo_id` 仍表示共享物理 buffer identity，实例隔离由 `data_index` 完成。

**实现**：
```
GetMaterialDataAccessor<PBRSurfaceRow>()
  → registry 预创建的共享 buffer + ActiveIDManager → {ssbo_id, data_index}
每球 SetMaterialDataResource(binding)
RPC 物化 → TryGetRowBuffer(ssbo_id) → row_gpu = gpu_base + data_index×row_bytes
```

**涉及文件**：`MaterialSSBOBufferRegistry.h/.cpp`、
`MaterialSSBOLayout.h`、`MaterialDataRows.h`、`MaterialRecipe.h`、
`PrimitiveComponent.h/.cpp`、`RenderPrimitiveCollectSystem.cpp` 和示例 accessor
创建点。通用 `SSBOBufferRegistry` 仍只负责非材质 SSBO 与材质纹理引用池。

---

### 6.2 阶段 2 — MeshDrawParams 池化

**目标**：所有 primitive 几何绘制参数创建期一次写入全局池，`GeometryID` 引用；池永久固定（预分配上限、只增不减、基址不变）。

**现状**：`PrimitiveBatchPipeline::WriteMeshDrawCommands` 每帧按 draw 序写 88B 行 + ICB 命令（命令序=行序）。

**终态**：primitive 创建期 `EnsureMeshDrawParams` 写一次池行 → `GeometryID`；渲染期命令面只引用 `GeometryID`。

**改动点**：
1. 建 `MeshDrawParamsPool`（或复用 registry 池化机制）：`EnsureMeshDrawParams(geometry) → GeometryID`，创建期写地址尾 + 段偏移。
2. `WriteMeshDrawCommands` 删每帧写行；`EnsureMeshDrawParams` 移创建期。
3. `GeometryDataBuffer` 创建点分配 `GeometryID`。

**涉及文件**：`PrimitiveBatchPipeline.cpp`、`ShaderBufferSources.h`、geometry 资产创建点、`PipelineMaterialRenderer.cpp`。

**验收**：编译 + 全部示例；MeshDrawParams 行只在创建期写一次；几何不变池零重写。

**待澄清**：`GeometryID` 粒度（per asset vs per 运行时实例）；池容量（同 1024？）；`addr_transform_id`（顶点级 id 流）是否本阶段一并池化。

---

### 6.3 阶段 3 — `mtl_data_addrs` 双地址 → 双 index

**目标**：`MaterialInstanceAddresses` 的两个 `uint64_t` 地址 → 两个 `uint32_t` index；核心「地址 → 类型池基址 + index」两级寻址。

**现状（16B 双地址）**：
```
RPC 物化 → mtl_data_addrs[实例] = { payload_address, texture_reference_address }
FS → MTL_ROW(i)= 行结构(values[i].payload_address)；MTL_TEX(i)= 引用行(values[i].texture_reference_address)
```

**终态（双 index）**：
```
mtl_data_addrs[实例] = { payload_index, texture_reference_index }（4B×2）
FS → UBO[type].base + index×row_bytes → 行（payload）；纹理 index 直查 MaterialTextureReferencePool
```

**改动点**：
1. `MaterialInstanceAddresses` 两字段 `uint64_t` → `uint32_t`；`MaterialInstanceAddressesRef`/`MaterialTextureReferencesRef` 元素类型随之改。
2. `MTL_ROW(i)`/`MTL_TEX(i)`（`MaterialShaderEmitter.cpp:167-169`/`:194-196`）改两级解引用：类型池基址（UBO）+ index×row_bytes；纹理 index 直查纹理池（池基址 push/UBO，阶段 5 定）。
3. 物化（`RenderPrimitiveCollectSystem.cpp:951/:958` + 纹理段）由「算地址」改「填 index」；`TryGetRowSegment` → `row_pool[type]`（阶段 1 产物）。
4. FS 发射新增「类型→UBO 字段名」固定配表（`PBRSurface → addr_pbr_surface` 等）。
5. 纹理侧：`MaterialTextureReferencePool` 行号即 `TextureID`（已完成），`texture_reference_index` 直接填该行号。

**涉及文件**：`ShaderBufferSources.h`（`MaterialInstanceAddresses` 字段类型）、`MaterialShaderEmitter.cpp`、`RenderPrimitiveCollectSystem.cpp`、`PrimitiveBatchPipeline.cpp`、`TextRenderPipeline.cpp`（单行表）、`MaterialShaderCompiler.cpp`（发射配表）。

**验收**：编译 + 数据槽示例（SimpleCube/BasicLitMeshes/PBRSpheres）+ 文本 + Texture2DArray layer；grow 场景 index 稳定；`MTL_ROW`/`MTL_TEX` 两步解引用视觉正确。

**待澄清**：`payload_index` 与 `texture_reference_index` 是否统一 index 空间（同 `MaterialID`？）；纹理池基址（per-definition）进 UBO 还是随材质 push（阶段 5 一并定）。

---

### 6.4 阶段 4 — 4-ID draw item SSBO

**目标**：每 primitive 一个 `{TransformID, GeometryID, MaterialID, TextureID}`（16B）紧凑结构，替代 88B 内嵌地址行。SSBO 形态，ReBAR CPU 直写（预留 CS 写）。

**现状**：`MeshDrawParams` 行（88B）内嵌 8 地址 + 段偏移，per-batch 每帧写；材质侧 16B 双地址。

**终态**：
```
4-ID draw item 表（SSBO，per primitive，ReBAR 直写）
ICB 命令面 → gl_DrawID → 4-ID 行 → 4 个 ID → 各池
```

**改动点**：
1. 新 `DrawItem4ID`（X-macro + offsetof 断言，仿 `MeshDrawParams`）：`{uint32_t transform_id, geometry_id, material_id, texture_id}`（16B）。
2. 4-ID 表建 SSBO（ReBAR 直写）；`MeshDrawParams` 行瘦身为 `GeometryID` 引用（阶段 2 池化后）。
3. `PipelineMaterialRenderer`/`PrimitiveBatchPipeline` draw 提交改走 4-ID；ICB 多命令保留。

**涉及文件**：`ShaderBufferSources.h`（`DrawItem4ID` X-macro）、`PrimitiveBatchPipeline.cpp`、`PipelineMaterialRenderer.cpp`、`SSBOBufferRegistry`（4-ID 表 SSBO，ReBAR）。

**验收**：编译 + 全部示例；合批命令数不变、命令面只引用 ID；换 ID 视觉正确。

**待澄清**：`TransformID` 是否替代 `l2w_index`/`ResolveTransformID` 链；4-ID 表全局池化（per primitive 常驻）还是 per-batch 每帧填；`TextureID` 与 `MaterialTextureReferencePool` 行号对应关系。

---

### 6.5 阶段 5 — UBO 分层

**目标**：类型池基址 + MeshDrawParams 基址挪入全局 UBO（一次写永久）；`mtl_data_addrs`/纹理引用池/`L2W` 基址保持 push constants。

**现状**：`RootAddresses`（7×uint64）全 push，每 MaterialBatch push 一次。

**终态**：
```
全局 UBO：addr_mesh_draw_params + addr_pbr_surface + addr_emissive_surface + addr_transmission_surface
push constants：addr_mtl_data_addrs + addr_texture_references + addr_l2w
```

**改动点**：
1. 新 `GlobalPoolAddresses` UBO（X-macro + offsetof 断言）。
2. `RootAddresses` 收敛：去掉挪走字段（`addr_mesh_draw_params`）；类型池基址是新增 UBO 字段。
3. `VKPipelineLayoutData.cpp`/`RootAddressPush.h`：UBO 绑定 + push 字段收缩；三渲染路径（PMR/Line/Text）push 调用更新。
4. GLSL 发射：`pc_root` 减字段 + 新 UBO block 声明（mesh+FS 双侧一致，同 X-macro）。

**涉及文件**：`ShaderBufferSources.h`、`VKPipelineLayoutData.cpp`、`RootAddressPush.h`、`MeshShaderHeaderGen.h`/`MaterialShaderEmitter.cpp`、三渲染器 push 调用。

**验收**：编译 + 全部示例；永久地址一次写、不再随批 push；push 面只含随上下文变的地址。

**待澄清**：新 UBO 放 Scene 集（binding 4）还是独立全局集；L2W 是否本阶段挪 UBO；纹理引用池基址（per-definition）是否进 UBO。

---

### 6.6 阶段 6 — 删 asset 绑定链（已完成）

**目标**：删除「类型即身份」后冗余的多键结构，收敛为单槽 + 类型直指。

**删除面**（纹理 authoring 已名称化，不在本阶段）：
1. `PrimitiveComponent`：多键 authoring resource 收敛为单一材质数据资源。
2. `MaterialRecipe`：多项 asset 列表收敛为唯一 `MaterialSSBOBinding`。
3. `MaterialComponent`：删除 resolved binding 缓存和过渡期查询 API。
4. `RenderPrimitiveCollectSystem`：直接消费 recipe binding，删除多键物化过渡层。
5. 编译期 schema、契约和 provider manifest 删除材质数据的数值 slot 字段；
   `MaterialDefinition.material_private_data` 直接选择材质 BDA row 类型，provider metadata
   不再参与 payload 合并。
6. `MakeRecipeSSBOId(local_id)` 只保留通用 recipe SSBO identity 生成用途。

**验收**：编译 + gate + 全仓 grep 确认旧材质 slot 字段、多键 vector 和 slot-keyed
authoring API 均无残留。

---

## 7. 已完成事项 + 开放待办

### 7.1 已完成（2026-09-11 纹理引用重构）
- tex_tail/`TextureSlot` 固定槽/`TextureLayerRow`/`TextureRectArraySurfaceRow` 删除；材质行收敛为纯 payload（3 类型）。
- `MaterialTextureReferencePool`（per-definition 池）+ `MaterialTextureReferenceLayout`（TOML 声明顺序推导）。
- `MaterialInstanceAddresses`（16B 双地址）取代原 8B 单地址行。
- authoring 名称化（`SetMaterialTextureResource(name,..., array_layer)` /
  `SetMaterialTextureArrayLayer(name, layer)`）；旧 `TextureSlot::Custom0` layer 通道删除。
- 2D/Texture2DArray 统一进 `texture2DArray[]`；专用数组材质/provider 删除。
- `SSBOType` 枚举收敛（`TextureLayer`/`TextureRectArraySurface` 已删）。

### 7.2 开放待办（独立，不阻塞阶段 1-6）
1. **sampler 运行时创建未接入**——filter/wrap/swizzle/compare 的 TOML 配置只到「解析 + 布局 hash + 契约传递」。
2. **屏蔽示例恢复**——`Geometry/LoadGeometry/LoadScene`（+`Context.cpp`）纹理重构后未适配，暂屏蔽，需恢复。

---

## 8. 已拍板决策清单

1. 一个 `MaterialSSBOType` 只会有一个材质 SSBO。
2. 材质字段专用类型另立枚举；纹理引用已独立成 `MaterialTextureReferencePool`（per-definition）。
3. 任意材质可访问所有材质字段 SSBO（固定配表：`PBRSurface` → `pbr_surface` 等）。
4. BDA 分层：类型池基址 + MeshDrawParams 基址 → UBO；`mtl_data_addrs`/纹理引用池/L2W 基址 → push constants（L2W 暂 push）。
5. MeshDrawParams 池永久固定（预分配上限、只增不减）。
6. 4-ID draw buffer 做 SSBO：现 ReBAR CPU 写，未来 CS 写。
7. TransformDataBuffer 沿用 static/dynamic 两段。
8. 材质字段类型池由 `MaterialSSBOBufferRegistry` 管理，默认 1024 行，超限
   fail-fast。

---

## 9. 风险与注意事项

1. **全局行分配器**（阶段 1）：行号分配/释放，池 buffer 永不重建；释放复用前确认无 in-flight 引用。可复用纹理池已验证的「free_rows + 代际 + retire epoch」机制（`row_generations` + `MaterialTextureConfigurationRetirement`，retire_epoch 延迟 3），避免各行一套。
2. **grow 语义**：类型池 1024 不扩容（fail-fast）；未来扩容则基址变 → UBO 刷新（阶段 5 一次性成本，帧边界执行）。
3. **MaterialID 层级**：两级（MaterialID → `mtl_data_addrs` 行 → 类型池 index），阶段 3 钉死字段布局；现形态该行 16B 双地址。
4. **TransformID vs addr_transform_id**：实例级 L2W 行号 vs 顶点级 id 流——阶段 4 澄清是否替代 l2w_index 链。
5. **ReBAR → CS 写**：阶段 4 的 4-ID SSBO 现 CPU 写，未来 CS 写需同步机制（不阻塞，仅预留形态）。
6. **L2W 归属**：暂 push；若迁 UBO（per-world），阶段 5 之外单独评估。
7. **纹理引用池（已完成）**：per-definition 建，容量 TOML `texture_configurations.max_count` 默认 1024，行 0 保留；回收走 retire epoch。运行时 sampler 创建待接（§7.2）。

---

## 10. 现状关键文件清单（供拆小计划定位）

| 层 | 文件 | 关键符号/行 |
|---|---|---|
| 枚举 | `inc/hgl/graph/ssbo/SSBOTypes.h` | 通用 `SSBOType` 与材质专用 `MaterialSSBOType`、类型名和 stride 工具 |
| 行结构 | `inc/hgl/graph/ssbo/MaterialDataRows.h` | `PBRSurfaceRow`(32B)/`EmissiveSurfaceRow`(16B)/`TransmissionSurfaceRow`(16B) + sizeof 断言(:40-45) |
| 布局 | `inc/hgl/graph/ssbo/MaterialSSBOLayout.h` | `GetMaterialSSBOStructName/RowName/StructGLSL`、`MaterialRowTypeTraits` |
| 材质池 | `inc/hgl/graph/module/MaterialSSBOBufferRegistry.h/.cpp` | 预创建共享材质 buffer、每类型 `ActiveIDManager`、`MaterialSSBODataAccessor<T>`、`TryGetRowBuffer` |
| 通用池/域 | `inc/hgl/graph/module/SSBOBufferRegistry.h` | 非材质 `AllocateArrayAccessor`、通用 `ssbo_id`、材质纹理引用池 |
| 纹理引用池 | `inc/hgl/graph/module/MaterialTextureReferencePool.h` + `.cpp` | `Acquire/Write/Retire/CollectRetired`、`MakePoolKey`(:86) |
| 纹理引用结构 | `inc/hgl/mtl/MaterialRecipe.h` | `MaterialTextureReference`(:194)、`MaterialTextureReferenceLayout`(:218)、`BuildMaterialTextureReferenceLayout`(:447)、`DefaultMaterialTextureConfigurationCapacity`(:80) |
| 访问器 | `inc/hgl/graph/module/MaterialSSBOBufferRegistry.h` | `MaterialSSBODataAccessor<T>`（RAII 行 ID/提交/释放） |
| MeshDrawParams | `inc/hgl/graph/ShaderBufferSources.h` | `HGL_MESH_DRAW_PARAMS_FIELD_LIST`(:16-30)、88B 断言(:84)、`MaterialInstanceAddresses`(:89-97) |
| RootAddresses | 同上 | `HGL_ROOT_ADDRESSES_FIELD_LIST`(:105-112)、56B 断言 |
| auth | `inc/hgl/ecs/components/PrimitiveComponent.h/.cpp` | 单一 `MaterialDataAuthoringResource`、`SetMaterialDataResource`、`SetMaterialTextureResource`、`SetMaterialTextureArrayLayer` |
| recipe | `inc/hgl/mtl/MaterialRecipe.h` | 唯一 `MaterialSSBOBinding`（`ssbo_id` + `data_index`） |
| 解析缓存 | `inc/hgl/ecs/components/MaterialComponent.h` | `cached_effective_recipe`；不再缓存 resolved binding/table |
| 收集/物化 | `src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp` | `MaterializeRecipeRowsForPrimitive`、`TryGetRowBuffer`、纹理引用物化 |
| 批/命令 | `src/ecs/support/PrimitiveBatchPipeline.cpp` | `WriteMeshDrawCommands`、`MaterialInstanceAddresses` 行(:746/768/829) |
| 渲染器 | `src/ecs/support/PipelineMaterialRenderer.cpp` | push 表、indirect flush |
| 发射 | `src/ShaderGen/compile/MaterialShaderEmitter.cpp` | `BuildMaterialSSBODeclarations`(:91)、`MTL_ROW`(:167-169)、`MTL_TEX`(:194-196)、`MaterialInstanceAddressesRef`(:415) |
| 变换 | `inc/hgl/ecs/support/TransformAssignmentBuffer.h` | static/dynamic 段、`EnsureCapacity`、`WriteStatic/DynamicDirtyIndices` |
| 文本数据 | `src/ecs/support/text/TextRenderPipeline.cpp` | `MaterialInstanceAddresses` 单行表(:519-535)、`MTL_TEX(0)`(:281) |
| 示例 | `example/Basic/PBRSpheres.cpp` | `InitMaterialDataSSBO`、`MaterialSSBODataAccessor` + `SetMaterialDataResource`、`SetMaterialTextureResource(..., Texture2DArray, "", row)` |
| 示例 | `example/Basic/AutoMergeMaterialInstance.cpp` / `BasicLitMeshes.cpp` | 同型材质 accessor + 单一材质数据资源 |
