# Primitive 详细构成：Geometry / GeometryData / VertexDataManager

> 涉及文件：
> - `inc/hgl/graph/mesh/Primitive.h`
> - `inc/hgl/graph/mesh/GeometryDrawRange.h`
> - `inc/hgl/graph/mesh/GeometryDataBuffer.h`
> - `inc/hgl/graph/mesh/StaticMesh.h`
> - `inc/hgl/graph/geo/VKGeometryData.h`
> - `inc/hgl/graph/geo/VKGeometry.h`
> - `inc/hgl/graph/geo/GeometryCreater.h`
> - `inc/hgl/vk/VertexDataManager.h`
> - `inc/hgl/vk/pipeline/VKPipeline.h`
> - `inc/hgl/graph/module/GeometryManager.h`
> - `inc/hgl/graph/asset/PrimitiveAsset.h`
> - `inc/hgl/ecs/components/PrimitiveComponent.h`
> - `src/SceneGraph/mesh/Primitive.cpp`
> - `src/SceneGraph/VKGeometryData.cpp`
> - `src/SceneGraph/VKGeometry.cpp`
> - `src/Vulkan/VertexDataManager.cpp`
> - `src/ecs/support/PrimitiveBatchPipeline.cpp`（批内 draw 提交）
>
> 术语：**VDM**（共享顶点数据池）/ **Private**（每几何独占缓冲区）两条几何数据来源，判定接口 `geometry->GetVDM() != nullptr`。

---

## 1. 概述

`Primitive` 是 ECS 渲染中**最小的渲染单位**，由一个几何体配一个材质实例组合而成。

```
Primitive
├── Pipeline *           ← 渲染管线（VkPipeline 包装 + MaterialPipelineConfig）
├── ShaderProgram *      ← 材质程序（原 MaterialInstance 位，持 VkPipelineLayout）
├── Geometry *           ← 几何接口（名称 + 包围体 + VAB/IBO/meshlet 访问代理）
│     └── GeometryData * ← 几何实现（VAB/IBO 的持有和分配）
│           └── [可选] via VertexDataManager（批量共享缓冲区）
├── GeometryDataBuffer * ← 绑定信息缓存（顶点语义 → VkBuffer 对照表，供排序/刷新）
└── GeometryDrawRange    ← 绘制范围（vertex_offset / first_index / counts）
```

> `Primitive` **不持有任何描述符集**——资源绑定由 ECS recipe runtime 负责；
> GPU 侧顶点取数走 `MeshDrawParams` 行（BDA 地址表，见 §9）。

---

## 2. Primitive 各成员详解

```cpp
class Primitive
{
    Pipeline *          pipeline;          // 渲染管线（不持有所有权）
    ShaderProgram *     material_program;  // 材质程序（含 VkPipelineLayout）
    Geometry *          geometry;          // 几何体接口

    GeometryDataBuffer *data_buffer;       // 绑定信息缓存（语义 → VkBuffer）
    GeometryDrawRange   draw_range;        // 绘制参数（offset/count）
};
```
（真源 `inc/hgl/graph/mesh/Primitive.h:17-35`，成员顺序即上表）

### 2.1 `Pipeline`

`VkPipeline` 的 C++ 包装（`inc/hgl/vk/pipeline/VKPipeline.h`），携带创建时的 `mtl::MaterialPipelineConfig config`（着色器阶段、顶点输入语义、光栅化/深度等**静态**状态）与 overlay。

- 创建与生命周期：`RenderPass::CreatePipeline()`（`src/Vulkan/VKRenderPass.cpp:50`，`ShaderProgram*` 重载在 `:121`、`MaterialRecipe` 重载在 `:137`）创建，并在同一 RenderPass 内按 `VkPipeline` 去重复用（`:101`）；`StaticMesh` 只持引用（`PipelinePtrSet = OrderedSet<Pipeline *>`，`inc/hgl/graph/mesh/StaticMesh.h:34`）。
- **没有 `PipelineManager`，也没有独立的 `RasterState` 对象**：静态光栅状态在 `MaterialPipelineConfig`，动态状态由 EDS 下发（渲染侧 `ApplyPipelineState` 走 `vkCmdSet*`）。

**约束**：`DirectCreatePrimitive()` 不再做 VIL 一致性校验——`VIL`/`GetVIL()`/`VertexInputGroup` 已随经典 attribute 管线退役（全树 0 命中）；顶点输入语义由几何自身的 `GeometryVertexFormat` 决定。

### 2.2 `ShaderProgram`（原 `MaterialInstance` 位）

`Primitive::material_program` 由 `ShaderProgramManager` 按「材质定义 + recipe」编译并缓存（`AutoIdObjectManager<ShaderProgramID, ShaderProgram>`，`inc/hgl/graph/module/ShaderProgramManager.h:46`），对外提供 `GetPipelineLayout()`。

- 材质参数值不再放在描述符集里：改为 `GlobalSSBOBufferRegistry` 的材质数据行（`PBRSurfaceRow` 等）+ 纹理引用行，经 `GlobalSSBOBinding{ssbo_type, ssbo_id, data_index}` 定位。
- 换材质不再有 `ChangeMaterialInstance()`（0 命中）：ECS 侧经 `PrimitiveComponent::SetPrimitiveAsset()`（几何 + recipe 配对）与 `MaterialRecipe` 驱动。

### 2.3 `GeometryDataBuffer`（draw call 绑定信息）

```cpp
struct GeometryDataBuffer
{
    uint32_t          geometry_id;    // 几何在 MeshDrawParams 池中的行号（0 = 未注册）
    uint32_t          vab_count;      // 顶点属性槽数量（= GeometryVertexFormat::GetCount()）
    VkBuffer *        vab_list;       // 各槽对应的 VkBuffer（flat C 数组）
    VertexSemantic *  vab_semantic;   // 每个槽的顶点语义（按 GeometryVertexFormat 填充）
    VkDeviceSize *    vab_offset;     // 各槽字节偏移（当前恒为 0）

    IndexBuffer *     ibo;            // 索引缓冲区（无索引时 nullptr）
    VertexDataManager *vdm;           // 仅用于排序/比较，不实际使用
};
```

`GeometryDataBuffer` 是 `DirectCreatePrimitive()` 在创建 Primitive 时构建的 **顶点语义 → VkBuffer 对照表**。顶点输入已统一为 SSBO（mesh shader 路径按语义取 BDA，见 §9），因此这张表的作用是**运行时对照与刷新**：`Update(const Geometry *)` 在几何顶点数据变化时重填（`src/SceneGraph/mesh/Primitive.cpp`，ECS 侧由 `PrimitiveComponent::EnsureRuntimeGeometryBinding()` 调用）。

`operator<=>` 使它可用于 `DrawBatch` 排序，比较序为 `geometry_id → vdm → vab_count → vab_list → vab_offset → ibo`（`inc/hgl/graph/mesh/GeometryDataBuffer.h`）：相同 `geometry_id`/VDM 的批＝同一 `MeshDrawParams` 行、同一批大缓冲区，可合并提交。

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
- `Set(const Geometry *)` 在 Primitive 构造时填初值；`operator<=>` 按同名字段序参与批次排序（`inc/hgl/graph/mesh/GeometryDrawRange.h:31`）。

---

## 3. Geometry — 几何接口层

`Geometry` 是 `GeometryData` 的公开访问代理，**屏蔽初始化/分配细节**，仅暴露只读查询接口。

```cpp
class Geometry
{
    AnsiString      geometry_name;
    GeometryData *  geometry_data;
    BoundingVolumes bounding_volumes;    // AABB/sphere 包围体

    DeviceBuffer *  meshlets_buffer / meshlet_vertices_buffer /   // meshlet 四表（可选）
                    meshlet_triangles_buffer / meshlet_bounds_buffer;
    uint32_t        meshlet_count = 0;
    uint32_t        geometry_id = 0;                     // MeshDrawParams 行号
    GlobalSSBOBufferRegistry * mesh_draw_params_pool = nullptr;

public:
    const   VkDeviceSize    GetVertexCount  ()const;
    const   uint32_t        GetVABCount     ()const;
    const   int             GetVABIndex     (const VertexSemantic)const;
            VAB *           GetVAB          (const int)const;
            VAB *           GetVAB          (const VertexSemantic)const;
            VkBuffer        GetVkBuffer     (const int)const;
            VkBuffer        GetVkBuffer     (const VertexSemantic)const;
    const   int32_t         GetVertexOffset ()const;     // → geometry_data->GetVertexOffset()
    const   uint32_t        GetIndexCount   ()const;
            IndexBuffer *   GetIBO          ()const;
    const   uint32_t        GetFirstIndex   ()const;     // → geometry_data->GetFirstIndex()
    const   GeometryVertexFormat &GetGeometryVertexFormat()const;
    VertexDataManager *     GetVDM          ()const;     // → geometry_data->GetVDM()

    // mesh 路径：meshlet 四表 + MeshDrawParams 行注册（BDA 地址表，见 §9）
    bool            HasMeshlets            ()const;
    uint32_t        GetMeshletCount        ()const;
    DeviceBuffer *  GetMeshletsBuffer      ()const;
    DeviceBuffer *  GetMeshletVerticesBuffer()const;
    DeviceBuffer *  GetMeshletTrianglesBuffer()const;
    DeviceBuffer *  GetMeshletBoundsBuffer ()const;
    void            SetMeshlets(uint32_t count,DeviceBuffer *mb,DeviceBuffer *mvb,DeviceBuffer *mtb,DeviceBuffer *mbb=nullptr);

    uint32_t        GetGeometryID          ()const;       // 0 = 尚未注册
    bool            RegisterMeshDrawParams (GlobalSSBOBufferRegistry *,VulkanDevice *);
    bool            EnsureMeshDrawParams   (GlobalSSBOBufferRegistry *,VulkanDevice *);
};
```

（真源 `inc/hgl/graph/geo/VKGeometry.h:44-137`）VAB 查询只支持**索引**与 **`VertexSemantic`** 两种——按名字（`AnsiString`）查 VAB 的重载已退役。

`Geometry` 对象由 `GeometryManager` 以 `AutoIdObjectManager<GeometryID, Geometry>` 管理生命周期（`inc/hgl/graph/module/GeometryManager.h:17`），可按 ID 检索。

---

## 4. GeometryData — 几何实现层

### 4.1 基类定义

```cpp
class GeometryData
{
protected:
    GeometryVertexFormat geometry_vertex_format;  // 顶点属性布局（原 VIL，几何自带、不外部共享）

    uint32_t        vertex_count;   // 顶点总数（创建时固定）
    uint32_t        index_count;    // 索引总数

    std::vector<VAB *> vab_list;    // 长度 = geometry_vertex_format.GetCount()
    IndexBuffer *   ibo;            // 索引缓冲区

protected:
    // 子类实现：在对应存储策略下分配 VAB/IBO
    virtual VAB *        CreateVAB(const int vab_index, const VkFormat, const void *data, const AnsiString &name) = 0;
    virtual IndexBuffer *CreateIBO(const uint32_t ic, const IndexType &, const AnsiString &name) = 0;

public:
    // 属性声明与批量创建
    int  DeclareVertexAttribute(const VertexSemantic, const VkFormat, const uint8_t vec_size = 0, const uint32_t stride = 0);
    int  DeclareVertexAttribute(const VertexInputFormat *vif);
    bool CreateAllVAB(const AnsiString &geometry_name = "Geometry");
    VAB *InitVAB(const int vab_index, const void *data, const AnsiString &name = "VAB");
    IndexBuffer *InitIBO(const int index_count, const IndexType, const AnsiString &name = "IBO");
    void UnmapAll();

    virtual int32_t            GetVertexOffset() const = 0;  // 私有缓冲区=0，VDM=子分配起始
    virtual uint32_t           GetFirstIndex()   const = 0;
    virtual VertexDataManager *GetVDM()          const = 0;  // 私有缓冲区=nullptr
};
```

（真源 `inc/hgl/graph/geo/VKGeometryData.h:26-77`；`VIL` 参数已由 `GeometryVertexFormat` 取代）

### 4.2 三种具体实现（均为匿名命名空间内部类）

| 实现类 | 工厂函数 | 缓冲区来源 | `GetVertexOffset` | `GetVDM` |
|--------|----------|------------|-------------------|----------|
| `GeometryDataPrivateBuffer` | `CreateGeometryData(VulkanDevice*, gvf, vc[, BufferAllocPolicy])` | `VulkanDevice::CreateVAB/CreateIBO` | 0 | `nullptr` |
| `GeometryDataPrivateBufferBM` | `CreateGeometryData(BufferManager*, gvf, vc[, BufferAllocPolicy])` | `BufferManager::CreateVAB/CreateIBO` | 0 | `nullptr` |
| `GeometryDataVDM` | `CreateGeometryData(VertexDataManager*, gvf, vc)` | VDM 池子分配（`AcquireVAB/AcquireIB`） | `vab_node->GetStart()` | `vdm` |

（三者均为 `src/SceneGraph/VKGeometryData.cpp` 匿名命名空间内部类，只经工厂函数暴露，构造参数是 `GeometryVertexFormat`；仅 `GeometryDataVDM` 额外吃 `BufferAllocPolicy` 以外的东西——私有两条路径可选 `BufferAllocPolicy`。）

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

    GeometryVertexFormat geometry_vertex_format;  // 原 `const VIL *vil`
    uint            vi_count;                     // 顶点输入流数量

    VkDeviceSize vab_max_size;        // 大缓冲区总顶点槽数（Init 时固定）
    VkDeviceSize vab_cur_size;        // 当前分配出去的顶点槽数
    VAB **       vab;                 // vi_count 个大型 VkBuffer，各对应一个属性流

    VkDeviceSize ibo_cur_size;
    IndexBuffer *ibo;                 // 共享 IBO（可选）

    BlockAllocator vbo_data_chain;    // 顶点区段分配器
    BlockAllocator ibo_data_chain;    // 索引区段分配器
};
```

访问器：`GetGeometryVertexFormat()` / `GetVABStreamCount()` / `GetVABMaxCount()` / `GetVABCurCount()` / `GetIndexType()` / `GetIndexMaxCount()` / `GetIndexCurCount()`（`inc/hgl/vk/VertexDataManager.h`）。

> 头文件里还记着 SSBO 顶点输入的寻址契约（`:13-34`）：indexed draw 中 `gl_VertexIndex = BaseVertex + 索引值`（firstVertex 已自动含入），shader 直接 `data[gl_VertexIndex]` 就是 VDM 大缓冲的绝对顶点号，**不要再加 `gl_BaseVertex`**（会双加错位）；VAB 数据是紧凑格式，SSBO 声明必须 `layout(std430, scalar)`，否则 vec3 数组 stride 16B 错位。

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
| `VertexDataManager(VulkanDevice*, const GeometryVertexFormat &)` | `device->CreateVAB/CreateIBO` | `delete vab[i]; delete ibo` |
| `VertexDataManager(BufferManager*, const GeometryVertexFormat &)` | `buffer_manager->CreateVAB/CreateIBO` | `buffer_manager->Release(vab[i]/ibo)` |

---

## 6. GeometryCreater — 几何体创建辅助类

`GeometryCreater` 是上层代码创建 `Geometry` 的便捷入口，封装了以下三条创建路径：

| 构造方式 | 底层 GeometryData 类型 | 典型使用场景 |
|----------|------------------------|-------------|
| `GeometryCreater(VulkanDevice*, const GeometryVertexFormat &, BufferManager *bm = nullptr)`（bm 为空） | `GeometryDataPrivateBuffer` | 不共享，单个几何体 |
| `GeometryCreater(VulkanDevice*, const GeometryVertexFormat &, BufferManager *)` | `GeometryDataPrivateBufferBM` | BufferManager 统一管理 |
| `GeometryCreater(VertexDataManager *)` | `GeometryDataVDM` | 批量渲染共享缓冲区 |

选择逻辑在 `Init()` 里（`src/SceneGraph/geo/GeometryCreater.cpp:86-98`）：`vdm` 优先 → 否则 `buffer_manager` → 否则 `device`；VDM 路径下索引类型继承自 VDM（`Init` 的 `it` 参数失效）。

核心方法（**全部按 `VertexSemantic` 寻址，按名字的 VAB 接口已退役**）：

```cpp
bool Init(const AnsiString &name, const uint32_t vertices_count,
          const uint32_t index_count = 0, IndexType it = IndexType::AUTO);
void SetBufferPolicy(const BufferAllocPolicy);   // 默认 GPUOnly
void Clear();

// 写顶点属性数据
bool WriteVAB(const VertexSemantic semantic, const VkFormat format, const void *data);

// 取 VAB / TypedArrayView 用于结构化写入
VertexAttribBuffer *GetVAB(const VertexSemantic semantic, const VkFormat format = VK_FORMAT_UNDEFINED);
template<typename TypedArrayViewType> GetTypedArrayView(const VertexSemantic semantic);
```

（真源 `inc/hgl/graph/geo/GeometryCreater.h:174-220`）

---

## 7. `DirectCreatePrimitive()` — 创建 Primitive 的完整流程

```cpp
Primitive *DirectCreatePrimitive(Geometry *geom, ShaderProgram *material, Pipeline *p)
{
    if (!geom || !material) return nullptr;

    // 1. 顶点输入统一为 SSBO：槽位数 = 几何自身 GeometryVertexFormat 的属性数
    const GeometryVertexFormat &gvf = geom->GetGeometryVertexFormat();
    const uint32_t attr_count = gvf.GetCount();

    // 2. 建 GeometryDataBuffer（VkBuffer 对照表 + geometry_id）
    GeometryDataBuffer *gdb = new GeometryDataBuffer(attr_count, geom->GetIBO(), geom->GetVDM());
    gdb->geometry_id = geom->GetGeometryID();

    // 3. 按语义（不是名字、不是材质 VIL 顺序）填表
    for (uint32_t i = 0; i < attr_count; i++)
    {
        const GeometryVertexAttributeFormat *attr = gvf.Get(i);
        if (!attr || attr->semantic == VertexSemantic::Unknown) continue;

        const VkBuffer buf = geom->GetVkBuffer(attr->semantic);
        if (buf == VK_NULL_HANDLE) continue;
        if (i >= gdb->vab_count) break;

        gdb->vab_list[i]     = buf;
        gdb->vab_offset[i]   = 0;
        gdb->vab_semantic[i] = attr->semantic;
    }

    return new Primitive(geom, material, p, gdb);   // draw_range 在 ctor 内 Set(geometry)
}
```

（真源 `src/SceneGraph/mesh/Primitive.cpp:77-108`）

**关键点**：

1. 填表按**几何自带语义**（`VertexSemantic`），而不是材质 VIL 的名字——`VIL` 一致性校验、`vif.name` 查表、格式/stride 断言全部随经典 attribute 管线退役（全树 0 命中）。
2. `vab_offset` 恒为 0：VDM 多对象时偏移由 `vertex_offset`（写入 `MeshDrawParams.vertex_base`）承担。
3. 槽位数取 `gvf.GetCount()`（属性数），不是「最大 binding 号 + 1」。
4. `geometry_id` 来自 `Geometry::RegisterMeshDrawParams()`——GPU 侧取数的唯一入口（见 §9）；`Geometry::EnsureMeshDrawParams()` 用它做「未注册才注册」的幂等封装。

---

## 8. 各层所有权关系

```
StaticMesh                       ← 持 PrimitiveList（owns）+ PipelinePtrSet（仅引用）
└── owns Primitive *
      ├── (not owned) Pipeline *          ← RenderPass::CreatePipeline() 创建，pass 内按 VkPipeline 去重缓存
      ├── (not owned) ShaderProgram *     ← ShaderProgramManager（AutoIdObjectManager<ShaderProgramID,ShaderProgram>）
      ├── (not owned) Geometry *          ← GeometryManager（AutoIdObjectManager<GeometryID,Geometry>）
      │     └── owns GeometryData *
      │           ├── 模式A: owns VAB[], owns IBO          (GeometryDataPrivateBuffer / ...BM)
      │           └── 模式B: 借用 VDM 的 VAB[]，借用 IBO  (GeometryDataVDM)
      │                 └── VDM 由上层（Geometry 创建者）管理生命周期
      ├── owns GeometryDataBuffer *       ← Primitive 创建时 new，析构时 delete
      └── value GeometryDrawRange         ← 内嵌值，无动态分配
```

ECS 渲染路径用同一套底层对象，但不经 `Primitive` 对象：

```
PrimitiveComponent
      ├── (not owned) PrimitiveAsset *    ← 几何 + recipe 的资产级配对
      ├── (not owned) Geometry *          ← runtime_geometry
      └── owns GeometryDataBuffer *       ← runtime_data_buffer（SAFE_CLEAR 释放）
```

**没有 `PrimitiveManager`**：`Primitive` 的持有者是 `StaticMesh`（`CreatePrimitive()` / `AddPrimitive()` 接管生命周期，`inc/hgl/graph/mesh/StaticMesh.h:65-80`）；ECS 侧由 `PrimitiveComponent` 自行维护运行时绑定缓存。

---

## 9. 渲染时数据访问路径

顶点取数已走 **BDA 行寻址 + mesh shader**，分「注册」与「提交」两步：

**注册（几何创建 / 顶点数据变化时一次）**

```
Geometry::RegisterMeshDrawParams(GlobalSSBOBufferRegistry *, VulkanDevice *)
                                            src/SceneGraph/VKGeometry.cpp:112-175
  ├── 逐语义取 BDA：addr_position / addr_uv / addr_ntb / addr_color /
  │                 addr_luminance / addr_transform_id / addr_size
  ├── 索引：addr_index = ibo->GetVkBuffer() 的 BDA
  ├── 计数与偏移：index_base = GetFirstIndex()、vertex_base = GetVertexOffset()、
  │              is_indexed、total_vertices
  ├── meshlet（若有）：addr_meshlets / addr_meshlet_vertices / addr_meshlet_triangles
  └── geometry_id = pool->Acquire(params)（已注册则 pool->Write(geometry_id, params)）
```

**提交（每帧，ECS 批内一条 multi-draw）**

```
PrimitiveBatchPipeline::WriteMeshDrawCommands()
                                            src/ecs/support/PrimitiveBatchPipeline.cpp
  → 每几何一行 MeshDrawCommand{ geometry_id, first_instance }（8B，按 gl_DrawID 索引）
  → 一条 vkCmdDrawMeshTasksIndirectEXT multi-draw
      └── mesh shader：pc_root.addr_mesh_draw_params + geometry_id 解引用该行
          （MeshDrawParamsRef，buffer_reference_align=16），
          顶点/索引/meshlet 地址全部来自行内 BDA
```

**关键点**：SSBO 顶点输入路径下**没有** `vkCmdBindVertexBuffers` / `vkCmdBindIndexBuffer`（全树 0 命中）；`GeometryDataBuffer.vab_list` 只用于运行时对照/排序，真正被 shader 读的是 `MeshDrawParams` 行。行结构由 GLSL 侧字段表生成，与 C++ 行结构同源（`src/ShaderGen/meshgen/MeshShaderVertexAdapter.h:39-70`）。

在 `GeometryDataVDM` 模式下，多个几何共用同一 VDM 大 `VkBuffer`，但各自持有一行 `MeshDrawParams`，`vertex_base` 为其在大缓冲中的起始顶点号——shader 用 `data[gl_VertexIndex]` 绝对寻址即可（寻址契约见 §5.1）。

---

## 10. 两种缓冲区模式对比

| 维度 | 私有缓冲区（GeometryDataPrivateBuffer/...BM）| VDM 共享缓冲区（GeometryDataVDM）|
|------|----------------------------|---------------------------------|
| VkBuffer 对象数 | 每个 Geometry 独占 N 个 VAB | 全场景共享 N 个大 VAB |
| `vertex_offset` | 0 | `BlockAllocator::UserNode::GetStart()` |
| GPU 取数 | 各自一行 `MeshDrawParams`，行内 BDA 指向私有 VkBuffer | 同一大 VkBuffer 的 BDA + 行内 `vertex_base` 偏移 |
| 批次合并 | 相同 `geometry_id` 的行可连号提交（一条 multi-draw） | 相同 VDM 的行共享 buffer，靠 `vertex_base` 区分 |
| 动态增删 | 随意，创建/删除 Geometry 互不影响 | 受 BlockAllocator 碎片影响 |
| 适用场景 | 少量大几何体；动态生成/删除 | 大量小几何体批量渲染（地形/粒子/UI） |
| `GeometryDataBuffer::vdm` | `nullptr` | 指向所属 VDM（用于批次排序） |
---

## 11. 本次校对记录（2026-09，以当前代码为准）

| 旧说法 | 现状 | 依据 |
|---|---|---|
| `MaterialInstance *mat_inst` | `ShaderProgram *material_program`（材质参数值改走材质数据行 + `GlobalSSBOBinding`） | `inc/hgl/graph/mesh/Primitive.h:19-33` |
| `PipelineManager` 管理 Pipeline 生命周期 / 独立 `RasterState` | 无 `PipelineManager`、无 `RasterState`：`RenderPass::CreatePipeline()` 创建并在 pass 内去重；静态状态在 `MaterialPipelineConfig`，动态状态走 EDS | `src/Vulkan/VKRenderPass.cpp:50/101/121`、`inc/hgl/vk/pipeline/VKPipeline.h` |
| `VIL` / `GetVIL()` / `VertexInputGroup` / `vif.name` 查 VAB | 已退役（全树 0 命中）；改成 `GeometryVertexFormat` 按 `VertexSemantic` 寻址 | `inc/hgl/graph/geo/VKGeometryData.h:26-77`、`src/SceneGraph/mesh/Primitive.cpp:77-108` |
| `vkCmdBindVertexBuffers` / `vkCmdBindIndexBuffer` + `vkCmdDrawIndexed` | 顶点输入统一为 SSBO：`Geometry::RegisterMeshDrawParams()` 注册 `MeshDrawParams` 行（BDA）→ 批内 `vkCmdDrawMeshTasksIndirectEXT` multi-draw | `src/SceneGraph/VKGeometry.cpp:112-175`、`src/ecs/support/PrimitiveBatchPipeline.cpp` |
| `GeometryDataBuffer` 只有 `vab_count/vab_list/vab_offset/ibo/vdm` | 新增 `geometry_id`（MeshDrawParams 行号）与 `vab_semantic`（按语义填槽）；排序序以 `geometry_id` 打头 | `inc/hgl/graph/mesh/GeometryDataBuffer.h` |
| `GeometryCreater(…, VIL*, …)` / `WriteVAB(const AnsiString &name, …)` | 构造参数是 `GeometryVertexFormat`；`WriteVAB`/`GetVAB`/`GetTypedArrayView` 全部按 `VertexSemantic` | `inc/hgl/graph/geo/GeometryCreater.h:174-220` |
| `VertexDataManager(…, VIL*)` + `const VIL *vil; const VIF *vif_list;` | `VertexDataManager(…, const GeometryVertexFormat &)` + `GeometryVertexFormat geometry_vertex_format; uint vi_count;` | `inc/hgl/vk/VertexDataManager.h` |
| `PrimitiveManager` owns `Primitive *` | 无该管理器：`StaticMesh` 持 `PrimitiveList`（owns）与 `PipelinePtrSet`（引用）；ECS 侧 `PrimitiveComponent` 持 `runtime_data_buffer`/`runtime_geometry` | `inc/hgl/graph/mesh/StaticMesh.h:34-80`、`src/ecs/components/PrimitiveComponent.cpp:152-190` |
| `ChangeMaterialInstance()` | 已退役（0 命中），换材质走 `PrimitiveComponent::SetPrimitiveAsset()` + recipe | `inc/hgl/ecs/components/PrimitiveComponent.h:161-166` |

### 未能核实 / 不再适用

- `VIF`（`VertexInputFormat` 的“每流格式表”用法）：结构体本身仍在（`inc/hgl/vk/VKVertexInputFormat.h`），但 `VertexDataManager` 已不再持有 `const VIF *vif_list`，旧文那一栏直接删除、不替换。
- `Primitive::GetVAB()/GetIBO()` 仍在（转发到 `Geometry`），但当前渲染路径不再使用——保留描述，不宣称它们是取数路径。
