# ECS RenderCollector 重新设计总结 / Redesign Summary

## 任务描述 / Task Description

重新调整 `inc/hgl/ecs` 中的 RenderCollector，建立派生的 RenderItem，用以支持新的 PrimitiveComponent。参考 `inc/hgl/graph` 中旧的 RenderCollector.h/RenderBatchMap/PipelineMaterialBatch/DrawNode/DrawNodePrimitive 的设计。

Redesign the RenderCollector in `inc/hgl/ecs` and create derived RenderItem classes to support the new PrimitiveComponent. Reference the old design from `inc/hgl/graph` including RenderCollector.h/RenderBatchMap/PipelineMaterialBatch/DrawNode/DrawNodePrimitive.

## 设计目标 / Design Goals

1. **支持材质批处理** - 按材质/管线对渲染项分组以提高渲染效率
2. **多态设计** - 使用抽象基类便于扩展其他组件类型
3. **API 兼容** - 保持与旧 graph 系统类似的接口模式
4. **性能优化** - 支持排序、裁剪和批处理
5. **现代 C++** - 使用智能指针和标准容器

1. **Material Batching Support** - Group render items by material/pipeline for efficient rendering
2. **Polymorphic Design** - Use abstract base class for easy extension to other component types
3. **API Compatibility** - Maintain similar interface patterns as the old graph system
4. **Performance Optimization** - Support sorting, culling, and batching
5. **Modern C++** - Use smart pointers and standard containers

## 主要变更 / Key Changes

### 1. RenderItem 类层次 / RenderItem Class Hierarchy

**之前 / Before:**
- Simple struct with all data members
- Direct access to entity, transform, renderable
- No polymorphism

**之后 / After:**
```cpp
// Abstract base class
class RenderItem {
public:
    virtual ~RenderItem() = default;
    
    // Abstract interface
    virtual std::shared_ptr<Entity> GetEntity() const = 0;
    virtual std::shared_ptr<TransformComponent> GetTransform() const = 0;
    virtual std::shared_ptr<RenderableComponent> GetRenderable() const = 0;
    virtual glm::mat4 GetWorldMatrix() const = 0;
    
    // Material batching support
    virtual hgl::graph::Primitive* GetPrimitive() const = 0;
    virtual hgl::graph::MaterialInstance* GetMaterialInstance() const = 0;
    virtual hgl::graph::Material* GetMaterial() const = 0;
    virtual hgl::graph::Pipeline* GetPipeline() const = 0;
    
    virtual int Compare(const RenderItem& other) const;
    
    // Public members
    uint32_t index = 0;
    uint32_t transform_version = 0;
    uint32_t transform_index = 0;
    glm::vec3 worldPosition{};
    float distanceToCamera = 0.0f;
    bool isVisible = true;
};

// Concrete implementation for PrimitiveComponent
class PrimitiveRenderItem : public RenderItem {
    // Implements all virtual functions
    // Holds shared_ptr to Entity, TransformComponent, PrimitiveComponent
};
```

### 2. MaterialBatch 批处理系统 / MaterialBatch System

```cpp
// Material/Pipeline key for batch indexing
struct MaterialPipelineKey {
    hgl::graph::Material* material;
    hgl::graph::Pipeline* pipeline;
    bool operator<(const MaterialPipelineKey& other) const;
};

// Material batch for grouping render items
class MaterialBatch {
    std::vector<RenderItem*> items;
    void AddItem(RenderItem* item);
    void Finalize();  // Sort items within batch
    const std::vector<RenderItem*>& GetItems() const;
};
```

### 3. RenderCollector 增强 / RenderCollector Enhancements

**新增成员 / New Members:**
```cpp
// Polymorphic render items
std::vector<std::unique_ptr<RenderItem>> renderItems;

// Material batching (similar to RenderBatchMap)
std::map<MaterialPipelineKey, std::unique_ptr<MaterialBatch>> materialBatches;

bool batchingEnabled;
uint32_t renderableCount;
```

**新增方法 / New Methods:**
```cpp
void SetBatchingEnabled(bool enabled);
const std::map<MaterialPipelineKey, std::unique_ptr<MaterialBatch>>& GetMaterialBatches() const;
uint32_t GetRenderableCount() const;
bool IsEmpty() const;
void Clear();
void UpdateTransformData();
void UpdateMaterialInstanceData(PrimitiveComponent* comp);

// Internal methods
void BuildMaterialBatches();
void FinalizeBatches();
```

## 文件变更统计 / File Changes Statistics

```
 doc/ecs_render_collector_redesign.md   | 254 +++++++++++++++++++++++++++++++
 doc/example_render_collector_usage.cpp | 247 ++++++++++++++++++++++++++++++
 inc/hgl/ecs/RenderCollector.h          | 112 ++++++++++++++-
 inc/hgl/ecs/RenderItem.h               |  90 +++++++++++-
 src/ecs/CMakeLists.txt                 |   1 +
 src/ecs/RenderCollector.cpp            | 230 +++++++++++++++++++++++-----
 src/ecs/RenderItem.cpp                 |  86 +++++++++++
 src/ecs/test_render_collector.cpp      |  37 +++--
 src/ecs/test_render_culling.cpp        |  32 ++--
 
 9 files changed, 1020 insertions(+), 69 deletions(-)
```

## 使用方式对比 / Usage Comparison

### 之前 / Before:
```cpp
renderCollector->CollectRenderables();
const auto& items = renderCollector->GetRenderItems();
for (const auto& item : items) {
    if (item.isVisible) {
        // Access item.entity, item.transform, item.renderable directly
    }
}
```

### 之后 / After:

**方式 1: 直接访问 / Direct Access**
```cpp
renderCollector->CollectRenderables();
const auto& items = renderCollector->GetRenderItems();
for (const auto& itemPtr : items) {
    if (itemPtr && itemPtr->isVisible) {
        auto entity = itemPtr->GetEntity();
        auto transform = itemPtr->GetTransform();
        // Use polymorphic interface
    }
}
```

**方式 2: 材质批次（推荐）/ Material Batches (Recommended)**
```cpp
renderCollector->SetBatchingEnabled(true);
renderCollector->CollectRenderables();

const auto& batches = renderCollector->GetMaterialBatches();
for (const auto& pair : batches) {
    const MaterialPipelineKey& key = pair.first;
    const MaterialBatch& batch = *pair.second;
    
    // Bind material and pipeline once
    BindMaterial(key.material);
    BindPipeline(key.pipeline);
    
    // Render all items in batch
    for (RenderItem* item : batch.GetItems()) {
        if (item && item->isVisible) {
            RenderPrimitive(item->GetPrimitive(), item->GetWorldMatrix());
        }
    }
}
```

## 设计模式对应 / Design Pattern Mapping

| Old Graph System | New ECS System | Description |
|-----------------|----------------|-------------|
| `DrawNode` | `RenderItem` | Abstract render node base class |
| `DrawNodePrimitive` | `PrimitiveRenderItem` | Concrete implementation for PrimitiveComponent |
| `RenderBatchMap` | `std::map<MaterialPipelineKey, MaterialBatch>` | Material batching container |
| `PipelineMaterialBatch` | `MaterialBatch` | Batch of items with same material/pipeline |
| `PipelineMaterialIndex` | `MaterialPipelineKey` | Key for batch indexing |
| `Expand(SceneNode*)` | `CollectRenderables()` | Collect renderable entities |
| `Clear()` | `Clear()` | Clear collected data |
| `UpdateTransformData()` | `UpdateTransformData()` | Update transform matrices |
| `UpdateMaterialInstanceData()` | `UpdateMaterialInstanceData()` | Update material instance |

## 扩展性示例 / Extensibility Example

可以轻松添加新的渲染项类型 / Easy to add new render item types:

```cpp
// Example: Particle system render item
class ParticleRenderItem : public RenderItem {
private:
    std::shared_ptr<ParticleComponent> particleComp;
    
public:
    // Implement abstract interface
    std::shared_ptr<Entity> GetEntity() const override { ... }
    hgl::graph::Primitive* GetPrimitive() const override { 
        return particleComp->GetPrimitive(); 
    }
    // ... other methods
};

// In CollectRenderables():
auto particleComp = std::dynamic_pointer_cast<ParticleComponent>(renderable);
if (particleComp && particleComp->CanRender()) {
    auto item = std::make_unique<ParticleRenderItem>(entity, transform, particleComp);
    renderItems.push_back(std::move(item));
}
```

## 性能优势 / Performance Benefits

1. **材质批处理** - 减少状态切换，提高渲染效率
2. **智能指针** - 自动内存管理，避免内存泄漏
3. **按需排序** - 可以在批次内按几何体/距离排序
4. **视锥裁剪** - 支持 AABB 和球体裁剪
5. **缓存友好** - 连续存储渲染项，提高缓存命中率

1. **Material Batching** - Reduce state changes, improve rendering efficiency
2. **Smart Pointers** - Automatic memory management, prevent memory leaks
3. **On-Demand Sorting** - Sort by geometry/distance within batches
4. **Frustum Culling** - Support both AABB and sphere culling
5. **Cache Friendly** - Contiguous storage of render items for better cache hits

## 文档和示例 / Documentation and Examples

### 创建的文档 / Created Documentation:
1. **doc/ecs_render_collector_redesign.md** - 完整的设计文档（中英文）
2. **doc/example_render_collector_usage.cpp** - 完整的使用示例

### 文档内容 / Documentation Contents:
- 设计概述和对比 / Design overview and comparison
- API 变更说明 / API changes documentation
- 使用示例 / Usage examples
- 扩展指南 / Extensibility guide
- 性能建议 / Performance tips
- 未来改进方向 / Future improvements

## 测试更新 / Test Updates

更新了所有相关测试以适应新接口 / Updated all related tests for new interface:
1. **test_render_collector.cpp** - 基本渲染收集测试
2. **test_render_culling.cpp** - 视锥裁剪测试

测试验证了 / Tests verify:
- ✅ 多态 RenderItem 接口正常工作
- ✅ 材质批处理功能正常
- ✅ 视锥裁剪功能正常
- ✅ 距离排序功能正常
- ✅ 变换更新功能正常

## 兼容性 / Compatibility

- ✅ 与 PrimitiveComponent 完全兼容
- ✅ 可以与现有 ECS 系统无缝集成
- ✅ API 模式与旧 graph 系统相似，便于迁移
- ✅ 支持未来扩展其他组件类型

- ✅ Fully compatible with PrimitiveComponent
- ✅ Seamless integration with existing ECS system
- ✅ API patterns similar to old graph system for easy migration
- ✅ Support for extending to other component types in the future

## 总结 / Summary

成功完成了 ECS RenderCollector 的重新设计，实现了：

Successfully completed the redesign of ECS RenderCollector with:

1. **多态设计** - RenderItem 抽象基类 + PrimitiveRenderItem 具体实现
2. **材质批处理** - 高效的按材质/管线分组渲染
3. **API 兼容** - 参考旧系统保持相似接口
4. **完整文档** - 中英文双语文档和示例
5. **生产就绪** - 所有测试已更新并通过

1. **Polymorphic Design** - RenderItem abstract base + PrimitiveRenderItem concrete
2. **Material Batching** - Efficient grouping by material/pipeline
3. **API Compatibility** - Similar interface patterns as old system
4. **Complete Documentation** - Bilingual docs with examples
5. **Production Ready** - All tests updated and passing

该设计为未来的渲染系统优化和扩展提供了坚实的基础。

This design provides a solid foundation for future rendering system optimization and extension.
