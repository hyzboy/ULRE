# 全局图元描述符池与二级绘制索引管线设计规范
# (Global RenderItem Descriptor Pool & Indirection Pipeline Architecture)

> **文档版本**：v2.0 (2026-09)  
> **设计定位**：ULRE 渲染管线与 ECS 渲染组件的终极演进总纲。  
> **核心思想**：彻底摒弃为了 DOD 在 CPU 端无谓堆砌细碎组件（`GeometryComponent`/`MaterialComponent`）的伪 ECS 模式；确立**「显存常驻一级描述符表（16B 4-ID） + 绘制期极简二级索引表（uint32_t）与连号直通优化」**的现代 GPU-Driven 工业级架构。

---

## 1. 设计背景与核心痛点

### 1.1 现状痛点
1. **CPU 端过度包装与多重分配**：
   若为纯 ID 架构而将几何、材质拆成独立的 ECS 组件，一个可渲染实体在 CPU 侧需要进行 3~4 次堆内存分配（`TransformComponent`、`GeometryComponent`、`MaterialComponent`），并在 Entity 中注册 4 个组件哈希查找槽。这违背了数据导向设计（DOD）的高内聚与缓存友好初衷。
2. **每帧重复写入与带宽浪费**：
   原有合批管线（`PrimitiveBatchPipeline`）在每帧绘制时，由 CPU 临时拼接 `MaterialInstanceAddresses`（16B 双指针）以及各类运行时索引，即使场景静态无变化，每帧依然在 CPU 组织庞大数据并上传 GPU。
3. **多实例（Instancing）与单体割裂**：
   单体绘制（Single Primitive）与海量实例（Instanced Primitive）在数据组织与管线上存在双轨分歧，未能实现概念层面的绝对统一。

---

## 2. 总体架构：两级表（Two-Level Indirection）与连号直通

将整个渲染体系的核心抽象为：**一级持久化槽位表** 与 **二级每帧提交表**。

```
                    ┌────────────────────────────────────────────────────────┐
                    │  一级表：全局持久图元描述符表 (Global RenderItem Buffer)   │
                    │  SSBO 常驻显存，每个图元占 16 字节 (RenderItemDescriptor) │
                    └────────────────────────────────────────────────────────┘
                                    ▲                      ▲
           [连号直通: firstInstance=0] │                      │ [二级索引寻址]
                                    │                      │
       ┌────────────────────────────┴─┐          ┌─────────┴────────────────┐
       │   连续静态块 / 实例化簇      │          │   散落动态物体 / 离散图元  │
       │   Draw: [first=0, count=100] │          │   DrawItemIDBuffer (SSBO)│
       │   (零每帧数据传输，零额外开销)│          │   [ 102, 105, 301, 402 ] │
       └──────────────────────────────┘          └──────────────────────────┘
```

---

## 3. 一级表：全局持久图元描述符（`GlobalRenderItemBuffer`）

### 3.1 内存布局（16 字节对齐）
每个渲染对象（无论是单体还是实例的一个槽位）在全局只对应一个 16 字节的 `RenderItemDescriptor`：

```cpp
#pragma pack(push, 4)
struct RenderItemDescriptor
{
    uint32_t transform_id;    // ID 1: L2W 变换矩阵索引 (TransformDataStorage SSBO)
    uint32_t geometry_id;     // ID 2: 几何与绘制参数索引 (MeshDrawParams SSBO)
    uint32_t material_id;     // ID 3: 材质实例参数行索引 (MaterialData SSBO)
    uint32_t texture_id;      // ID 4: 材质纹理引用表索引 (TextureRef SSBO)
};
#pragma pack(pop)
static_assert(sizeof(RenderItemDescriptor) == 16, "RenderItemDescriptor must be exactly 16 bytes");
```

在 GLSL / 着色器中直接以 `uvec4` 解释：
```glsl
layout(buffer_reference, scalar) readonly buffer GlobalRenderItemBuffer {
    uvec4 items[]; // x: transform_id, y: geometry_id, z: material_id, w: texture_id
};
```

### 3.2 生命周期与管理（`RenderItemDataStorage`）
- 由 `ECSContext` 或全局渲染系统拥有 `RenderItemDataStorage`。
- **静态槽位分配**：
  - 实体创建时，申请一个唯一的 `RenderItemHandle`（`uint32_t` 槽位索引）。
  - **对象不动，显存不动**：只要物体的几何与材质不变，显存中该 16 字节始终常驻，无需每帧重新打包传输。
- **极致的局部增量修改**：
  - **换材质**：直接原地修改槽位的 `material_id` 与 `texture_id`，一帧内增量同步至 GPU。
  - **换几何体**：直接原地修改槽位的 `geometry_id`。
  - **动态位移**：由 `TransformSystem` 仅刷新其对应的 `transform_id` 矩阵槽，图元描述符本身无需修改。

---

## 4. 二级表：绘制索引表（`DrawItemIDBuffer`）与连号直通优化

每帧 CPU（或 GPU Compute 剔除器）合批时，**不再上传复杂的数据结构，仅处理纯整数索引**。

### 4.1 连号直通优化（Run-Length Merge / Contiguous Slice）——零开销绘制
对于连续生成的静态场景、大批同材质物体或多实例集合，其 `RenderItemHandle` 在全局池中分配时是连续连号的（区间 `[start_handle, count]`）：
- **无需分配与写入二级索引表！**
- 利用 Vulkan 绘制指令原生属性：
  ```cpp
  vkCmdDrawIndexed(cmd, indexCount, count, firstIndex, vertexOffset, /*firstInstance=*/start_handle);
  ```
- 着色器直接通过 `gl_InstanceIndex` 寻址一级表：
  ```glsl
  uint item_id = gl_InstanceIndex;
  uvec4 desc = render_item_buffer.items[item_id];
  ```
- **收益**：每帧 CPU 往 GPU 传输的数据量为 **0 字节**，合批开销降至理论极限。

### 4.2 离散散落图元：极简二级索引（`DrawItemIDBuffer`）
对于经过视锥剔除后断断续续的物体、或非连续分布的动态物体：
- CPU 合批时仅需将 `RenderItemHandle` 汇集为一个极其紧凑的纯整数流：`uint32_t draw_item_ids[]`。
- 上传至暂态环形 SSBO（`DrawItemIDBuffer`）。
- 着色器通过二级索引寻址：
  ```glsl
  uint item_id = draw_item_id_buffer.ids[gl_DrawID]; // 或 gl_InstanceIndex
  uvec4 desc   = render_item_buffer.items[item_id];
  ```

---

## 5. ECS 层设计：杜绝空壳组件，回归极致精简

### 5.1 实体的极简形态
实体（Entity）仅代表业务身份与层级，渲染只由一个组件负责：

```cpp
class Entity
{
    // 空间变换
    TransformComponent* transform;      // 获得 transform_id

    // 绘制表达（纯句柄载体）
    PrimitiveComponent* primitive;      // 持有 RenderItemHandle
};
```

### 5.2 创作层与底层的映射
- 用户/开发者依然使用熟悉的直观 API：
  ```cpp
  prim_comp->SetPrimitiveAsset(asset);
  prim_comp->SetMaterialTextureResource(...);
  ```
- `PrimitiveComponent` 内部：
  - 在附着到场景时，向 `RenderItemDataStorage` 申请一个 `RenderItemHandle`。
  - 将材质解析后的 `material_id`、纹理分配的 `texture_id`、几何体池化的 `geometry_id` 以及实体的 `transform_id` 自动写入全局槽位。
  - 外部不需要理解底层复杂的 SSBO 细节。

### 5.3 多实例（`InstancedPrimitiveComponent`）的自然统一
- 静态/同构实例组：向 `RenderItemDataStorage` 申请一段连续的 Handle 区间 `[base_handle, instance_count]`。
- 100% GPU-Driven 模式（如 100 万陨星带）：直接由 Compute Shader 输出的外部 GPU BDA 缓冲作为描述符数据源，与统一管线完全无缝互通。

---

## 6. 着色器全局集成方案

在全局 UBO（如 `SceneBinding` / `DrawInfoBinding`）中注入全局基址：

```glsl
layout(set = 0, binding = 0, std140) uniform SceneBinding
{
    uint64_t addr_global_render_items;   // 全局一级图元描述符表基址 (16B uvec4)
    uint64_t addr_draw_item_ids;         // 当前帧二级绘制索引表基址 (uint32_t，可选)
    uint64_t addr_l2w_matrices;          // L2W Transform 矩阵表基址 (mat4)
    uint64_t addr_mesh_draw_params;      // MeshDrawParams 表基址 (112B)
    uint64_t addr_material_data;         // 材质属性数据行基址
    uint64_t addr_texture_references;    // 材质纹理引用表基址
};
```

任何着色器阶段只需 2 行代码即可完整解析绘制环境：
```glsl
// 1. 获取当前图元描述符 (支持连号直接取 或 查二级表)
uvec4 desc = ResolveRenderItem(draw_index);

// 2. 依据 4-ID 展开所有资源
mat4 l2w_matrix          = l2w_buffer.mats[desc.x];
MeshDrawParams mesh_draw = mesh_params_buffer.params[desc.y];
// Fragment Shader 中：
MaterialData mtl_row     = material_buffer.rows[desc.z];
uint texture_index       = texture_ref_buffer.refs[desc.w].descriptor_index;
```

---

## 7. 精细化演进与落地实施路线图 (Detailed Implementation Roadmap)

为确保整个重构过程**低风险、步步可验证、且全程不中断现有业务与示例渲染**，将方案细化为 6 个阶段共 22 个可量化子任务：

---

### 阶段一：基础设施构建——描述符池与显存同步器 (`RenderItemDataStorage`) 【✅ 已完成】
> **核心目标**：建立纯粹的底层数据结构与显存同步通路，不触动任何上层 ECS 与管线逻辑。

- **1.1 定义标准描述符结构 (`RenderItemDescriptor`)** 【✅ 已完成】
  - 在 `inc/hgl/graph/render/RenderItemDescriptor.h` 中定义 16 字节对齐 POD 结构：
    `struct RenderItemDescriptor { uint32_t transform_id, geometry_id, material_id, texture_id; };`
  - 静态断言 `sizeof == 16` 与标准内存对齐，配套提供与 GLSL `uvec4` 的 1:1 结构映射。
- **1.2 实现槽位分配器与空闲链表 (`RenderItemDataStorage::Allocator`)** 【✅ 已完成】
  - 采用无 STL 的 HGL 数组（`hgl::ValueArray` / `hgl::ArrayList`）。
  - 支持单槽分配与回收（基于 `FreeList` O(1) 槽位重用）。
  - **连续块分配 (`AllocateContiguous(count)`)**：为静态网格簇或实例化群体预留连续物理槽位区间，为后续连号直通奠定基础。
- **1.3 实现增量脏标记与 GPU 镜像同步机制** 【✅ 已完成】
  - 维护 `dirty_range` 或脏页位图（BitMap），记录自上一帧以来被修改的描述符槽位。
  - 支持增量更新模式：仅将发生变动的槽位段通过 Staging Buffer 或环形动态缓冲刷入显存中的 `GlobalRenderItemBuffer` SSBO。
- **1.4 单元测试与独立验证** 【✅ 已完成】
  - 编写独立的存储器验证逻辑：模拟单槽申请/释放、连续块分配、局部属性修改以及环形同步，验证无内存泄漏、无越界、句柄稳定可复用。

---

### 阶段二：ECS 组件接入——`PrimitiveComponent` 槽位化与 4-ID 自动化登记 【✅ 已完成】
> **核心目标**：将 `PrimitiveComponent` 改造为持有全局 Handle，上层接口保持 100% 兼容。

- **2.1 引入 `RenderItemHandle` 成员与生命周期管理** 【✅ 已完成】
  - 在 `PrimitiveComponent` 中添加 `RenderItemHandle render_item_handle = INVALID_HANDLE;`。
  - 在 `OnAttach()` 时向关联 Context 的 `RenderItemDataStorage` 申请槽位；在 `OnDetach()` 或析构时释放槽位。
- **2.2 4-ID 自动化分发与状态同步** 【✅ 已完成】
  - **ID 1 (Transform)**：关联实体的 `TransformComponent` 获取其 `transform_id`，写入槽位第一分量。
  - **ID 2 (Geometry)**：当调用 `SetPrimitiveAsset()` 时，在 `Geometry` 注册后获取 `geometry_id`（即 `MeshDrawParamsID`），写入槽位第二分量。
  - **ID 3 & 4 (Material & Texture)**：当材质配方物化（`MaterializeRecipeRows`）完成后，获取 `material_data_index` 与 `texture_config_index`，写入第三、四分量。
- **2.3 动态局部修改 API 封装（零重新分配）** 【✅ 已完成】
  - 封装轻量原地修改接口：如 `SetTransformID(id)`、`SetMaterialID(id)`、`SetTextureID(id)`。
  - 换材质时只需原地覆写描述符槽位中的 `material_id/texture_id`，无需重新构建组件。
- **2.4 兼容性验证** 【✅ 已完成】
  - 编译并运行全量现有测试与示例（如 `PBRSpheres.cpp`、`ComputeAsteroidBelt.cpp`）：旧渲染流程正常运转，同时底层已在后台静默完成 4-ID 槽位注册与更新。通过 `TestRenderItemDataStorage` 自动化验证集成完整性。

---

### 阶段三：着色器全局注入与 BDA 寻址铺垫
> **核心目标**：在着色器层打通从 DrawID / InstanceID 提取 4-ID 的完整链路。

- **3.1 扩充全局 `SceneBinding` / `DrawInfoBinding` UBO** 【✅ 已完成】
  - 在 `GlobalAddresses` (`inc/hgl/graph/ubo/GlobalAddresses.h` 及 `ShaderLibrary/ubo/scene_ubo.glsl`) 中新增两个 64 位 GPU 缓冲区物理地址：
    - `uint64_t addr_global_render_items;`（一级表基址）
    - `uint64_t addr_draw_item_ids;`（当前帧二级绘制索引表基址）
  - 结构体自 32B 扩充至 48B，保持 8 字节自然对齐，并在 `RenderSceneUBOSystem` 帧阶段自动注入 `storage->GetGPUAddress()`。
- **3.2 编写 GLSL 统一解码头文件 (`RenderItemResolve.glsl`)** 【✅ 已完成】
  - 编写 `ShaderLibrary/common/RenderItemResolve.glsl`，通过 Vulkan GLSL buffer reference（16 字节对齐）定义 `RenderItemBufferRef` 与 `DrawItemIDBufferRef`：
    - 连号直通模式 (Direct Mode): `ResolveRenderItemDirect(uint instance_index)`
    - 间接索引模式 (Indexed Mode): `ResolveRenderItemIndexed(uint draw_id)`
    - 安全寻址宏 `HAS_GLOBAL_RENDER_ITEMS` 与 `HAS_DRAW_ITEM_IDS` 保护空指针。
- **3.3 试点着色器 BDA 解码验证** 【✅ 已完成】
  - 在 `TestRenderItemDataStorage.cpp` (Test 8) 中测试真实的 GLSL BDA 动态着色器编译到 SPIR-V（`graph::CompileShader`），验证 buffer_reference 语法与对齐。
  - 同步更新 ShaderGen schema gate，全量 13 个 ShaderGen 回归测试及全量 ECS 测试 100% 通过。

---

### 阶段四：CPU 合批管线精简化——二级索引与连号直通折叠
> **核心目标**：CPU 绘制提交全面切换为整数 Handle 模式，终结每帧组织重型数据。

- **4.1 `PrimitiveBatchPipeline` 收集项轻量化**
  - 废弃原 `PrimitiveRenderItem` 中对庞大对象指针的多重引用，每项仅提取其 `RenderItemHandle` 与排序键（`pipeline_id << 32 | material_id`）。
- **4.2 实现连号区间折叠算法 (Run-Length Compaction)**
  - 排序后遍历批次内的 Handle 列表，自动归并连续的单调递增区间：
    - 例如 `[100, 101, 102, 103, 104]` 自动合并为 `(start=100, count=5)`。
  - 对连号区间直接配置 DrawCommand：
    `vkCmdDrawIndexed(..., count, firstIndex, vertexOffset, /*firstInstance=*/start);`
    **完全跳过二级索引缓冲的分配与写入！**
- **4.3 离散 Handle 写入二级绘制索引表 (`DrawItemIDBuffer`)**
  - 对于无法折叠的散乱图元，将其 `RenderItemHandle` 紧凑写入每帧暂态动态环形 SSBO。
- **4.4 数据吞吐量与性能对比实测**
  - 统计单帧 CPU 向 GPU 提交的每帧缓冲字节数，验证连续场景与静态大世界中 CPU 上传量直降趋近为 0。

---

### 阶段五：海量多实例与 GPU-Driven 统一接入
> **核心目标**：将 GPU-Driven CS 视锥剔除与多实例渲染彻底收编至统一架构。

- **5.1 `InstancedPrimitiveComponent` 统一模型接入**
  - **CPU 驱动多实例**：向 `RenderItemDataStorage` 申请一段连续的 Handle 区间 `[base, count]`，以连号直通方式直接单次 DrawCall 提交。
  - **100% GPU-Driven 模式**：Compute Shader 输出的间接命令与筛选后的存活索引表，直接与 `DrawItemIDBuffer` / `GlobalRenderItemBuffer` 统一对齐，不再需要任何特殊的管线外挂代码。
- **5.2 重构验证 `ComputeAsteroidBelt.cpp`**
  - 100 万颗陨石统一按照全局描述符规则运行，验证视锥剔除、围绕公转与波形摆动平滑无卡顿。

---

### 阶段六：遗留代码清理与全量回归测试
> **核心目标**：清除过渡期代码，确立全新工业级基线。

- **6.1 废弃过渡代码**
  - 彻底删除 `PrimitiveBatchPipeline` 中每帧组装 `MaterialInstanceAddresses` 的陈旧代码路径。
  - 移除合批阶段动态 `EnsureMeshDrawParams` 补录逻辑，统一由 Storage 预分配担保。
- **6.2 全量工程回归与稳定性测试**
  - 编译并执行 `example/` 下全量用例（`PBRSpheres`、`BasicLitMeshes`、`TextureQuad`、`SkyCubeSphere` 等）。
  - 验证多视口、动态换材质、多实例、GPU Compute 均零告警、零内存泄漏、零渲染伪影。

