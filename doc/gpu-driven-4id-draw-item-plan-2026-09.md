# 材质数据全局池化 + 4-ID Draw Item 大计划（技术文档）

> 状态：**总纲**（不直接执行）。每个阶段由后续会话据此拆小计划、逐任务 build+run 验证。
> 前置：vertex-bda-v2 主线（7 表 BDA + pc_root + 合批统一）已闭环；A6-2b 终态 Scene(0)/Bindless(1) 两集。

---

## 0. 目标与终态

把渲染数据从「每帧 CPU 重建 per-batch 行表 + 内嵌 BDA 地址」改造为「创建期全局池化 + 渲染期只给 4 个 ID」，并建立「BDA 传递方式 = 数据生命周期」的分层。

**一句话**：数据全局池化、基址分层传递（UBO / push）、每 primitive 一个 4-ID draw item，CPU 只写 ID。

---

## 1. 现状数据面（扫描结论，供各阶段引用）

### 1.1 材质专用 SSBO 类型（`SSBOTypes.h:10-50`）
- 引擎内部：`MeshDrawParams` / `LocalToWorld` / `LocalToWorldIndex` / `MaterialPrivateDataIndex`
- 材质字段：`PBRSurface` / `EmissiveSurface` / `TextureRectArraySurface` / `TransmissionSurface`
- 纹理句柄：`TextureLayer`（`TextureLayerRow`，per-material）
- `UserDefined`（通配）
- `IsMaterialSSBOType`（`:37-50`）混收字段 + TextureLayer + UserDefined——**待拆分枚举**。

### 1.2 buffer 粒度与实例行（`SSBOBufferRegistry.h:145-188`）
- `AllocateArrayAccessor<T>(name, count)`：每次调用 `AllocateSSBOId()`（自增）+ 建独立 arena buffer → **同类型可多块 buffer**（`ssbo_id` 维度）。
- 实例行：`data_index`（accessor 内行号）；`shared_across_instances` 支持多实例共享一行。
- 行表 `mtl_data_addrs`：每实例一行，存该实例材质数据行的 **BDA 地址**（`MaterialDataAddressesRef.values[i]`，`MaterialShaderEmitter.cpp:390/188-190`）。
- `fragDataIndexID` = draw item 序号（行表下标）；FS 经 `MTL_ROW(i)` 取行地址解引用。

### 1.3 每 primitive 的资产绑定链（多键残留，待删）
- `PrimitiveComponent.materialPrivateDataSlotResources`（**vector**，`PrimitiveComponent.h:95`），按 `(slot_name, slot)` 键 Set/Get/Clear（`.cpp:359-434`）。
- `recipe.ssbo_assets`（**vector**，`MaterialRecipe.h:280`），`RecipeSSBOAssetBinding`（`:38-47`）带 slot 键。
- `MaterialComponent.resolved_ssbo_bindings`（**vector**，`MaterialComponent.h:52`），`ResolvedSSBOBinding` 按键。
- `RenderPrimitiveCollectSystem.cpp`：`ResolveRecipeSSBOBindingId`（`:75-87`）、物化循环 `:826-845`、`ssbo_assets` 遍历 `:873-900`。
- 编译侧已单槽：`ResolveEffectiveMaterialPrivateData`（definition ⊕ manifest 合并单 `SSBOType`）。

### 1.4 L2W（`TransformAssignmentBuffer.h:34,56,74-85`）
- **已分 static/dynamic 两段** + ring + 行表。静态段数据不动，动态段每帧生成——本计划沿用，不新增改造。

### 1.5 每帧重建的行表
- `MeshDrawParams` 行：`PrimitiveBatchPipeline::WriteMeshDrawCommands` 每帧按 draw 序写（88B：段偏移 + 8×顶点流 BDA）。
- `mtl_data_addrs` 行：RPC 物化时每帧填行地址。

---

## 2. 终态数据模型

```
┌─ 全局 UBO（一次写，永久）────────────────────────────┐
│  addr_mesh_draw_params      MeshDrawParams 池基址    │
│  addr_pbr_surface           PBRSurface 池基址        │
│  addr_emissive_surface      EmissiveSurface 池基址   │
│  addr_transmission_surface  TransmissionSurface 池基址│
│  addr_texture_rect_array    TextureRectArraySurface 池基址│
└───────────────────────────────────────────────────────┘

┌─ push constants（随材质/场景）───────────────────────┐
│  addr_mtl_data_addrs        每材质 mtl_data_addrs 池基址 │
│  addr_texture_layer_rows    每材质 TextureLayerRow 池基址│
│  addr_l2w                   per-world L2W 池基址（暂）  │
└───────────────────────────────────────────────────────┘

┌─ 4-ID draw item（SSBO，per primitive）──────────────┐
│  { TransformID, GeometryID, MaterialID, TextureID } │
│  （现 ReBAR CPU 直写；未来 ComputeShader 写）        │
└───────────────────────────────────────────────────────┘
```

**寻址链**：
- 几何：`GeometryID` → MeshDrawParams 池行 → 顶点流地址 / 段偏移
- 材质：`MaterialID` → mtl_data_addrs 池行（该行存「类型池内 index」）→ `UBO[type].base + index` → 字段
- 纹理：`TextureID` → TextureLayerRow 池行 → `tex_tail` 句柄
- 变换：`TransformID` → L2W 池（静态段 / 动态段）

---

## 3. 分阶段改造（每段独立拆小计划）

### 阶段 1 — 材质字段类型池化
- **目标**：每字段类型一个全局唯一大 SSBO；`SSBOBufferRegistry` 一次性创建，默认 **1024 行**，超限 fail-fast。
- **改造**：新增「材质字段专用枚举」（与引擎内部类型彻底分开）；`AllocateArrayAccessor` 改为按类型全局池分配（去 `AllocateSSBOId` 自增多块），行号 = 全局行分配器产出。
- **涉及**：`SSBOTypes.h`（拆枚举）、`SSBOBufferRegistry.h/.cpp`（池化 + 全局行分配）、示例侧 accessor 创建点（PBRSpheres / BasicLitMeshes / AutoMerge / Text）。
- **依赖**：无（独立）。
- **验收**：编译 + 全部示例视觉一致；同类型 accessor 共块（`ssbo_id` 维度消失）。

### 阶段 2 — MeshDrawParams 池化
- **目标**：所有 primitive 的几何绘制参数创建期一次写入全局池，`GeometryID` 引用；池永久固定（预分配上限、只增不减、删除留空洞）。
- **改造**：`WriteMeshDrawCommands` 每帧重建 → primitive 创建期 `EnsureMeshDrawParams` 写一次；池基址永久固定。
- **涉及**：`PrimitiveBatchPipeline.cpp`（建池/写行/ID 分配）、`PipelineMaterialRenderer.cpp`（不再每批填行）、geometry 生命周期（`GeometryDataBuffer` 创建点）。
- **依赖**：阶段 1（可选并行）。
- **验收**：编译 + 全部示例；确认 MeshDrawParams 行只在创建期写一次（日志/断点）。

### 阶段 3 — mtl_data_addrs 改 index + TextureLayerRow 池化
- **目标**：行表存 **index**（非地址）；`TextureLayerRow` 每材质池化（行号 `TextureID`）。
- **改造**：`mtl_data_addrs` 列 8B 地址 → 4B index；FS `MTL_ROW(i)` 改「`UBO[type].base + index`」两步解引用；`TextureLayerRow` 由 per-material 独立 buffer → 每材质池（仍 per-material，排除全局字段池）。
- **涉及**：`MaterialShaderEmitter.cpp`（MTL_ROW / MaterialDataAddressesRef / 材质行声明）、`PrimitiveBatchPipeline.cpp` / `RenderPrimitiveCollectSystem.cpp`（行表填 index）、Text 数据路径（`TextRenderPipeline.cpp` 的 texture_layer_buffer）。
- **依赖**：阶段 1、2。
- **验收**：编译 + 数据槽示例（SimpleCube/BasicLitMeshes/PBRSpheres）+ 文本；grow 场景下 index 稳定（换池内容不改 ID）。

### 阶段 4 — 4-ID draw item SSBO
- **目标**：每 primitive 一个 `{ TransformID, GeometryID, MaterialID, TextureID }` 紧凑结构，替代 88B 内嵌地址行。
- **形态**：SSBO，现 ReBAR CPU 直写，预留未来 ComputeShader 写。
- **涉及**：新 draw item 结构（X-macro 单源 + offsetof 断言）、ICB 命令面引用 ID（`gl_DrawID` → 4-ID → 池）、`PipelineMaterialRenderer` / `PrimitiveBatchPipeline` draw 提交改走 ID。
- **依赖**：阶段 1、2、3。
- **验收**：编译 + 全部示例；合批（indirect multi-draw）不变，命令面只引用 ID；换数据 = 换 ID，池内容不动。

### 阶段 5 — UBO 分层
- **目标**：类型池基址 + MeshDrawParams 基址挪入全局 UBO（一次写永久）；`mtl_data_addrs` / `TextureLayerRow` / `L2W` 基址保持 push constants（随材质/场景）。
- **改造**：新全局 UBO（放 Scene 集或独立全局集）；pc_root 去掉挪走字段；push 面收缩为「随上下文变」的地址。
- **涉及**：`ShaderBufferSources.h`（RootAddresses 收敛）、`VKPipelineLayoutData.cpp` / `RootAddressPush.h`（UBO 绑定 + push 字段）、`RootAddresses` X-macro、三渲染路径 push 调用（PMR/Line/Text）。
- **依赖**：阶段 4。
- **验收**：编译 + 全部示例；验证永久地址（MeshDrawParams/类型池）确为一次写、不再随批 push。

### 阶段 6 — 删 asset 绑定链（多 SSBO 假能力）
- **目标**：删除「类型即身份」后冗余的多键结构。
- **删除面**：`PrimitiveComponent.materialPrivateDataSlotResources` vector + `(slot_name, slot)` 键 API、`recipe.ssbo_assets` vector + `RecipeSSBOAssetBinding` 键字段、`MaterialComponent.resolved_ssbo_bindings` + `ResolvedSSBOBinding`、`ResolveRecipeSSBOBindingId`、`MakeRecipeSSBOId(slot)`、RPC 物化循环；`material_private_data_slot` 字段（schema 107 处）收敛。
- **依赖**：阶段 1-5。
- **验收**：编译 + gate + 全仓 grep 零残留；`ssbo_id` / 多键 vector / slot 键全部消失。

---

## 4. 已拍板决策清单

1. 一个 SSBOType 只会有一个 SSBO（仅材质字段专用类型）。
2. 材质字段专用类型**另立枚举**，与引擎内部类型彻底分开；`TextureLayerRow` 排除在全局字段池外（per-material 池）。
3. 任意材质可访问所有材质字段 SSBO（固定配表：`PBRSurface` → `pbr_surface` 等固定名，shader 同写）。
4. BDA 分层：**类型池基址 + MeshDrawParams 基址 → UBO**；**mtl_data_addrs / TextureLayerRow / L2W 基址 → push constants**（L2W 暂 push）。
5. MeshDrawParams 池**永久固定**（预分配上限、只增不减），故基址放 UBO 一次写。
6. 4-ID draw buffer 做 **SSBO**：现 ReBAR CPU 写，未来 ComputeShader 写（GPU-driven 预留）。
7. TransformDataBuffer 沿用现有 **static/dynamic 两段**（静态不动、动态每帧）。
8. 材质字段类型池 **SSBOBufferRegistry 一次性创建，默认 1024 行**，超限 fail-fast。

---

## 5. 风险与注意事项

1. **全局行分配器**：阶段 1 引入；行号生命周期 = 分配/释放 ID（池 buffer 永不重建、基址不变）。释放复用行号前确认无 in-flight 引用（ReBAR 直写无 stage 问题，但 CS 写阶段需 ring/围栏）。
2. **grow 语义**：类型池默认 1024 不扩容（fail-fast）；若未来需扩容，grow 换 buffer → 基址变 → UBO 刷新（阶段 5 一次性成本，帧边界执行）。
3. **TextureLayerRow 归路**：明确「per-material 池」而非全局字段池；`TextureID` 只在材质内有效（与 4-ID 中 `MaterialID` 的层级一致）。
4. **4-ID 的 MaterialID 层级**：指向 `mtl_data_addrs` 行（该行存类型池 index），还是直接类型池行——按「mtl_data_addrs 存 index」既定，是**两级**（MaterialID → mtl_data_addrs 行 → 类型池 index）。拆小计划时在阶段 3 钉死字段布局。
5. **L2W 归属**：暂 push；若迁 UBO（per-world），阶段 5 之外单独评估。
6. **ReBAR 直写 → CS 写**：阶段 4 的 4-ID SSBO 现 CPU 写，未来 CS 写需同步机制（不阻塞本计划，仅预留形态）。

---

## 6. 现状关键文件清单（供拆小计划定位）

| 层 | 文件 | 关键行/符号 |
|---|---|---|
| 枚举 | `inc/hgl/graph/ssbo/SSBOTypes.h` | `SSBOType`(:10-28)、`IsMaterialSSBOType`(:37-50) |
| 池/域 | `inc/hgl/graph/module/SSBOBufferRegistry.h` | `AllocateArrayAccessor`(:145-188)、`AllocateSSBOId`(:120) |
| auth | `inc/hgl/ecs/components/PrimitiveComponent.h/.cpp` | `materialPrivateDataSlotResources`(:95)、`Set/Get/Clear`(.cpp:359-434) |
| recipe | `inc/hgl/mtl/MaterialRecipe.h` | `RecipeSSBOAssetBinding`(:38-47)、`ssbo_assets`(:280) |
| 解析缓存 | `inc/hgl/ecs/components/MaterialComponent.h` | `ResolvedSSBOBinding`(:21-24)、`resolved_ssbo_bindings`(:52) |
| 收集/物化 | `src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp` | `ResolveRecipeSSBOBindingId`(:75-87)、物化(:826-845)、行表填(:873-900) |
| 批/命令 | `src/ecs/support/PrimitiveBatchPipeline.cpp` | `WriteMeshDrawCommands`(MeshDrawParams 行) |
| 渲染器 | `src/ecs/support/PipelineMaterialRenderer.cpp` | push 4 表、indirect flush |
| 发射 | `src/ShaderGen/compile/MaterialShaderEmitter.cpp` | `MTL_ROW`(:188-190)、`MaterialDataAddressesRef`(:390)、`BuildMaterialSSBODeclarations`(:91-193) |
| 变换 | `inc/hgl/ecs/support/TransformAssignmentBuffer.h` | static/dynamic 段(:34)、`EnsureCapacity`(:74) |
| 根地址 | `inc/hgl/graph/ShaderBufferSources.h` | `RootAddresses` / `HGL_ROOT_ADDRESSES_FIELD_LIST` |
| 文本数据 | `src/ecs/support/text/TextRenderPipeline.cpp` | `texture_layer_buffer`(:415-419) |
