# RenderPrimitiveSystem - Specialized ECS Rendering

## 概述 / Overview

按照 ECS 设计原则，创建了专门的 `RenderPrimitiveSystem` 来处理 `PrimitiveComponent` 的渲染。

Following ECS design principles, created a dedicated `RenderPrimitiveSystem` to handle `PrimitiveComponent` rendering.

## 设计理念 / Design Philosophy

### ECS 原则 / ECS Principle

**一个系统处理一种组件类型 / One System Per Component Type**

在 ECS 架构中，每个系统应该专注于处理特定类型的组件。这样做的好处：

In ECS architecture, each system should focus on handling specific component types. Benefits:

1. **职责分离** - 每个系统有明确的职责
2. **易于维护** - 修改一种渲染不影响其他
3. **性能优化** - 针对特定类型优化
4. **可扩展性** - 容易添加新的渲染系统

1. **Separation of Concerns** - Each system has clear responsibility
2. **Easy Maintenance** - Changes to one renderer don't affect others
3. **Performance** - Optimize for specific types
4. **Extensibility** - Easy to add new rendering systems

## 系统对比 / System Comparison

### 之前 / Before: Generic RenderCollector

```cpp
class RenderCollector : public System {
    // Handles ALL renderable components
    std::vector<std::unique_ptr<RenderItem>> renderItems;
    
    void CollectRenderables() {
        // Check for any RenderableComponent
        auto renderable = entity->GetComponent<RenderableComponent>();
        
        // Special handling for PrimitiveComponent
        if (auto primitiveComp = std::dynamic_pointer_cast<PrimitiveComponent>(renderable)) {
            // ...
        }
    }
};
```

**问题 / Problems:**
- 混合处理不同类型的组件
- 需要运行时类型检查 (dynamic_pointer_cast)
- 难以针对特定类型优化

- Mixed handling of different component types
- Requires runtime type checking (dynamic_pointer_cast)
- Hard to optimize for specific types

### 之后 / After: Specialized Systems

```cpp
// Specialized system for primitives
class RenderPrimitiveSystem : public System {
    std::vector<std::unique_ptr<PrimitiveRenderItem>> renderItems;
    
    void CollectPrimitives() {
        // Directly get PrimitiveComponent
        auto primitiveComp = entity->GetComponent<PrimitiveComponent>();
        // No type casting needed!
    }
};

// Future: Other specialized systems
class RenderParticleSystem : public System {
    void CollectParticles() {
        auto particleComp = entity->GetComponent<ParticleComponent>();
    }
};

class RenderLineSystem : public System {
    void CollectLines() {
        auto lineComp = entity->GetComponent<LineComponent>();
    }
};
```

**优势 / Advantages:**
- 每个系统专注于一种组件
- 无需运行时类型转换
- 更好的性能和类型安全

- Each system focuses on one component type
- No runtime type casting
- Better performance and type safety

## 架构示意图 / Architecture Diagram

```
World
  ├─ Entities
  │    ├─ Entity 1: TransformComponent + PrimitiveComponent
  │    ├─ Entity 2: TransformComponent + ParticleComponent
  │    ├─ Entity 3: TransformComponent + LineComponent
  │    └─ Entity 4: TransformComponent + PrimitiveComponent
  │
  └─ Systems
       ├─ RenderPrimitiveSystem  ──→ Handles Entity 1, 4
       ├─ RenderParticleSystem   ──→ Handles Entity 2
       └─ RenderLineSystem        ──→ Handles Entity 3
```

## 使用示例 / Usage Example

### 创建和使用 RenderPrimitiveSystem

```cpp
// 1. Create world
auto world = std::make_shared<World>("GameWorld");
world->Initialize();

// 2. Register specialized rendering system
auto renderPrimitiveSystem = world->RegisterSystem<RenderPrimitiveSystem>();
renderPrimitiveSystem->SetWorld(world);
renderPrimitiveSystem->Initialize();

// 3. Set up camera
CameraInfo camera;
// ... configure camera
renderPrimitiveSystem->SetCameraInfo(&camera);

// 4. Create entities with PrimitiveComponent
auto entity = world->CreateEntity<Entity>("MeshObject");
auto transform = entity->AddComponent<TransformComponent>();
auto primitiveComp = entity->AddComponent<PrimitiveComponent>();
// ... set primitive data

// 5. Collect and render primitives
renderPrimitiveSystem->CollectPrimitives();

// Method 1: Direct access
const auto& items = renderPrimitiveSystem->GetRenderItems();
for (const auto& item : items) {
    if (item && item->isVisible) {
        Render(item->GetPrimitive(), item->GetWorldMatrix());
    }
}

// Method 2: Batched rendering (recommended)
renderPrimitiveSystem->SetBatchingEnabled(true);
const auto& batches = renderPrimitiveSystem->GetMaterialBatches();
for (const auto& [key, batch] : batches) {
    BindMaterial(key.material);
    BindPipeline(key.pipeline);
    
    for (RenderItem* item : batch->GetItems()) {
        DrawPrimitive(item->GetPrimitive(), item->GetWorldMatrix());
    }
}
```

## 未来扩展 / Future Extensions

### 添加新的渲染系统 / Adding New Rendering Systems

```cpp
// Example: Particle rendering system
class RenderParticleSystem : public System {
private:
    std::vector<std::unique_ptr<ParticleRenderItem>> renderItems;
    
public:
    void CollectParticles() {
        for (const auto& entity : world->GetEntities()) {
            auto transform = entity->GetComponent<TransformComponent>();
            auto particleComp = entity->GetComponent<ParticleComponent>();
            
            if (transform && particleComp && particleComp->IsActive()) {
                auto item = std::make_unique<ParticleRenderItem>(
                    entity, transform, particleComp);
                renderItems.push_back(std::move(item));
            }
        }
    }
    
    void Update(float deltaTime) override {
        CollectParticles();
        // Particle-specific rendering logic
    }
};

// Example: Line rendering system
class RenderLineSystem : public System {
private:
    std::vector<std::unique_ptr<LineRenderItem>> renderItems;
    
public:
    void CollectLines() {
        for (const auto& entity : world->GetEntities()) {
            auto transform = entity->GetComponent<TransformComponent>();
            auto lineComp = entity->GetComponent<LineComponent>();
            
            if (transform && lineComp && lineComp->IsVisible()) {
                auto item = std::make_unique<LineRenderItem>(
                    entity, transform, lineComp);
                renderItems.push_back(std::move(item));
            }
        }
    }
    
    void Update(float deltaTime) override {
        CollectLines();
        // Line-specific rendering logic (no batching needed)
    }
};
```

## API 对比 / API Comparison

### RenderCollector (通用 / Generic)

```cpp
auto collector = world->RegisterSystem<RenderCollector>();
collector->CollectRenderables(); // 收集所有类型 / Collects all types
const auto& items = collector->GetRenderItems(); // 返回 RenderItem* / Returns RenderItem*
```

### RenderPrimitiveSystem (专用 / Specialized)

```cpp
auto system = world->RegisterSystem<RenderPrimitiveSystem>();
system->CollectPrimitives(); // 只收集 PrimitiveComponent / Only collects PrimitiveComponent
const auto& items = system->GetRenderItems(); // 返回 PrimitiveRenderItem* / Returns PrimitiveRenderItem*
```

## 性能优势 / Performance Benefits

1. **无类型转换开销** - 不需要 dynamic_pointer_cast
2. **更好的缓存局部性** - 相同类型的数据连续存储
3. **针对性优化** - 每个系统可以针对其组件类型优化
4. **并行处理** - 不同系统可以并行运行（未来）

1. **No Type Casting Overhead** - No need for dynamic_pointer_cast
2. **Better Cache Locality** - Same type data stored contiguously
3. **Targeted Optimization** - Each system optimized for its component type
4. **Parallel Processing** - Different systems can run in parallel (future)

## 文件列表 / File List

**新增文件 / New Files:**
- `inc/hgl/ecs/RenderPrimitiveSystem.h` - Header
- `src/ecs/RenderPrimitiveSystem.cpp` - Implementation
- `doc/RENDER_PRIMITIVE_SYSTEM.md` - This documentation

**保留文件 / Retained Files:**
- `inc/hgl/ecs/RenderCollector.h` - Generic collector (可选保留 / optional)
- All other component and helper files remain unchanged

## 总结 / Summary

`RenderPrimitiveSystem` 遵循 ECS 设计原则，为每种可渲染组件类型提供专门的系统。这种设计：

`RenderPrimitiveSystem` follows ECS design principles, providing a dedicated system for each renderable component type. This design:

✅ 更符合 ECS 架构理念 / Better aligned with ECS architecture
✅ 类型安全，无需运行时转换 / Type-safe, no runtime casting
✅ 易于扩展新的渲染类型 / Easy to extend with new rendering types
✅ 性能更优，针对性优化 / Better performance with targeted optimization
✅ 职责清晰，易于维护 / Clear responsibilities, easy to maintain
