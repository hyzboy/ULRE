# 材质实例数据 SSBO Arena + BDA 重构技术方案

日期：2026-09
状态：方案定稿（待实施）
分支基线：SharedOneSSBO

---

## 1. 背景与目标

### 1.1 现状链路（为什么复杂）

以 `example/Basic/PBRSpheres.cpp` 为入口的现状：

- 每组材质实例由 `ResourceDomainManager::AllocateArrayAccessor<T>` 分配一块**独占 SSBO**，
  身份 = `{SSBOType, ssbo_id}`（`inc/hgl/graph/module/ResourceDomainManager.h:96-126`）；
- ShaderGen 按 `SSBOType` 发射 per-type GLSL struct + `set=2/binding=0` 的描述符
  （`src/ShaderGen/compile/MaterialShaderCompiler.cpp:119-135`、`inc/hgl/graph/ssbo/MaterialSSBOLayout.h`）；
- ECS 侧每批（MaterialBatch）逐 item 解析 ssbo_id、校验批内一致性、创建 per-batch
  MaterialParameters、绑定 set 2（`src/ecs/systems/render/RenderDescriptorBindingSystem.cpp:618-711,831-883`）；
- `data_index` 经 PerObject 集的 4B 行表 SSBO（`mtl_private_data_index`）路由进 GPU。

复杂度根源：**类型异构行距**（32/16/16/4B + UserDefined）导致"每类型每材质一块 buffer"，
`{type,id}` 双维身份系统、批 key 里的 SSBO 签名、per-batch 描述符集全是它的下游。

### 1.2 目标

1. 所有材质实例数据（不分类型）共用**一个预分配大 SSBO（arena）**，16B 块粒度管理；
2. 行表（原 `mtl_private_data_index`）改存 **64 位设备地址（BDA）**，shader 内
   `T(addr)` cast + 解引用，材质数据**零描述符**；
3. 渲染过程中**不再有逐材质/逐批的材质数据描述符绑定**；set 2（Material 集）整体消失；
4. ShaderGen 的材质 SSBO 发射从"描述符管线"收缩为"buffer_reference 声明"；
   类型尺寸自由（16B 整倍数即可），消除 C++/GLSL 行距双维护；
5. 保持 `AllocateArrayAccessor` API 兼容，21 个示例无感/低成本迁移。

### 1.3 非目标

- 不动 LocalToWorld / MeshDrawParams / 顶点数据 SSBO（它们本就是全局单例，无此问题）；
- 不改批处理/绘制排序的既有语义（合批率提升是附带收益，不作为验收项）；
- 不做 DEVICE_LOCAL + staging 上传优化（第一版 HOST_VISIBLE|HOST_COHERENT 直写）；
- 不做稀疏内存 / 多 arena（预留 3-bit arena id 扩展位，见 §10）。

---

## 2. 总体架构

```
                    CPU                                        GPU
┌─────────────────────────────────────────┐
│ TypedBlockAllocator<LitMaterialData> ... │   每类型一个，slot_blocks=ceil(sizeof(T)/16)
│         │ batch Acquire(16*slot_blocks)  │   LIFO free list，类型内复用不归还
│         ▼                                │
│      BlockPool  (0号块=null哨兵)          │   块号 ↔ 字节：base + index*16
│         │                                │
│  MappedArenaAllocator (AbstractMemory-   │
│  Allocator 适配器，CanRealloc=false)      │
└─────────┼────────────────────────────────┘
          │ 持久映射 (HOST_VISIBLE|CACHED|COHERENT)
          ▼
   ┌─────────────── MaterialDataArena：一块 128MB VkBuffer ───────────────┐
   │ block 0(零填充默认行) │ T@b1 │ ... │ T@bN │ ...        (16B 粒度)   │
   └──────────────────────────────────────────────────────────────────────┘
          │ vkGetBufferDeviceAddress → arena_base（会话恒定，永不 realloc）
          │
          │ 每帧 CPU 重写行表：rows[i] = arena_base + block_i*16
          ▼
   ┌ mtl_data_addrs（8B/行，PerObject 集 SSBO，每批一块，每帧重写）┐
   └────────────────────────────────────────────────────────────────┘
          │  FS: PBRSurfaceData(mtl_data_addrs.values[fragDataIndexID])
          ▼
   shader 内 BDA 解引用（buffer_reference_align=16, scalar layout）
   —— 材质数据不占用任何描述符，set 2 整体移除
```

关键不变量：

- **I1**：arena 永不 realloc、永不搬移 → `arena_base` 会话内恒定，已下发地址永不失效；
- **I2**：CPU 契约 `GetData(i) == base + i*16` 已被测试钉死
  （`CMCore/examples/datatype/TypedBlockAllocatorTest.cpp:155`）；
- **I3**：block index 进入 GPU 的**唯一通道**是每帧重写的地址行表
  → 块号复用无跨帧悬空（实体删除后行不再写）；
- **I4**：0 号块零填充 = "默认材质行"，地址 `arena_base+0` 永远可安全解引用，shader 无分支。

---

## 3. 核心机制规范

### 3.1 Arena 与块分配

- 容量：默认 **64MB**（可配置 16–512MB）。
  注意：BDA 寻址不经过描述符，`maxStorageBufferRange` 的 128MiB 规范下限**不再约束**本方案；
  容量只需考虑显存占用（HOST_VISIBLE 计入系统内存/共享内存）。
- 块大小：16B（`block_size`）。所有行结构 `sizeof(T) % 16 == 0` 由 static_assert 强制
  （现有 4 种类型中仅 TransmissionSurface 需 4B→16B 补齐）。
- 管理栈：`CMCore/inc/hgl/type/` 的 `BlockPool` / `BlockAllocator`(best-fit+合并) /
  `TypedBlockAllocator<T>` 原样使用（已有 4 万次随机交错压力测试）。
- 分配模式（同一块号空间，并存）：
  - **段分配**：`BlockPool::Acquire(count*slot_blocks)`，供 `AllocateArrayAccessor(name,count)`
    的"连续行"语义（示例模式）；
  - **逐对象分配**：`TypedBlockAllocator<T>::Acquire()`，供运行期动态增删材质实例。
- 必补守卫（见 §9-R2）：
  - `TypedBlockAllocator::Release` 加 double-release 检测与"块号属于本类型 batch 段"校验（debug 构造）；
  - `BlockPool` 头注释写明架构约束 I3（块号不得进入跨帧结构）。

### 3.2 GPU 内存适配器（新代码，~80 行）

实现 `AbstractMemoryAllocator`（`CMCore/inc/hgl/type/MemoryAllocator.h:9-57`）：

```cpp
class MappedArenaAllocator final : public AbstractMemoryAllocator
{
    DeviceBuffer *buffer;        // 非拥有；由 BufferManager::CreateSSBO 创建
    void *mapped;                // vkMapMemory 一次，会话内持久
    // Reserve(bytes)  → 校验 buffer 尺寸 >= bytes（不分配、不增长）
    // Get()/Get(off)  → mapped / mapped+off
    // Write(src,off,sz) → memcpy(mapped+off, src, sz)（HOST_COHERENT，无需 flush）
    // CanRealloc()    → false   ← 地址稳定性红线，钉死
};
```

- 内存属性：`HOST_VISIBLE | HOST_CACHED | HOST_COHERENT`（桌面全覆盖，构造期断言）；
  非 coherent 设备回退策略：第一版直接断言要求（目标硬件面内不存在）。
- arena buffer 创建参数：`BufferUsageFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR`；
  内存分配 flags 必须含 `VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT`。

### 3.3 设备特性（一次性，全部 inert 可先行合入）

`src/Vulkan/VKDeviceCreater.cpp`：

1. `vk12_features.bufferDeviceAddress = dev12.bufferDeviceAddress;`（:247-271 特性链内加一行）；
2. 核心 `VkPhysicalDeviceFeatures.shaderInt64 = ...`（行表 `uint64_t` 与 BDA 指针运算需要）；
3. 硬件要求项（require）注册：bufferDeviceAddress、shaderInt64 为 Must；
4. `vkGetBufferDeviceAddress` 在 arena 创建时调用一次，`arena_base` 缓存于 arena 对象。

引擎基线 Vulkan 1.4（`src/Vulkan/VKInstance.cpp:23`），以上均为 core 特性，无扩展谈判。

### 3.4 行结构契约（C++/GLSL 双侧）

每类型一行 = 材质数据 + **统一 10 槽纹理句柄尾**（`TextureSlot::RANGE_SIZE = 10`，
`inc/hgl/graph/ssbo/TextureSlot.h`），scalar layout 与 C++ POD 逐字节一致：

```cpp
// C++（hgl/graph/ssbo/ 下，与现有 LitMaterialData 同居）
struct PBRSurfaceRow          // 80B = 5 blocks
{
    Color4f base_color;       // 16
    float   metallic;         // 4
    float   roughness;        // 4
    float   normal_scale;     // 4
    float   fresnel;          // 4
    uint32  tex[10];          // 40  bindless 句柄，snake_case 名与 TextureSlot 一一对应
};
```

```glsl
// ShaderGen 发射（buffer_reference，非描述符）
layout(buffer_reference, scalar, buffer_reference_align=16)
buffer PBRSurfaceRow
{
    vec4  base_color;
    float metallic, roughness, normal_scale, fresnel;
    uint  base_color_, normal_, metallic_, roughness_, emissive_,
          occlusion_, opacity_mask_, height_, custom0_, custom1_;
    // 字段名生成规则见 §6.4：纹理句柄以 tex_<slot_name> 命名，避免与数据字段冲突
};
```

- 纹理句柄并入行内 ⇒ 原 `mtl_texture_layer_rows`（Material 集独立 SSBO）**整体移除**，
  其每实例行数据改写到行尾 `tex[]`（CPU 侧写入点与今天 TextureLayer 行写入相同：
  `RenderPrimitiveCollectSystem` 材质行物化处）；
- 各类型行大小：PBRSurfaceRow 80B(5 块)、EmissiveSurfaceRow 64B(4 块)、
  TextureRectArrayRow 64B(4 块)、TransmissionRow 16B(1 块)；UserDefined ≤ 行尾自定；
- `MaterialSSBOLayout.h` 的表保留，但条目从"GLSL struct 字符串"升级为
  "C++ 行结构 + GLSL 行声明"双侧同源（建议以 C++ 结构 + 生成/镜像断言消除双维护，
  static_assert(sizeof(Row) % 16 == 0)）。

### 3.5 Shader 寻址模型（BDA 变体 B：行表存完整地址）

行表（新 `SBS_MaterialDataAddresses`）：

```cpp
// inc/hgl/graph/ShaderBufferSources.h 新增，替代 SBS_MaterialPrivateDataIndexRows
constexpr const ShaderBufferSource SBS_MaterialDataAddresses{
    DescriptorSetType::PerObject, "mtl_data_addrs", "MaterialDataAddresses"
};  // 仅 Fragment 阶段可见
```

```glsl
// ShaderGen 发射
layout(set=1, binding=N) readonly buffer MaterialDataAddresses { uint64_t values[]; } mtl_data_addrs;

// 材质源接入宏（MaterialShaderEmitter 生成，材质模块只见宏）
#define MTL_ROW(i) <RowType>(mtl_data_addrs.values[(i)])
```

材质源改动模式（每个模块 1–3 行）：

```glsl
// 旧：const PBRSurfaceData material_data = MTL_DATA.data[source_input.dataIndex];
//     ... mtl_texture_layer_rows.data[i].base_color
// 新：
const PBRSurfaceRow m = MTL_ROW(source_input.dataIndex);   // 指针
vec3 base = m->base_color.rgb;
uint base_color_handle = m->tex_base_color;                // 句柄行内直读
```

**varying 语义变化**：`fragDataIndexID`（`ShaderSemanticRegistry.cpp:24`，flat uint，
保持形状不改）的值从"已解析的数据行号"变为"**batch 内 item 序号**"（即地址行表下标）。
`ResolveMaterialPrivateDataIndex` 删除；mesh 模板改为直接赋值：

```glsl
// MeshShaderModeVertexPassthrough.h:74 / LineQuad.h:55 / CharQuad.h:67
// 旧：fragDataIndexID[v] = ResolveMaterialPrivateDataIndex(gl_InstanceIndex);
// 新：fragDataIndexID[v] = gl_InstanceIndex;     //（CharQuad 保留 gl_DrawID 变体）
```

地址表 SSBO 只需 Fragment 可见性（地址解析全部在 FS）。

### 3.6 数据流

初始化（一次）：

```
CreateSSBO("MaterialDataArena", 64MB, HOST_VISIBLE|CACHED|COHERENT,
           usage|=SHADER_DEVICE_ADDRESS)
→ alloc flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
→ vkMapMemory（持久） → vkGetBufferDeviceAddress → arena_base
→ BlockPool::Init(MappedArenaAllocator, 64MB/16=4M 块, 16)
→ memset(block0, 0, 16)  // 默认行 I4
```

每帧（RenderCollect → FrameSync → DrawSubmit）：

```
Collect:  material_comp->data_index_values[0] = block_index（语义改为块号）
Batch:    EnsureBatchIndexRows → 8B/行（容量算式更新）
          WriteBatchIndexRows: rows[i] = arena_base + block_i*16
FrameSync: RDBS 绑 mtl_data_addrs（PerObject 集，行表 buffer→GPUBuffer）
           （材质数据本体：无任何绑定动作）
Draw:     set0 全局 / set1 行表 / set3 bindless；set2 不存在
          FS: MTL_ROW(fragDataIndexID)->…
```

材质数据更新（动画材质）：CPU 直写 arena 行（coherent），**必须在 vkQueueSubmit 之前**
完成——与今天 `SSBOArrayAccessor::Commit` 的时序约束一致，无新增风险。

---

## 4. 各层改造明细（含代码锚点）

### 4.1 设备/内存层

| 文件 | 改动 |
|---|---|
| `src/Vulkan/VKDeviceCreater.cpp:247-271` | vk12 特性链 +bufferDeviceAddress；核心特性 +shaderInt64；require 项登记 |
| `BufferManager::CreateSSBO` 调用点（arena 专用工厂） | 新 `CreateArenaBuffer(name, bytes)`：usage/alloc flag/映射/取地址一步完成 |
| 新文件 `inc/hgl/graph/module/MaterialDataArena.h` + `src/SceneGraph/module/MaterialDataArena.cpp` | arena 单例：持有 buffer、mapped、arena_base、BlockPool；暴露 `GetBlockBase()`、`TypedBlockAllocator<T>* GetAllocator<T>()`（懒建）、`AddressOf(block)` |

### 4.2 分配器层（CMCore）

- `BlockPool` / `BlockAllocator` / `TypedBlockAllocator`：**不改接口**；
- `TypedBlockAllocator::Release`：+debug 校验（free list 查重 + 块号归属检查）；
- 测试补三项（§8.2）。

### 4.3 资源域与访问器（API 兼容层）

| 文件 | 改动 |
|---|---|
| `ResourceDomainManager.h:96-126` `AllocateArrayAccessor` | 后端切换：`EnsureBuffer` 段 → `arena->AcquireRange<T>(count)`（底层 `BlockPool::Acquire(count*slot_blocks)`，连续语义保持）；不再创建独立 DeviceBuffer |
| `SSBOArrayAccessor.h` | 行距与 `sizeof(T)` 解耦：`element_stride = slot_blocks*16`，`operator[] = mapped + idx*stride`；`Commit()` 保留为 no-op（coherent 直写，API 兼容/未来 device-local 复用）；`GetSSBOId/GetSSBOBinding` 标记 deprecated（返回 arena 常量），新增 `GetBlockBase()` |
| `ResourceDomainManager.cpp:145-224` | 材质类 domain（`IsMaterialSSBOType` 为真者）不再走 domain_map；`ValidateStructStrideForDomain` 对材质类型改为行结构注册断言 |
| `SSBOTypes.h:134-161` | `MakeRecipeSSBOId/MakeECSSSBOId/IsECSSSBOId` 的**材质数据用途**作废（LocalToWorld 保留段仍用）；注释标注 |
| `MaterialRecipe.h:38-47,391-444` | `RecipeSSBOAssetBinding`/`UpsertRecipeSSBOAssetBinding` deprecated：材质数据不再需要 recipe 级 SSBO 身份（struct 类型由 MaterialDefinition 决定，行位置由组件携带）。迁移期保留为 no-op，M3 后删除 |
| `PrimitiveComponent.cpp:359-386` | `MaterialPrivateDataSlotAuthoringResource.data_index` 语义 = block index（字段名不变，注释更新） |

### 4.4 ShaderGen

| 文件 | 改动 |
|---|---|
| `MaterialSSBOLayout.h` | 条目升级为 §3.4 的行结构（C++/GLSL 双侧 + sizeof 断言） |
| `MaterialShaderEmitter.cpp:120-169` | `BuildMaterialSSBODeclarations` 重写：发射 `buffer_reference` 行声明 + `MTL_ROW` 宏；**不再**发射 `readonly buffer …data[]`/`#define MTL_DATA`/set+binding |
| `MaterialShaderEmitter.cpp:281-309` | `mtl_private_data_index` 声明与 `ResolveMaterialPrivateDataIndex` 删除 → `mtl_data_addrs`（uint64 行表）声明 |
| `MaterialShaderEmitter.cpp:323-356` | `BuildFSIndexTableDecls` 的 `mtl_texture_layer_rows` 注入删除（句柄已行内） |
| `ShaderBuildContext.cpp:176-191` | `AddSSBOMaterialPrivateData` 删除；`AddSSBOMaterialPrivateDataIndex` → `AddSSBOMaterialDataAddresses`（Fragment only） |
| `MaterialShaderCompiler.cpp:119-135` | `AddMaterialPrivateDataSlotDescriptor` → `AddMaterialRowReference`（只加 struct 声明，无描述符） |
| `MaterialShaderCompiler.cpp:192-228` | 能力规则表删 `MaterialPrivateData`/`MaterialTextureLayerTable`/`MaterialPrivateDataIndex` 三行（改为无条件行声明 + 地址表常驻） |
| `DescriptorContract.cpp:73-83` | MaterialPrivateData/TextureLayer/Index 的规范化与契约条目删除 |
| `DescriptorBuilderCommon.h:395-410` | `EnsureMaterialPrivateDataIndexTable` 删除 |
| `ShaderBufferSources.h:15-20` | `SBS_MaterialTextureLayerRows` 删除；`SBS_MaterialPrivateDataIndexRows` → `SBS_MaterialDataAddresses` |
| `DescriptorSetTypeDef.h:93` | `DescriptorSetType::Material` 枚举值**保留**（序列化契约兼容）但停止发射；pipeline layout 不再含 set 2 |
| `MeshShaderModeVertexPassthrough.h:74`、`LineQuad.h:55-57`、`CharQuad.h:67-69` | varying 赋值改为直传 item 序号（§3.5） |
| `Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` | golden 期望整体更新（`:3760` 处 `#define MTL_DATA …` 断言替换为 buffer_reference 断言；set2 布局断言删除） |
| **shader artifact 缓存** | **必须核查并提升缓存版本/确认 link 哈希覆盖 GLSL 内容**（`MaterialShaderCompiler.cpp:43-53` 加载路径）——GLSL 已变而 SPV 缓存未失效是本重构最隐蔽的翻车点 |

### 4.5 ShaderLibrary（材质模块，改动模式统一）

当前 `MTL_DATA` 使用者 6 个（迁移时以 grep 复核）：
`material/pbr_surface_source.glsl`、`pbr_texturearray_source.glsl`、`texture_array_source.glsl`、
`debug_normal_source.glsl`、`luminance_source.glsl`、`unlit_source.glsl`。

每个模块：数据行取用 1 行（`MTL_DATA.data[i]` → `MTL_ROW(i)` 指针）+ 纹理句柄行
（`mtl_texture_layer_rows.data[i].<slot>` → `m->tex_<slot>`）若干行；
`@ulre ssbo mtl_private_data …` 注解头更新为 `@ulre row_ref <Type>` 之类（manifest 解析同步）。
`common/material_source_interface.glsl` 的 `MaterialSourceInput.dataIndex` 字段名不变（语义=item 序号）。

### 4.6 ECS

| 文件 | 改动 |
|---|---|
| `MaterialComponent.cpp:50-78` | `resolved_ssbo_bindings` 机制删除；`data_index_values[0]` 语义改 block index |
| `RenderPrimitiveCollectSystem.cpp:771,814-935` | `MaterializeRecipeRowsForPrimitive` 大幅缩水：不再解析 ssbo_id/合成 scope id（`:879-884` MakeECSSSBOId 兜底删除）；`:889-893` 纹理行表 resolved binding 删除；改为登记 block_index；纹理句柄写入行尾 `tex[]` |
| `RenderPrimitiveCollectSystem.cpp:647-660` | `RegisterMaterialStructLayout` → arena 行结构注册（一次性） |
| `PrimitiveBatchPipeline.cpp:630-689` | `EnsureBatchIndexRows`：行宽 4B→8B，容量算式 ×`sizeof(uint64)` |
| `PrimitiveBatchPipeline.cpp:691-750` | `WriteBatchIndexRows`：`rows[i] = arena->AddressOf(material_comp->data_index_values[0])`（0 号哨兵块同样映射为 base+0，合法地址） |
| `PrimitiveBatchPipeline.cpp:807-809` | 批 key 删除 SSBO 绑定签名（合批率提升的来源） |
| `RenderDescriptorBindingSystem.cpp:618-711` | `resolve_recipe_batch_struct_ssbo_id` 整段删除 |
| `RenderDescriptorBindingSystem.cpp:831-883` | MaterialPrivateData 与 MaterialTextureLayerTable 两分支删除 |
| `RenderDescriptorBindingSystem.cpp:898-928` | MaterialPrivateDataIndex 分支改绑 `mtl_data_addrs`（行表 GPUBuffer） |
| `RenderDescriptorBindingSystem.cpp:538-570` | `ensure_batch_mp` 对 set 2 退役（set 2 已不存在） |
| `PipelineMaterialRenderer.cpp:182-223,384-403` | set 2 批次覆盖路径删除；set 1 重绑逻辑保留（改绑地址行表） |
| `PrimitiveRenderSystem.cpp` / `VKCommandBufferRender.cpp:281-311` | 无结构性改动（set 2 消失后逐集绑定循环自然少一次） |

---

## 5. 迁移策略：双路径 + 四里程碑

材质数据的 GLSL 发射、行表格式、ECS 解析必须同切，无法独立灰度；
故采用**编译期/运行时双路径 flag**（`ULRE_MATERIAL_ARENA_BDA`），旧路径完整保留至 M3 末删除。

### M1 基础设施（可独立合入，inert）

设备特性（§3.3）、`MaterialDataArena` + `MappedArenaAllocator`、
`TypedBlockAllocator` debug 守卫、`BlockPool` 三项补测、arena 单元测试。
验收：单测全绿；现有渲染路径行为零变化（特性启用无副作用）。

### M2 双路径切通（核心攻关）

ShaderGen buffer_reference 发射（flag 后）、地址行表、ECS 并行路径（flag 后）、
`AllocateArrayAccessor` 后端切换（flag 后走 arena 段分配）。
验收：flag 开启时 `SimpleCube`（最简）与 `PBRSpheres`（100 实例×PBR+纹理数组）
渲染结果与旧路径逐像素一致（截图对比）；flag 关闭时全部示例不变。

### M3 迁移与删除

21 个示例切换 flag 并验证（`UpsertRecipeSSBOAssetBinding` 调用点删除，
authoring 的 `data_index` 数值来源不变）；回归门 golden 全面更新；
确认后删除旧路径：`ResolveMaterialPrivateDataIndex`、`mtl_private_data`/
`mtl_texture_layer_rows` 发射与绑定分支、`resolved_ssbo_bindings`、
`ensure_batch_mp` set2、材质域的 `EnsureBuffer` copy-on-grow 路径、
`UpsertRecipeSSBOAssetBinding`（deprecated no-op 一并删）。
验收：全示例可视验证 + 回归门全绿 + RDBS/批处理代码净删量达到预期（§7）。

### M4 清理与文档

`DescriptorSetType::Material` 枚举标 deprecated 注释、`SSBOTypes.h` ID 命名空间注释、
`SSBOArrayAccessor` deprecated 接口移除、本方案文档归档更新。

### 回滚

M1/M2 期间任意时刻关 flag 即回旧路径；M3 删除前保持双路径至少一个里程碑周期。

---

## 6. 关键设计决策记录（已定）

| # | 决策 | 备选与否决理由 |
|---|---|---|
| D1 | BDA 直上，不做"无 BDA 统一胖行"中间态 | shader 内字节寻址必然改 GLSL，无 BDA 仅剩 uvec4 手工解包（布局双维护，恰是要消灭的）；arena 设计中和了 BDA 三大代价（特性/flag/地址稳定性） |
| D2 | 行表存**完整 64B 地址**（变体 B），非"4B 块号+全局 UBO 基址"（变体 A） | B 零 shader 算术、零基址管线；乘加在 CPU 每帧重写行表时顺带完成；行表 8B/行可忽略 |
| D3 | 纹理句柄并入行结构尾（10×uint32） | 消灭 set 2 最后一个描述符与 `mtl_texture_layer_rows` 整套机制；句柄本就是 per-instance 数据 |
| D4 | `fragDataIndexID` varying 保持 uint 形状，语义改为 item 序号，地址解析移 FS | 64 位 varying（uvec2 拆分）触碰面更大；FS 一次依赖加载可忽略 |
| D5 | `AllocateArrayAccessor` API 不变，后端换 arena 段分配 | 21 示例兼容是硬约束；连续性语义由 `BlockPool::Acquire(count*slot_blocks)` 保证 |
| D6 | `DescriptorSetType::Material` 枚举保留不删 | set_type 参与序列化契约（DescriptorContract 哈希），删值破坏兼容 |
| D7 | 默认 64MB 可配置；0 号块 = 零填充默认行 | BDA 不受 maxStorageBufferRange 约束；零行保证地址永非空、shader 无分支 |
| D8 | HOST_VISIBLE\|HOST_CACHED\|HOST_COHERENT 持久映射 | 材质数据读放量极小，PCIe 非瓶颈；写路径零拷贝零 flush；device-local+staging 留待有实测需求 |

### 实现期待定项（不阻塞开工）

- arena 默认容量最终值（16/64/128MB，取示例+场景实测量级）；
- `MTL_ROW` 宏 vs 生成函数（倾向宏，与现有 `#define MTL_DATA` 风格一致）；
- 纹理句柄字段命名 `tex_<slot_name>` 是否需要冲突规避的最终拼写；
- `@ulre` 注解头新语法（`@ulre row_ref`）与 manifest 解析的具体形式。

---

## 7. 预期收益（可量化验收）

| 指标 | 现状 | 重构后 |
|---|---|---|
| 材质数据描述符 | 每批 2 个（data + texture_layer）+ per-batch MaterialParameters | **0**（BDA，零描述符） |
| 每帧 vkUpdateDescriptorSets 中材质数据次数 | 批数 × 2 | 0 |
| pipeline layout set 数 | 5（Scene/PerObject/Material/Bindless/Vertex） | 4 |
| RDBS 材质数据分支代码 | ~250 行（618-711 + 831-883） | ~10 行（行表绑定） |
| 批 key 维度 | program+pipeline+SSBO 签名 | program+pipeline（合批率↑） |
| 新类型接入成本 | SSBOTypes.h stride 表 + MaterialSSBOLayout.h GLSL 表 + CPU 结构三处同步 | C++ 行结构一处（GLSL 声明同源生成/镜像断言） |
| ShaderGen 材质 SSBO 发射路径 | 描述符 allocator + set/binding + schema 槽位 + 能力规则 | 纯 struct 声明（~30 行发射器） |

---

## 8. 测试计划

### 8.1 单元（CMCore，已有 + 补）

- 已有：`TypedBlockAllocatorTest.cpp` 5 组（含 4 万次交错压力、
  `GetData==base+i*16` 寻址不变量断言 `:155`）；
- 补 1：ReleaseAll 后 `pool.Acquire(free_count)` 返回 1（**完全合并**结构性断言，
  覆盖 `BlockAllocator.cpp:139-250` 四条合并分支）；
- 补 2：类型级 double-release / 越权 Release 守卫（守卫实现后）；
- 补 3：碎片楔死——大 slot_blocks 交错后超大连续 Acquire 干净返回 0。

### 8.2 新增单元（引擎侧）

- `MappedArenaAllocator`：Reserve 边界、CanRealloc=false、Write/Get 偏移正确性；
- `MaterialDataArena`：0 号块零填充、`AddressOf(block) == arena_base + block*16`（与
  CPU `GetData` 同一不变量的 GPU 侧镜像）、耗尽返回 0 哨兵；
- `WriteBatchIndexRows`：行值 = 期望地址（mock arena_base）。

### 8.3 回归与可视

- `ShaderResourceSchemaRegressionGate`：buffer_reference 发射、set2 缺席、
  `MTL_ROW` 宏、地址表声明、mesh 模板 varying 直传——全部 golden 化；
- 可视基线：M2 起 `SimpleCube`/`PBRSpheres` 新旧路径截图逐像素对比；
  M3 全 21 示例走查（重点：TextureBlinnPhongMeshes、AutoMergeMaterialInstance、
  SingleSphereMaterialSwitch、Environment 组——覆盖纹理切换/材质切换路径）；
- shader 缓存验证：旧缓存目录存在时启动，确认重新编译（防 §4.4 缓存失效遗漏）。

---

## 9. 风险登记册

| # | 风险 | 等级 | 缓解 |
|---|---|---|---|
| R1 | SPV artifact 缓存未随 GLSL 变化失效 → 加载旧 shader 混跑 | 高（隐蔽） | M2 第一项即核查缓存键（`MaterialShaderCompiler.cpp:43-53`），必要时全局缓存版本 +1；测试计划 8.3 含专项 |
| R2 | 块号 double-release / 越权释放 → 静默数据串扰 | 高 | debug 守卫（§3.1）+ 补测 2；release 构造下首月可加运行时计数器 |
| R3 | 块号进入跨帧结构 → 复用悬空 | 中 | 架构约束 I3 写入 BlockPool 注释 + code review 检查项；行表每帧重写为唯一通道 |
| R4 | 双路径期间两套发射逻辑漂移 | 中 | flag 硬隔离在同一发射函数的分枝，M3 周期限时（一个里程碑内完成迁移删除） |
| R5 | 旧 Intel iGPU（Gen11-）无 BDA | 低（目标硬件面外） | require 项 Must 拒绝启动并列明缺失特性；不提供 fallback（D1 已否决无 BDA 路线） |
| R6 | HOST_VISIBLE 内存压力（64MB 常驻） | 低 | 容量可配置；实测可降至 16MB（100 万实例×80B 才 80MB，示例量级远小） |
| R7 | 多帧 in-flight 期间 CPU 改写 arena 行 | 低 | 与现状 Commit 时序约束一致（提交前写）；文档化，不改机制 |
| R8 | `scalar` layout 下 C++/GLSL 行结构漂移 | 中 | static_assert(sizeof%16) + 布局镜像断言 + 回归门 golden；C++ 结构为唯一真源 |

---

## 10. 未来扩展（本方案不做，但结构已预留）

- **多 arena**：块号 32 位中划 3 bit 作 arena id（8×512MB），CPU 侧
  `AddressOf` 按 id 取对应 base；shader 侧行表仍存完整地址，零改动；
- **行表描述符消灭**：行表地址经全局 UBO 下发 + 行表本体 BDA 寻址 →
  set 1 材质相关绑定归零（GPU-driven 渲染的前置）；
- **device-local arena**：`Write(offset,size)` 接口已支持按块 staging 上传，
  需要时加 dirty 区间合并即可；
- **ray tracing / 指针追逐**：行结构可含嵌套地址（材质图、图层链），
  buffer_reference 语法天然支持。

---

## 附录 A：废弃/语义变更概念对照表

| 概念 | 现位置 | 重构后 |
|---|---|---|
| `SSBOArrayAccessor::GetSSBOId/GetSSBOBinding` | SSBOArrayAccessor.h:177-190 | deprecated，返回 arena 常量，M4 删 |
| `UpsertRecipeSSBOAssetBinding` / `RecipeSSBOAssetBinding` | MaterialRecipe.h:38-47,391-444 | no-op deprecated，M3 删（示例调用点同步删） |
| `MakeRecipeSSBOId` / `MakeECSSSBOId`（材质数据用途） | SSBOTypes.h:134-161 | 作废；ECS 保留段（LocalToWorld）仍用 |
| `ResourceDomainManager` 材质域 / copy-on-grow | ResourceDomainManager.cpp:145-224 | 材质类型不再入域；机制保留给非材质域 |
| `ResolveMaterialPrivateDataIndex` | MaterialShaderEmitter.cpp:281-309 | 删除（mesh 模板直传 item 序号） |
| `mtl_private_data` / `MTL_DATA` / `mtl_texture_layer_rows` | 发射+6 个 GLSL 模块 | 删除；`MTL_ROW(i)` + 行内 `tex_<slot>` 取代 |
| `SBS_MaterialPrivateDataIndexRows` | ShaderBufferSources.h:18-20 | → `SBS_MaterialDataAddresses`（8B 地址） |
| `MaterialPrivateDataSlotAuthoringResource.data_index` | PrimitiveComponent.h:66-79 | 字段名不变，语义 = block index |
| `fragDataIndexID` | ShaderSemanticRegistry.cpp:24 | 形状不变，值语义 = batch item 序号 |
| `DescriptorSetType::Material` | DescriptorSetTypeDef.h:93 | 枚举保留（契约兼容），不再发射 |

## 附录 B：工作量估算（单人当量）

| 里程碑 | 内容 | 估算 |
|---|---|---|
| M1 | 特性+arena+适配器+守卫+单测 | 3–4 天 |
| M2 | 双路径发射+行表+ECS 并行路径+SimpleCube/PBRSpheres 对拍 | 4–6 天 |
| M3 | 21 示例迁移+回归门+旧路径删除 | 4–5 天 |
| M4 | 清理+文档 | 1 天 |
| 合计 | | **12–16 天** |

风险余量集中在 M2 的 ShaderGen 契约连锁（R1/R8）；分配器环节（此前最大不确定点）
已由 CMCore 现有实现与测试消除。
