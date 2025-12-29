# ECS (Entity-Component-System) Architecture

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
  - `Static` - Never moves, matrix cached permanently
  - `Movable` - Frequently updated, recalculated each frame
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

### 8. RenderCollector (`RenderCollector.h/cpp`)
System for collecting and processing renderable entities:
- **Entity Collection**: Gathers entities with TransformComponent and RenderableComponent
- **Frustum Culling**: Uses Frustum from CMMath (`hgl/math/geometry/Frustum.h`) for view culling
- **Distance Sorting**: Sorts render items by distance to camera (near to far)
- **CameraInfo Integration**: Uses CameraInfo from CMMath (`hgl/graph/CameraInfo.h`) for camera data
- **Visibility Filtering**: Respects component visibility flags
- **Features**:
  - RenderableComponent base class for renderable objects
  - RenderItem struct with collected rendering data
  - Enable/disable frustum culling and distance sorting
  - Efficient batch collection every frame

**Note**: Requires CMMath library to be linked for CameraInfo and Frustum types.

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

// Set up rendering with RenderCollector
auto renderCollector = world->RegisterSystem<RenderCollector>();
renderCollector->SetWorld(world);
renderCollector->Initialize();

// Configure camera (requires CMMath)
// CameraInfo and Frustum come from CMMath library
hgl::graph::CameraInfo camera;
// Set up camera parameters using CMMath's CameraInfo interface
// camera.position = ...
// camera.viewMatrix = ...
// camera.projectionMatrix = ...
renderCollector->SetCameraInfo(&camera);

// Add renderable component to entity
auto renderable = player->AddComponent<RenderableComponent>();
renderable->SetBoundingRadius(1.0f);

// Collect visible entities (with frustum culling and distance sorting)
renderCollector->CollectRenderables();
const auto& renderItems = renderCollector->GetRenderItems();
for (const auto& item : renderItems) {
    if (item.isVisible) {
        // Render the item
        item.renderable->Render(item.worldMatrix);
    }
}

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

Build and run the test:
```bash
cd src/ecs
g++ -std=c++17 -I../../inc -o test_ecs test_ecs.cpp *.cpp
./test_ecs
```

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
