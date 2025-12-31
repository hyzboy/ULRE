# ECS RenderCollector Redesign Documentation

## 概述 / Overview

本文档描述了对 `inc/hgl/ecs` 中的 `RenderCollector` 和 `RenderItem` 的重新设计，以支持新的 `PrimitiveComponent` 并实现类似于旧的 `inc/hgl/graph` 系统的批处理功能。

This document describes the redesign of `RenderCollector` and `RenderItem` in `inc/hgl/ecs` to support the new `PrimitiveComponent` with batching capabilities similar to the old `inc/hgl/graph` system.

## 设计参考 / Design Reference

### 旧系统 (Old System - inc/hgl/graph)

- **DrawNode**: 抽象基类，定义渲染节点接口
- **DrawNodePrimitive**: 具体实现，用于 PrimitiveComponent
- **RenderBatchMap**: 按材质/管线分组的批处理映射
- **PipelineMaterialBatch**: 管理同一材质/管线的渲染项列表
- **RenderCollector**: 收集场景节点并按材质批处理

### 新系统 (New System - inc/hgl/ecs)

- **RenderItem**: 抽象基类（类似 DrawNode）
- **PrimitiveRenderItem**: 具体实现（类似 DrawNodePrimitive）
- **MaterialBatch**: 管理同一材质/管线的渲染项
- **MaterialPipelineKey**: 用于批处理索引的键
- **RenderCollector**: 收集实体并支持材质批处理

## 主要变化 / Key Changes

### 1. RenderItem 类层次 / RenderItem Class Hierarchy

#### 之前 (Before)
```cpp
struct RenderItem {
    std::shared_ptr<Entity> entity;
    std::shared_ptr<TransformComponent> transform;
    std::shared_ptr<RenderableComponent> renderable;
    glm::mat4 worldMatrix;
    float distanceToCamera;
    bool isVisible;
};
```

#### 之后 (After)
```cpp
// 抽象基类
class RenderItem {
public:
    virtual ~RenderItem() = default;
    
    // 抽象接口
    virtual std::shared_ptr<Entity> GetEntity() const = 0;
    virtual std::shared_ptr<TransformComponent> GetTransform() const = 0;
    virtual std::shared_ptr<RenderableComponent> GetRenderable() const = 0;
    virtual glm::mat4 GetWorldMatrix() const = 0;
    
    // 材质批处理支持
    virtual hgl::graph::Primitive* GetPrimitive() const = 0;
    virtual hgl::graph::MaterialInstance* GetMaterialInstance() const = 0;
    virtual hgl::graph::Material* GetMaterial() const = 0;
    virtual hgl::graph::Pipeline* GetPipeline() const = 0;
    
    // 排序比较
    virtual int Compare(const RenderItem& other) const;
    
    // 公共成员
    uint32_t index = 0;
    glm::vec3 worldPosition{};
    float distanceToCamera = 0.0f;
    bool isVisible = true;
};

// 具体实现
class PrimitiveRenderItem : public RenderItem {
    // ... 实现所有虚函数
};
```

### 2. 材质批处理 / Material Batching

新增的批处理结构：

```cpp
// 材质/管线键（用于批处理索引）
struct MaterialPipelineKey {
    hgl::graph::Material* material;
    hgl::graph::Pipeline* pipeline;
    
    bool operator<(const MaterialPipelineKey& other) const;
};

// 材质批次（保存相同材质/管线的渲染项）
class MaterialBatch {
    void AddItem(RenderItem* item);
    void Finalize();  // 排序并准备渲染
    const std::vector<RenderItem*>& GetItems() const;
};
```

### 3. RenderCollector 增强 / RenderCollector Enhancements

#### 新增成员 (New Members)
```cpp
// 使用多态指针存储渲染项
std::vector<std::unique_ptr<RenderItem>> renderItems;

// 材质批处理支持（类似 RenderBatchMap）
std::map<MaterialPipelineKey, std::unique_ptr<MaterialBatch>> materialBatches;

bool batchingEnabled;
uint32_t renderableCount;
```

#### 新增方法 (New Methods)
```cpp
void SetBatchingEnabled(bool enabled);
const std::map<MaterialPipelineKey, std::unique_ptr<MaterialBatch>>& GetMaterialBatches() const;
uint32_t GetRenderableCount() const;
bool IsEmpty() const;
void Clear();
void UpdateTransformData();
void UpdateMaterialInstanceData(PrimitiveComponent* comp);
```

#### 内部方法 (Internal Methods)
```cpp
void BuildMaterialBatches();  // 从渲染项构建材质批次
void FinalizeBatches();        // 最终化批次（排序）
```

## 使用示例 / Usage Example

### 基本收集 / Basic Collection

```cpp
// 创建并初始化 RenderCollector
auto collector = std::make_shared<RenderCollector>();
collector->SetWorld(world);
collector->Initialize();

// 设置相机信息
collector->SetCameraInfo(&cameraInfo);

// 启用功能
collector->SetFrustumCullingEnabled(true);
collector->SetDistanceSortingEnabled(true);
collector->SetBatchingEnabled(true);

// 收集可渲染对象
collector->CollectRenderables();
```

### 访问渲染项 / Accessing Render Items

```cpp
// 方式 1: 直接访问渲染项列表
const auto& items = collector->GetRenderItems();
for (const auto& itemPtr : items) {
    if (itemPtr && itemPtr->isVisible) {
        auto entity = itemPtr->GetEntity();
        auto transform = itemPtr->GetTransform();
        // ... 使用渲染项
    }
}

// 方式 2: 使用材质批次（更高效）
const auto& batches = collector->GetMaterialBatches();
for (const auto& pair : batches) {
    const MaterialPipelineKey& key = pair.first;
    const MaterialBatch& batch = *pair.second;
    
    // 设置材质和管线
    // key.material, key.pipeline
    
    // 渲染批次中的所有项
    for (RenderItem* item : batch.GetItems()) {
        // ... 渲染项
    }
}
```

### 更新变换数据 / Updating Transform Data

```cpp
// 当变换改变时更新所有渲染项
collector->UpdateTransformData();

// 当材质实例改变时更新特定组件
PrimitiveComponent* comp = ...;
collector->UpdateMaterialInstanceData(comp);
```

## 与旧系统的对应关系 / Mapping to Old System

| 旧系统 (Old) | 新系统 (New) | 说明 (Description) |
|-------------|-------------|-------------------|
| `DrawNode` | `RenderItem` | 抽象渲染节点基类 |
| `DrawNodePrimitive` | `PrimitiveRenderItem` | PrimitiveComponent 的渲染节点 |
| `RenderBatchMap` | `std::map<MaterialPipelineKey, MaterialBatch>` | 材质批处理映射 |
| `PipelineMaterialBatch` | `MaterialBatch` | 材质批次 |
| `PipelineMaterialIndex` | `MaterialPipelineKey` | 批处理索引键 |
| `Expand(SceneNode*)` | `CollectRenderables()` | 收集渲染对象 |
| `Clear()` | `Clear()` | 清理收集的数据 |
| `UpdateTransformData()` | `UpdateTransformData()` | 更新变换数据 |
| `UpdateMaterialInstanceData()` | `UpdateMaterialInstanceData()` | 更新材质实例数据 |

## 优势 / Advantages

1. **多态设计**: RenderItem 是抽象基类，可以轻松扩展支持其他类型的可渲染组件
2. **材质批处理**: 通过材质/管线分组提高渲染效率
3. **兼容性**: API 设计参考旧的 graph 系统，便于迁移
4. **灵活性**: 可以选择使用原始列表或批次进行渲染
5. **性能**: 使用 unique_ptr 管理内存，避免不必要的拷贝

## 扩展性 / Extensibility

### 添加新的 RenderItem 类型

```cpp
// 示例：为粒子系统创建 ParticleRenderItem
class ParticleRenderItem : public RenderItem {
private:
    std::shared_ptr<ParticleComponent> particleComp;
    
public:
    // 实现抽象接口
    std::shared_ptr<Entity> GetEntity() const override { ... }
    // ... 其他方法
    
    // ParticleComponent 特定方法
    ParticleComponent* GetParticleComponent() const { return particleComp.get(); }
};

// 在 CollectRenderables 中添加支持
auto particleComp = std::dynamic_pointer_cast<ParticleComponent>(renderable);
if (particleComp && particleComp->CanRender()) {
    auto item = std::make_unique<ParticleRenderItem>(...);
    renderItems.push_back(std::move(item));
}
```

## 注意事项 / Notes

1. **多态开销**: 虚函数调用有轻微性能开销，但对于渲染系统来说可以忽略
2. **内存管理**: RenderItem 使用 unique_ptr，MaterialBatch 中使用原始指针（生命周期由 RenderCollector 管理）
3. **线程安全**: 当前实现不是线程安全的，如需多线程需要额外同步
4. **批次更新**: 每次 CollectRenderables() 都会重建批次，如果场景不变可以优化

## 未来改进 / Future Improvements

1. 持久化批次（场景不变时避免重建）
2. 多线程收集和批处理
3. 更高级的排序策略（Z-buffer 优化、透明度排序等）
4. GPU instancing 支持
5. 级联剔除（CSG、Portal 等）
