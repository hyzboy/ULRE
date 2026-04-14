# Primitive 详细构成：Geometry / GeometryData / VertexDataManager

> 涉及文件：
> - `inc/hgl/graph/mesh/Primitive.h`
> - `inc/hgl/graph/mesh/GeometryDrawRange.h`
> - `inc/hgl/graph/mesh/GeometryDataBuffer.h`
> - `inc/hgl/graph/geo/VKGeometryData.h`
> - `inc/hgl/graph/geo/VKGeometry.h`
> - `inc/hgl/graph/geo/GeometryCreater.h`
> - `inc/hgl/vk/VertexDataManager.h`
> - `src/SceneGraph/mesh/Primitive.cpp`
> - `src/SceneGraph/VKGeometryData.cpp`
> - `src/Vulkan/VertexDataManager.cpp`

---

## 1. 概述

`Primitive` 是 ECS 渲染中**最小的渲染单位**，由一个几何体配一个材质实例组合而成。

```
Primitive
├── GraphicsPipeline *           ← 渲染管线（着色器+RasterState）
├── MaterialInstance *   ← 材质实例（参数/纹理绑定）
├── Geometry *           ← 几何接口（名称 + 包围体 + 数据访问代理）
│     └── GeometryData * ← 几何实现（VAB/IBO 的持有和分配）
│           └── [可选] via VertexDataManager（批量共享缓冲区）
├── GeometryDataBuffer * ← CPU 侧 draw call 绑定信息缓存
└── GeometryDrawRange    ← 绘制范围（vertex_offset / first_index / counts）
```

---

## 2. Primitive 各成员详解

```cpp
class Primitive
{
    GraphicsPipeline *  pipeline;       // 渲染管线
    MaterialInstance *  mat_inst;       // 材质实例
    Geometry *          geometry;       // 几何体接口

    GeometryDataBuffer *data_buffer;    // GPU 侧绑定信息缓存（VkBuffer 列表）
    GeometryDrawRange   draw_range;     // 绘制参数（offset/count）
};
```

### 2.1 `GraphicsPipeline`

Vulkan GraphicsPipeline 对象，包含着色器、顶点输入格式（`VIL`）、光栅化状态等。`Primitive` 直接持有指针，不拥有所有权，由 `PipelineManager` 管理生命周期。

**约束**：`DirectCreatePrimitive()` 会检查 `MaterialInstance::GetVIL()` 与 `GraphicsPipeline::GetVIL()` 是否完全一致——格式或绑定点不匹配则拒绝创建。

### 2.2 `MaterialInstance`

材质实例，持有材质参数的具体值（颜色、纹理 Sampler、Uniform 数据等）。通过 `ChangeMaterialInstance()` 可以在同一 `Material`（母材质）下换绑不同的 MI，但不允许跨 Material 替换。

### 2.3 `GeometryDataBuffer`（draw call 绑定信息）

```cpp
struct GeometryDataBuffer
{
    uint32_t      vab_count;     // VAB 绑定槽数量（= max binding index + 1）
    VkBuffer *    vab_list;      // 各绑定槽对应的 VkBuffer（flat C 数组）
    VkDeviceSize *vab_offset;    // 各槽字节偏移（通常全为 0）

    IndexBuffer * ibo;           // 索引缓冲区（无索引时 nullptr）
    VertexDataManager *vdm;      // 所属 VDM（仅用于排序/比较，不实际调用）
};
```

`GeometryDataBuffer` 是 `DirectCreatePrimitive()` 在创建 Primitive 时构建的 **Flat VkBuffer 绑定表**，直接对应 Vulkan 的 `vkCmdBindVertexBuffers` 参数格式。它按照材质 VIL 中的 `binding` 索引将各 VAB 的 `VkBuffer` 填入对应位置，省去每帧渲染时的间接回查。

`operator<=>` 使得 `GeometryDataBuffer` 可以用于 `DrawBatch` 排序（VDM 相同的批次代表共享大缓冲区，可减少 vkCmdBindVertexBuffers 调用次数）。

### 2.4 `GeometryDrawRange`（绘制范围）

```cpp
struct GeometryDrawRange
{
    int32_t  vertex_offset;      // 顶点基偏移（元素单位，非字节）
    uint32_t first_index;        // 首个索引位置

    uint32_t vertex_count;       // 本次绘制的顶点数（可小于 data_vertex_count）
    uint32_t index_count;        // 本次绘制的索引数

    uint32_t data_vertex_count;  // 缓冲区实际容量（顶点数）
    uint32_t data_index_count;   // 缓冲区实际容量（索引数）
};
```

- `vertex_offset` / `first_index` 在使用 **VDM 共享大缓冲区** 时非零，代表该几何体在大 VkBuffer 中的起始槽位。
- `vertex_count` / `index_count` 可通过 `SetDrawCounts()` 或 `SetDrawRange()` 缩减，但不能超过 `data_*_count`。

---

## 3. Geometry — 几何接口层

`Geometry` 是 `GeometryData` 的公开访问代理，**屏蔽初始化/分配细节**，仅暴露只读查询接口。

```cpp
class Geometry
{
    AnsiString      geometry_name;
    GeometryData *  geometry_data;
    BoundingVolumes bounding_volumes;   // AABB/sphere 包围体

public:
    VkDeviceSize GetVertexCount() const;
    VAB *        GetVAB(int index) const;
    VAB *        GetVAB(const AnsiString &name) const;
    VkBuffer     GetVkBuffer(int index) const;
    int32_t      GetVertexOffset() const;   // → geometry_data->GetVertexOffset()
    uint32_t     GetIndexCount() const;
    IndexBuffer *GetIBO() const;
    uint32_t     GetFirstIndex() const;     // → geometry_data->GetFirstIndex()
    VertexDataManager *GetVDM() const;      // → geometry_data->GetVDM()
};
```

`Geometry` 对象由 `GeometryManager` 以 `AutoIdObjectManager` 管理生命周期，可通过 `GeometryID` 检索。

---

## 4. GeometryData — 几何实现层

### 4.1 基类定义

```cpp
class GeometryData
{
protected:
    const VIL *  vil;           // 顶点输入格式（来自 Material，外部持有）

    uint32_t     vertex_count;  // 顶点总数（创建时固定）
    uint32_t     index_count;   // 索引总数

    VAB **       vab_list;      // VAB 指针数组（长度 = vil->GetVertexAttribCount()）
    IndexBuffer *ibo;           // 索引缓冲区

protected:
    // 子类实现：在对应存储策略下分配 VAB/IBO
    virtual VAB *        CreateVAB(int vab_index, VkFormat, const void *data, const AnsiString &name) = 0;
    virtual IndexBuffer *CreateIBO(uint32_t ic, const IndexType &, const AnsiString &name) = 0;

public:
    virtual int32_t            GetVertexOffset() const = 0;  // 私有缓冲区=0，VDM=子分配起始
    virtual uint32_t           GetFirstIndex()   const = 0;
    virtual VertexDataManager *GetVDM()          const = 0;  // 私有缓冲区=nullptr
};
```

### 4.2 三种具体实现（均为匿名命名空间内部类）

| 实现类 | 工厂函数 | 缓冲区来源 | `GetVertexOffset` | `GetVDM` |
|--------|----------|------------|-------------------|----------|
| `GeometryDataPrivateBuffer` | `CreateGeometryData(VulkanDevice*, ...)` | `VulkanDevice::CreateVAB/IBO` | 0 | `nullptr` |
| `GeometryDataPrivateBufferBM` | `CreateGeometryData(BufferManager*, ...)` | `BufferManager::CreateVAB/IBO` | 0 | `nullptr` |
| `GeometryDataVDM` | `CreateGeometryData(VertexDataManager*, vc)` | VDM 池子分配（`AcquireVAB/AcquireIB`） | vab_node->GetStart() | vdm |

#### `GeometryDataPrivateBuffer` / `GeometryDataPrivateBufferBM`

每个 Geometry 独占自己的 `VkBuffer`，`vertex_offset = 0`, `first_index = 0`。析构时分别通过 `delete` 或 `buffer_manager->Release()` 释放。

#### `GeometryDataVDM`（共享缓冲区模式）

在 `VertexDataManager` 管理的大型共享 `VkBuffer` 中，通过 `BlockAllocator` 子分配一段连续区域：

```
构造：
    vab_node = vdm->AcquireVAB(vc)   // BlockAllocator 分配顶点区段
    ib_node  = vdm->AcquireIB(ic)    // BlockAllocator 分配索引区段（CreateIBO时延迟）

析构：
    vdm->ReleaseVAB(vab_node)
    vdm->ReleaseIB(ib_node)

GetVertexOffset() = vab_node->GetStart()   // 在大 VkBuffer 中的起始顶点槽
GetFirstIndex()   = ib_node->GetStart()    // 在大 IBO 中的起始索引槽
GetVAB(i)         = vdm->GetVAB(i)         // 直接返回 VDM 大缓冲区（所有 GeoDataVDM 共享同一 VkBuffer）
CreateVAB()       → 直接写入 vdm->GetVAB(i)，起始位置为 vab_node->GetStart()
```

---

## 5. VertexDataManager — 顶点数据管理器

`VertexDataManager`（别名 `VDM`）提供**大缓冲区 + BlockAllocator 子分配**策略，让多个 Geometry 共享同一组 `VkBuffer`，减少 GPU 缓冲区对象数量和绑定切换。

### 5.1 关键成员

```cpp
class VertexDataManager
{
    VulkanDevice *  device;           // 或使用 buffer_manager
    BufferManager * buffer_manager;

    const VIL *  vil;                 // 绑定的顶点输入格式
    uint         vi_count;            // VAB 流数量
    const VIF *  vif_list;            // 每流的格式信息

    VkDeviceSize vab_max_size;        // 大缓冲区总顶点槽数（Init 时固定）
    VkDeviceSize vab_cur_size;        // 当前分配出去的顶点槽数
    VAB **       vab;                 // vi_count 个大型 VkBuffer，各对应一个属性流

    VkDeviceSize ibo_cur_size;
    IndexBuffer *ibo;                 // 共享 IBO（可选）

    BlockAllocator vbo_data_chain;    // 顶点区段分配器
    BlockAllocator ibo_data_chain;    // 索引区段分配器
};
```

### 5.2 初始化流程（`Init(vbo_size, ibo_size, index_type)`）

```
1. 按 vil 的每条输入流（vi_count 个）创建对应格式的大型 VAB（vab_max_size 个顶点）
2. 若 ibo_size > 0，创建共享 IndexBuffer
3. 初始化 vbo_data_chain(vab_max_size) 和 ibo_data_chain(ibo_size)
```

### 5.3 子分配 API

```cpp
BlockAllocator::UserNode *AcquireVAB(VkDeviceSize count);  // 从 vbo_data_chain 分配 count 个顶点槽
BlockAllocator::UserNode *AcquireIB (VkDeviceSize count);  // 从 ibo_data_chain 分配 count 个索引槽

bool ReleaseVAB(BlockAllocator::UserNode *);
bool ReleaseIB (BlockAllocator::UserNode *);
```

`BlockAllocator::UserNode` 保存 `[start, count]`，`GetStart()` 即为子分配区段在大 VkBuffer 中的偏移（以元素为单位）。

### 5.4 两种构造方式

| 构造函数 | 底层分配者 | 析构行为 |
|----------|------------|----------|
| `VertexDataManager(VulkanDevice*, VIL*)` | `device->CreateVAB/CreateIBO` | `delete vab[i]; delete ibo` |
| `VertexDataManager(BufferManager*, VIL*)` | `buffer_manager->CreateVAB/CreateIBO` | `buffer_manager->Release(vab[i]/ibo)` |

---

## 6. GeometryCreater — 几何体创建辅助类

`GeometryCreater` 是上层代码创建 `Geometry` 的便捷入口，封装了以下三条创建路径：

| 构造方式 | 底层 GeometryData 类型 | 典型使用场景 |
|----------|------------------------|-------------|
| `GeometryCreater(VulkanDevice*, VIL*)` | `GeometryDataPrivateBuffer` | 不共享，单个几何体 |
| `GeometryCreater(VulkanDevice*, VIL*, BufferManager*)` | `GeometryDataPrivateBufferBM` | BufferManager 统一管理 |
| `GeometryCreater(VertexDataManager*)` | `GeometryDataVDM` | 批量渲染共享缓冲区 |

核心方法：

```cpp
bool Init(name, vertex_count, index_count, IndexType);   // 分配 GeometryData 并注册

// 写顶点属性数据
bool WriteVAB(const AnsiString &name, VkFormat format, const void *data);

// 获取 BufferAccessor 用于结构化写入
VertexAttribBuffer *GetVAB(const AnsiString &name, VkFormat format);
template<typename T> T GetBufferAccessor(const AnsiString &name, VkFormat format);
```

---

## 7. `DirectCreatePrimitive()` — 创建 Primitive 的完整流程

```cpp
Primitive *DirectCreatePrimitive(Geometry *geom, MaterialInstance *mi, GraphicsPipeline *p)
{
    // 1. 检查 VIL 一致性（Material.VIL == GraphicsPipeline.VIL）
    if (*vil != *p->GetVIL()) return nullptr;

    // 2. 确定 binding slot 最大值 → 决定 GeometryDataBuffer 大小
    const uint input_count = vil->GetVertexAttribCount(VertexInputGroup::Basic);
    uint max_binding = 0;
    for (vif in vil) max_binding = max(max_binding, vif.binding);

    // 3. 构建 GeometryDataBuffer（flat VkBuffer 绑定表）
    GeometryDataBuffer *gdb = new GeometryDataBuffer(max_binding+1, geom->GetIBO(), geom->GetVDM());

    // 4. 按材质 VIF 的 name 字段从 Geometry 查找对应 VAB
    //    验证 format 和 stride，填入 gdb->vab_list[binding] = vab->GetVkBuffer()
    for each (vif in vil->Basic)
    {
        vab = geom->GetVAB(vif.name);              // 按名称查找，与顺序无关
        assert(vab->GetFormat() == vif.format);    // 格式校验
        assert(vab->GetStride() == vif.stride);    // stride 校验
        gdb->vab_list[vif.binding] = vab->GetVkBuffer();
        gdb->vab_offset[vif.binding] = 0;
    }

    // 5. 构造 Primitive（同时初始化 draw_range.Set(geometry)）
    return new Primitive(geom, mi, p, gdb);
}
```

**关键点**：VAB 查找基于属性**名称**（`vif.name`），而非顺序。这使得 Geometry VAB 的存储顺序可以与 Material VIF 顺序不同，耦合度更低。

---

## 8. 各层所有权关系

```
PrimitiveManager
└── owns Primitive *
      ├── (not owned) GraphicsPipeline *         ← PipelineManager 持有
      ├── (not owned) MaterialInstance * ← 调用方持有
      ├── (not owned) Geometry *         ← GeometryManager 持有
      │     └── owns GeometryData *
      │           ├── 模式A: owns VAB[], owns IBO          (PrivateBuffer / PrivateBufferBM)
      │           └── 模式B: 借用 VDM 的 VAB[]，借用 IBO  (GeometryDataVDM)
      │                 └── VDM 由上层（Geometry创建者）管理生命周期
      ├── owns GeometryDataBuffer *      ← Primitive 创建时 new，析构时 delete
      └── value GeometryDrawRange        ← 内嵌值，无动态分配
```

---

## 9. 渲染时数据访问路径

Vulkan draw call 录制时，渲染系统通过以下路径提取所需信息：

```
Primitive::GetDataBuffer()    → GeometryDataBuffer
  ├── vab_list[i]             → vkCmdBindVertexBuffers(... vab_count, vab_list, vab_offset)
  └── ibo                     → vkCmdBindIndexBuffer(ibo->GetVkBuffer(), 0, ibo->GetIndexType())

Primitive::GetRenderData()    → GeometryDrawRange
  ├── 有索引: vkCmdDrawIndexed(index_count, 1, first_index, vertex_offset, 0)
  └── 无索引: vkCmdDraw(vertex_count, 1, vertex_offset, 0)
```

在 `GeometryDataVDM` 模式下，`vab_list[i]` 全部指向同一 VDM 大缓冲区的 `VkBuffer`，而 `vertex_offset` / `first_index` 非零——Vulkan 在 `vkCmdDrawIndexed` 的 `vertexOffset` 参数中将其应用于索引结果，实现多 Geometry 共享一次 BindVertexBuffers。

---

## 10. 两种缓冲区模式对比

| 维度 | 私有缓冲区（PrivateBuffer/BM）| VDM 共享缓冲区（GeometryDataVDM）|
|------|----------------------------|---------------------------------|
| VkBuffer 对象数 | 每个 Geometry 独占 N 个 VAB | 全场景共享 N 个大 VAB |
| `vertex_offset` | 0 | `BlockAllocator::UserNode::GetStart()` |
| Draw call 绑定切换 | 每个 Primitive 一次 Bind | 相同 VDM 的 Primitive 无需重复 Bind |
| 动态增删 | 随意，创建/删除 Geometry 互不影响 | 受 BlockAllocator 碎片影响 |
| 适用场景 | 少量大几何体；动态生成/删除 | 大量小几何体批量渲染（地形/粒子/UI） |
| `GeometryDataBuffer::vdm` | `nullptr` | 指向所属 VDM（用于批次排序） |

---

## 11. GeometrySignature — 几何体在材质 Variant 中的标识

`GeometrySignature`（定义在 `inc/hgl/graph/module/RuntimeMaterialRequest.h`）是一个轻量结构，描述一个几何体在材质 variant 解析时的"形状特征"，用于 `MaterialAssetRegistry::ResolveMI()` 的缓存键之一。

### 11.1 结构与字段含义

```cpp
struct GeometrySignature
{
    PrimitiveType primitive              = PrimitiveType::Triangles;
    uint32_t      vil_hash               = 0;   // 材质所需 VIL 属性集的 FNV-1a hash；0 表示 deferred
    uint32_t      geometry_layout_hash   = 0;   // (format, stride) per VAB 的 FNV-1a hash
    const Geometry *geometry_for_vil_derivation = nullptr;   // 运行时辅助，不参与 operator==
};
```

| 字段 | 含义 |
|------|------|
| `primitive` | 图元类型（Triangles / Lines 等），影响管线选择 |
| `vil_hash` | 已解析 VIL 的哈希；`0` 代表 VIL 尚未创建（deferred 路径） |
| `geometry_layout_hash` | 每条 VAB 的（format, stride）组合哈希，代表几何体的实际属性布局 |
| `geometry_for_vil_derivation` | 仅供首次 VIL 推导使用的运行时指针，**故意不纳入 operator==** |

### 11.2 operator== 的条件规则

```cpp
bool operator==(const GeometrySignature &o) const
{
    if (primitive != o.primitive || vil_hash != o.vil_hash) return false;
    // geometry_layout_hash 仅在 vil_hash == 0（deferred 路径）时参与比较
    if (vil_hash == 0 && geometry_layout_hash != o.geometry_layout_hash) return false;
    return true;
}
```

**规则**：一旦 `vil_hash != 0`（VIL 已解析），`geometry_layout_hash` **退出等值比较**。

- 这允许"材质所需属性相同但 Geometry 有额外冗余属性"的 Primitive 复用同一个 `variant_cache` 条目。
- 在 deferred 路径（`vil_hash == 0`）下，尚无 VIL 可比较，所以退而使用 `geometry_layout_hash` 区分不同布局，防止误命中。

### 11.3 与 VDM / GeometryData 的关系

VDM 模式下（`GeometryDataVDM`），`GetVDM()` 返回非空指针，但 `geometry_layout_hash` 仍按 VAB 的 `(format, stride)` 计算，与缓冲区是否共享无关。因此同一个 VDM 内不同格式的 Geometry 仍会产生不同的 `GeometrySignature`，正确隔离各自的材质 variant 缓存。
