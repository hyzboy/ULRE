# ECS (Entity-Component-System) Architecture

## ECS (Entity-Component-System) 架构（中文概要）

本目录提供 ULRE 的 ECS 基础实现，核心思路是组件化与数据驱动。

### 核心模块
- Object / Component / Entity / System / ECSContext：提供基础对象、组件、实体与系统生命周期管理
- TransformComponent + TransformDataStorage：单一共享 SOA 存储，集中管理 L2W
- RenderPrimitiveCollectSystem / RenderPrimitiveBatchSystem / RenderPrimitiveSubmitSystem：
  - Collect：收集可渲染实体
  - Batch：裁剪、排序、分批
  - Submit：提交绘制
- RenderFrameCache：ECSContext 持有的每帧共享缓存（renderItems/materialBatches 等）

### 执行流程（建议）
- Tick：Transform -> RenderPrimitiveCollect -> RenderPrimitiveBatch
- Render：RenderPrimitiveSubmit

This directory contains a basic ECS architecture implementation for the ULRE project, inspired by the hyzboy/AIECS repository design.

## Overview

The ECS pattern is a software architectural pattern commonly used in game development that follows the principle of composition over inheritance. It provides a flexible and efficient way to organize game entities and their behaviors.

## Core Components

### 1. Object (`Object.h/cpp`)
Base class for all ECS objects providing:
- Unique object IDs
- Object naming
- Lifecycle methods (OnCreate, OnUpdate, OnDestroy)

### 2. Component (`Component.h/cpp`)
Base component class for attaching data and behavior to entities:
- Lifecycle callbacks: `OnAttach()`, `OnUpdate(float)`, `OnDetach()`
- Weak reference to owner entity
- Component naming support

### 3. Entity (`Entity.h/cpp`)
Container for components representing game objects:
- Template-based component management
- `AddComponent<T>()` - Add a component of type T
- `GetComponent<T>()` - Retrieve a component of type T
- `HasComponent<T>()` - Check if entity has a component
- `RemoveComponent<T>()` - Remove a component
- Fast component lookup using `typeid` hash codes

### 4. System (`System.h/cpp`)
Base class for systems that process entities with specific components:
- `Initialize()` - Setup system
- `Update(float)` - Per-frame update
- `Shutdown()` - Cleanup

### 5. World (`World.h/cpp`)
Manager for all entities and systems:
- `CreateEntity<T>()` - Create and register new entities
- `RegisterSystem<T>()` - Register a system
- `GetSystem<T>()` - Retrieve a system by type
- `Initialize()`, `Update(float)`, `Shutdown()` - World lifecycle
- Entity and system iteration support

### 6. TransformComponent (`TransformComponent.h/cpp`)
Spatial transformation component with:
- **Local Space Transforms**: Position, Rotation (quaternion), Scale
- **World Space Transforms**: Calculated from local space and parent hierarchy
- **Parent-Child Hierarchy**: Support for transform inheritance
- **Transform Mobility**: 
  - `movable = true` - Frequently updated, recalculated each frame
  - `movable = false` - Static objects, matrix cached permanently
- **SOA Storage**: Uses TransformDataStorage for cache-friendly data layout
- **Batch Operations**: Efficient bulk updates through SOA storage
- **Matrix Caching**: Optimization for static transforms
- **Dirty Flag System**: Efficient recalculation only when needed

### 7. TransformDataStorage (`TransformDataStorage.h`)
Structure of Arrays (SOA) storage for transform data:
- **Cache-Friendly Layout**: Separate arrays for positions, rotations, scales
- **Handle-Based Access**: uint32_t handles for fast indexing
- **Batch Operations**: `UpdateMovableDirtyMatrices()` and `UpdateAllDirtyMatrices()` for efficient bulk processing
- **Mobility Optimization**: Separate tracking of static vs movable transforms
- **Zero-Copy Access**: Direct access to internal arrays for systems
- **Benefits**: 
  - Better cache performance (sequential memory access)
  - SIMD-friendly data layout
  - Easier to parallelize operations
  - Lower memory overhead per transform

### 8. RenderPrimitiveCollectSystem / RenderPrimitiveBatchSystem / RenderPrimitiveSubmitSystem
Rendering is split into three systems for clearer responsibilities:
- **Collect**: Gathers entities with TransformComponent + PrimitiveComponent into render items
- **Batch**: Frustum culls, sorts, assigns transform indices, and builds MaterialBatch lists
- **Submit**: Issues draw calls using batched data

**Note**: Uses CMMath Frustum (`hgl/math/geometry/Frustum.h`) and CameraInfo (`hgl/graph/CameraInfo.h`).

### 9. RenderFrameCache
Shared per-frame data cache owned by `ECSContext`:
- `renderItems` - Collected render items for the current frame
- `materialBatches` - Batched draws grouped by material/pipeline
- `renderableCount` - Number of collected items
- `cameraInfo` - Active camera data for the frame

### 9. PrimitiveComponent (`PrimitiveComponent.h/cpp`)
Renderable component for static mesh rendering:
- **Primitive Management**: Holds reference to `hgl::graph::Primitive` (geometry + material)
- **Material Override**: Optional MaterialInstance override without modifying Primitive
- **Rendering Support**: 
  - `CanRender()` - Checks if component has valid data and is visible
  - `Render(worldMatrix)` - Called by rendering systems
  - `GetPipeline()` - Access to rendering pipeline
- **Spatial Queries**: `GetLocalAABB()` for bounding box
- **Automatic Bounding Radius**: Updates based on primitive's AABB for frustum culling
- **Lifecycle Management**: OnAttach/OnUpdate/OnDetach hooks
- **Integration**: Works with RenderCollector and World systems
- **Simple Design**: Single class (no separate Data/Manager), derived from RenderableComponent

### 10. MaterialBatch (`MaterialBatch.h/cpp`)
Material batching system for efficient rendering:
- **Indirect Rendering Support**: Uses Vulkan indirect draw commands for optimal performance
  - `IndirectDrawBuffer` - For non-indexed geometry
  - `IndirectDrawIndexedBuffer` - For indexed geometry
- **Draw Call Batching**: Merges render items with same geometry and material
- **Batch Building**: Automatically constructs optimized draw batches
  - Merges consecutive items with identical geometry data
  - Generates indirect draw commands in GPU memory
  - Supports both indexed and non-indexed rendering
- **Renderer Integration**: Uses `PipelineMaterialRenderer` for actual rendering
- **Transform Management**: Placeholder support for `TransformAssignmentBuffer` (LocalToWorld matrices)
- **Material Instance Management**: Placeholder support for `MaterialInstanceAssignmentBuffer`
- **Features**:
  - Automatic buffer allocation (power-of-2 sizing)
  - Reuses buffers when possible to reduce allocations
  - Sorts items for optimal cache usage
  - Handles both direct and indirect rendering paths
- **Design Pattern**: Similar to SceneGraph's `PipelineMaterialBatch` for consistency

## Usage Example

```cpp
#include<hgl/ecs/World.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>

using namespace hgl::ecs;

// Create a world
auto world = std::make_shared<World>("GameWorld");

// Register systems
auto renderSystem = world->RegisterSystem<RenderSystem>();

// Initialize world
world->Initialize();

// Create entities
auto player = world->CreateEntity<Entity>("Player");

// Add components
auto transform = player->AddComponent<TransformComponent>();
transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));

// Create child entity
auto weapon = world->CreateEntity<Entity>("Weapon");
auto weaponTransform = weapon->AddComponent<TransformComponent>();
weaponTransform->SetParent(player); // Attach to player

// Access SOA storage for batch operations
auto storage = TransformComponent::GetSharedStorage();
std::cout << "Total transforms: " << storage->GetSize() << std::endl;

// Batch update all dirty transforms
storage->UpdateAllDirtyMatrices([](TransformDataStorage::HandleID id, 
                                   glm::vec3 pos, glm::quat rot, glm::vec3 scale) {
    // Process transform data in cache-friendly order
    // This is much faster than individual component updates
});

// Set up rendering systems
auto renderCollect = world->RegisterTickSystem<RenderPrimitiveCollectSystem>();
auto renderBatch = world->RegisterTickSystem<RenderPrimitiveBatchSystem>();
auto renderSubmit = world->RegisterRenderSystem<RenderPrimitiveSubmitSystem>();

// Configure camera (requires CMMath)
// CameraInfo and Frustum come from CMMath library
hgl::graph::CameraInfo camera;
// Set up camera parameters using CMMath's CameraInfo interface
// camera.position = ...
// camera.viewMatrix = ...
// camera.projectionMatrix = ...
renderCollect->SetCameraInfo(&camera);
renderBatch->SetCameraInfo(&camera);

// Add renderable component to entity
auto renderable = player->AddComponent<RenderableComponent>();
renderable->SetBoundingRadius(1.0f);

// Or use PrimitiveComponent for mesh rendering
auto primitive = player->AddComponent<PrimitiveComponent>();
// Assuming you have a hgl::graph::Primitive* loaded from file or created
hgl::graph::Primitive* mesh = LoadMeshFromFile("player.mesh");
primitive->SetPrimitive(mesh);

// Optional: Override material
hgl::graph::MaterialInstance* customMaterial = CreateCustomMaterial();
primitive->SetOverrideMaterial(customMaterial);

// Check rendering capability
if (primitive->CanRender()) {
    auto material = primitive->GetMaterial();
    auto pipeline = primitive->GetPipeline();
}

// Add bounding box for spatial queries and GPU culling
auto bbox = player->AddComponent<BoundingBoxComponent>();
bbox->SetAABB(glm::vec3(-1.0f), glm::vec3(1.0f));

// Access bounding box SOA storage for SSBO upload
auto bboxStorage = BoundingBoxComponent::GetSharedStorage();
const auto& minPoints = bboxStorage->GetAllMinPoints();
const auto& maxPoints = bboxStorage->GetAllMaxPoints();
// Upload to GPU: glBufferData(GL_SHADER_STORAGE_BUFFER, minPoints.size() * sizeof(glm::vec3), minPoints.data(), GL_STATIC_DRAW);

// Collect/Batch run during Tick, Submit runs during Render.

// Game loop
float deltaTime = 0.016f; // 16ms = 60 FPS
world->Update(deltaTime);

// Cleanup
world->Shutdown();
```

## Design Principles

1. **Self-Contained**: Uses standard C++ types (std::string, std::shared_ptr) for portability
2. **Type-Safe**: Template-based component management with compile-time type checking
3. **Efficient**: Hash-based component lookups, SOA storage, matrix caching, dirty flags
4. **Cache-Friendly**: TransformDataStorage uses Structure of Arrays layout for better performance
5. **Flexible**: Easy to extend with custom components and systems
6. **Simple**: Clear and intuitive API following ULRE conventions

## Namespace

All ECS classes are in the `hgl::ecs` namespace.

## Dependencies

- **C++17** or higher
- **GLM** library (for TransformComponent math operations)
- **CMMath** library (for CameraInfo and Frustum in RenderCollector)
- Standard C++ library

## Building

The ECS module is integrated into the ULRE build system via CMake:
- Header files: `inc/hgl/ecs/`
- Source files: `src/ecs/`
- CMake target: `ULRE.ECS`

## Testing

Comprehensive tests are included in `src/ecs/`:
- **test_ecs.cpp** - Core ECS features:
  - World and entity creation
  - Component management
  - System registration
  - Transform hierarchy
  - Parent-child relationships
  - SOA storage access and batch operations
- **test_render_collector.cpp** - Rendering system:
  - RenderCollector with camera setup
  - Frustum culling demonstration
  - Distance sorting
  - Visibility filtering
- **test_boundingbox.cpp** - Bounding box system:
  - BoundingBoxComponent with SOA storage
  - AABB creation and queries
  - Dirty flag management
  - SSBO-ready data layout demonstration
  - Direct array access for GPU uploads
- **test_primitive_component.cpp** - PrimitiveComponent usage:
  - Example demonstrating new PrimitiveComponent API
  - Comparison with old Component/Data/Manager pattern
  - Integration with World, Entity, and RenderCollector

Build and run the test:
```bash
cd src/ecs
g++ -std=c++17 -I../../inc -o test_ecs test_ecs.cpp *.cpp
./test_ecs
```

## Comparison with Legacy Component System

The new ECS architecture provides a simpler, more efficient alternative to the old Component/Data/Manager pattern:

### Old System (`inc/hgl/component/`)

**PrimitiveComponent hierarchy:**
```
Component → SceneComponent → GeometryComponent → RenderComponent → PrimitiveComponent
```

**Structure:**
- `PrimitiveComponentData` - Holds Primitive pointer and data
- `PrimitiveComponentManager` - Creates and manages components
- `PrimitiveComponent` - The component class
- Uses `SharedPtr<ComponentData>` and `WeakPtr`
- Complex macros: `COMPONENT_DATA_CLASS_BODY`, `COMPONENT_MANAGER_CLASS_BODY`

**Usage:**
```cpp
// Old pattern
auto manager = PrimitiveComponentManager::GetDefaultManager();
auto data = new PrimitiveComponentData(primitive);
ComponentDataPtr cdp(data);
auto component = manager->CreateComponent(cdp);
sceneNode->AddComponent(component);
```

### New System (`inc/hgl/ecs/`)

**PrimitiveComponent hierarchy:**
```
Component → RenderableComponent → PrimitiveComponent
```

**Structure:**
- Single `PrimitiveComponent` class
- Data stored directly in component
- No separate Manager class needed
- Uses standard `std::shared_ptr` and `std::weak_ptr`
- Clean, simple inheritance

**Usage:**
```cpp
// New pattern
auto entity = world->CreateEntity<Entity>("Object");
auto primitive = entity->AddComponent<PrimitiveComponent>();
primitive->SetPrimitive(myPrimitive);
```

### Benefits of New Design

1. **Simpler**: 60% less boilerplate code
2. **Cleaner**: Standard C++ patterns, no custom macros
3. **More Flexible**: Easy composition with other components
4. **Better Performance**: SOA storage for cache-friendly data access
5. **Easier Testing**: Self-contained components, no manager dependencies
6. **Modern C++**: Uses C++17 features and standard library

Both systems can coexist in the codebase during migration.

## Future Enhancements

Potential improvements for the future:
- Component pooling for better memory locality
- Further SOA optimization for other component types
- Entity archetype systems for faster queries
- Multi-threading support for parallel system updates
- Serialization support for save/load functionality

## References

- Inspired by: [hyzboy/AIECS](https://github.com/hyzboy/AIECS)
- Based on ECS architectural patterns commonly used in game engines
