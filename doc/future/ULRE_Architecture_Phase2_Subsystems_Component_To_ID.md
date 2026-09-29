# ULRE 架构重构 - 阶段二：全子系统 Component ID 化与访问器迁移技术规范 (Phase 2 Specification)

## 一、 阶段目标与背景 (Goals & Background)

在阶段一（Transform 体系）完成 ID 化与 Accessor 化验证后，本阶段将该范式**全面推广至引擎的所有其他子系统**。
* **现状痛点**：`RenderableComponent`, `MaterialComponent`, `BoundingBoxComponent`, `VisibilityComponent`, `CameraComponent` 等依然以 OOP 形式存在，每帧的渲染收集（Render Collect）仍充斥着多态虚函数分发与局部容器扩容开销。
* **阶段目标**：
  1. 彻底废除所有渲染子系统的 OOP Component；
  2. 建立 `GeometryAccessor`, `MaterialAccessor`, `CameraAccessor`, `LightAccessor` 等轻量 4 字节句柄；
  3. 底层数据全面收敛至中心化的 Storage 平铺数组（直连 GPU SSBO / BDA 物理内存）；
  4. 视锥剔除与渲染批次生成全面基于连续数组处理，完全脱离 Entity 对象遍历。

---

## 二、 几何网格子系统重构（Geometry / Mesh）

### 2.1 废除类与结构
* **彻底废除**：`PrimitiveComponent.h`, `RenderableComponent.h`, `InstancedPrimitiveComponent.h`。

### 2.2 几何数据收敛与 GeometryDescriptor
由 `GeometryManager` 维护全局连续的几何描述符数组：
```cpp
namespace hgl::ecs {
    struct alignas(16) GeometryDescriptor {
        uint64_t vertex_buffer_address; // BDA 顶点缓冲区物理地址
        uint64_t index_buffer_address;  // BDA 索引缓冲区物理地址
        uint32_t first_index;
        uint32_t index_count;
        uint32_t meshlet_offset;
        uint32_t meshlet_count;
        AABB     local_aabb;            // 局部包围盒
    };

    struct GeometryAccessor {
        uint32_t id = ~0u;
        constexpr GeometryAccessor() = default;
        constexpr explicit GeometryAccessor(uint32_t in_id) : id(in_id) {}

        bool IsValid() const { return id != ~0u; }
        const GeometryDescriptor& GetDescriptor() const;
        const AABB& GetLocalBounds() const;
        uint32_t GetIndexCount() const;
    };
    static_assert(sizeof(GeometryAccessor) == 4, "GeometryAccessor must be 4 bytes");
}
```

---

## 三、 材质子系统重构（Material）

### 3.1 废除类与数据直连 GPU SSBO
* **彻底废除**：`MaterialComponent.h` 中的面向对象封装；
* **物理数据模型**：实体的 `MaterialID` 直接作为 GPU 端 `MaterialSSBO` 连续内存的绝对行号（Row Index）。

### 3.2 MaterialAccessor 规范
```cpp
namespace hgl::ecs {
    struct MaterialAccessor {
        uint32_t id = ~0u;
        constexpr MaterialAccessor() = default;
        constexpr explicit MaterialAccessor(uint32_t in_id) : id(in_id) {}

        bool IsValid() const { return id != ~0u; }

        void SetBaseColor(const Vector4f& color);
        void SetRoughness(float roughness);
        void SetMetallic(float metallic);
        void SetNormalTextureIndex(uint32_t texture_id);
    };
    static_assert(sizeof(MaterialAccessor) == 4, "MaterialAccessor must be 4 bytes");
}
```

---

## 四、 包围盒与可见性剔除子系统重构（BoundingBox & Visibility）

### 4.1 废除类与连续内存平铺
* **彻底废除**：`BoundingBoxComponent.h`, `VisibilityComponent.h`；
* **数据收敛**：
  * 在 `BoundingBoxDataStorage` 中存放与 Transform 槽位 1:1 对应的连续 `WorldAABB[TotalCapacity]`；
  * `PrimitiveCullSystem` / Compute Culling 仅对该连续内存块执行 SIMD 视锥判定，输出可见性索引数组；
  * 整个剔除流程完全不涉及对 Entity 对象的随机解引用。

---

## 五、 相机与环境子系统重构（Camera & Light）

### 5.1 废除类与全局槽位映射
* **彻底废除**：`CameraComponent.h` 的深层虚继承；
* **数据模型**：
  * 场景支持最多 $K$ 个活动相机与光源，其投影参数、视口范围存放在固定大小的 `CameraInfoStorage` 中；
  * 实体仅持有 `CameraID`（4 字节）或 `LightID`（4 字节），通过 `CameraAccessor` 读写对应槽位。

---

## 六、 阶段二渲染收集闭环验证

在完成上述改造后，渲染主循环收集流程将达到极度紧凑：
```cpp
// 渲染项生成流程：无任何多态对象拼装，纯 ID 组合
void BuildRenderItemsBatch(const uint32_t* visible_indices, uint32_t visible_count) {
    for (uint32_t i = 0; i < visible_count; ++i) {
        uint32_t entity_idx = visible_indices[i];
        const auto& entity = g_Entities[entity_idx];

        // 提取 4-ID：直接是整数取值，没有任何虚函数或动态类型查询
        uint32_t transform_id = entity.GetTransform().id;
        uint32_t geometry_id  = entity.GetGeometry().id;
        uint32_t material_id  = entity.GetMaterial().id;
        uint32_t pipeline_id  = g_GeometryStorage->descriptors[geometry_id].pipeline_id;

        g_RenderQueue->PushDraw(transform_id, geometry_id, material_id, pipeline_id);
    }
}
```

---

## 七、 AI Coding Agent 落地任务清单 (Task Checklist)

* [ ] **Task 2.1**：在 `GeometryManager` 中确立连续 `GeometryDescriptor` 数组，实现 `GeometryAccessor`（4 字节），废除 `PrimitiveComponent`。
* [ ] **Task 2.2**：将 `MaterialSSBO` 槽位管理抽象为 `MaterialAccessor`（4 字节），废除 `MaterialComponent`。
* [ ] **Task 2.3**：重构 `BoundingBoxDataStorage`，实现连续 `WorldAABB[]`，废除 `BoundingBoxComponent` 与 `VisibilityComponent`。
* [ ] **Task 2.4**：实现 `CameraAccessor` 与 `LightAccessor`，废除对应 OOP 组件。
* [ ] **Task 2.5**：重构 `RenderPrimitiveCollectSystem`，基于 4-ID 平铺逻辑重写渲染提交，验证静态与动态材质网格渲染正常。

