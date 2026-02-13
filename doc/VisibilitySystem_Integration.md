# VisibilitySystem Integration Guide

## Overview

The VisibilitySystem provides a simple, component-based visibility management solution for the HGL ECS framework. Entities with a `VisibilityComponent` will be filtered during rendering collection based on their visibility state.

## Architecture

### Component Layer
- **VisibilityComponent** (`inc/hgl/ecs/VisibilityComponent.h`)
  - Lightweight component attached to entities
  - Simple boolean `visible` flag
  - Methods: `SetVisible(bool)`, `IsVisible()`

### System Layer (Placeholder)
- **VisibilitySystem** (`inc/hgl/ecs/VisibilitySystem.h`)
  - Placeholder for future optimization features
  - Currently not actively used
  - Could be used later for frustum culling, LOD, or cached queries

### Rendering Integration
- **RenderPrimitiveCollectSystem** checks `VisibilityComponent` during collection
- If entity has `VisibilityComponent` and `IsVisible() == false`, skip rendering
- Works in conjunction with `PrimitiveComponent::IsVisible()`

## Design Philosophy

**Simple CPU-side filtering** - No GPU buffer overhead, direct component query during render collection. This approach:
- ✅ Minimal overhead (single component check)
- ✅ Easy to understand and debug
- ✅ Works seamlessly with existing rendering pipeline
- ✅ No module dependency issues

## Current Implementation

### Rendering Collection with Visibility Check
```cpp
// In RenderPrimitiveCollectSystem::Update()
for (const auto& primitiveComp : primitives)
{
    // ... existing checks ...
    
    Entity* entity = primitiveComp->GetOwner();
    
    // Check VisibilityComponent if present
    auto vis_comp = entity->GetComponent<VisibilityComponent>();
    if (vis_comp && !vis_comp->IsVisible())
    {
        ++skipped_invisible;
        continue;  // Skip this entity
    }
    
    // ... collect for rendering ...
}
```

## Usage Examples

### Basic Visibility Control
```cpp
// In Gizmo3DMove.cpp
void SetGizmoMoveVisible(GizmoMoveECS *gizmo, bool visible)
{
    if(!gizmo || !gizmo->world)
        return;

    for(const auto &id : gizmo->entity_ids)
    {
        if(id.IsValid())
        {
            auto entity = gizmo->world->GetEntity(id);
            if(entity)
            {
                // Add or get VisibilityComponent
                auto vis_comp = entity->GetComponent<VisibilityComponent>();
                if(!vis_comp)
                {
                    vis_comp = entity->AddComponent<VisibilityComponent>();
                }
                if(vis_comp)
                {
                    vis_comp->SetVisible(visible);
                }
            }
        }
    }
}
```

### Gizmo Mode Switching
```cpp
// In GizmoTest.cpp - switch between Move/Rotate/Scale modes
SetGizmoMoveVisible(gizmo_move, current_mode == GizmoMode::Move);
SetGizmoRotateVisible(gizmo_rotate, current_mode == GizmoMode::Rotate);
SetGizmoScaleVisible(gizmo_scale, current_mode == GizmoMode::Scale);

// Only the active Gizmo will be rendered
```

## Implementation Details

### Rendering Filtering Flow
1. **RenderPrimitiveCollectSystem::Update()** iterates all PrimitiveComponents
2. For each component, check if entity has **VisibilityComponent**
3. If component exists and **IsVisible() == false**, skip collection
4. Only visible entities are added to render queue

### Performance Characteristics
- **O(1)** visibility check per entity (component lookup + bool check)
- **No per-frame scanning** of all entities
- **No GPU synchronization** overhead
- **No additional memory** beyond component storage

## Files Modified/Created

### New Files
- `inc/hgl/ecs/VisibilityComponent.h` - Component definition
- `inc/hgl/ecs/VisibilityDataStorage.h` - Placeholder for future use
- `src/ecs/VisibilityDataStorage.cpp` - Placeholder implementation
- `inc/hgl/ecs/VisibilitySystem.h` - Placeholder system
- `src/ecs/VisibilitySystem.cpp` - Placeholder implementation

### Modified Files
- `src/ecs/RenderPrimitiveCollectSystem.cpp` - Added VisibilityComponent check
- `src/ecs/CMakeLists.txt` - Added new files to build
- `src/ecs/Context.cpp` - Registered VisibilitySystem (placeholder)
- `example/Gizmo/Gizmo3DMove.cpp` - Added SetGizmoMoveVisible()
- `example/Gizmo/Gizmo3DRotate.cpp` - Added SetGizmoRotateVisible()
- `example/Gizmo/Gizmo3DScale.cpp` - Added SetGizmoScaleVisible()
- `example/Gizmo/Gizmo.h` - Added visibility function declarations

## Testing

### Gizmo Test Application
The system is tested in `example/Gizmo/GizmoTest.cpp`:
- Press `1` key → Show only Move Gizmo
- Press `2` key → Show only Rotate Gizmo
- Press `3` key → Show only Scale Gizmo

### Verification
- ✅ Only active Gizmo is rendered
- ✅ Inactive Gizmos are filtered during collection
- ✅ No visual artifacts or performance issues

## Future Opportunities

The placeholder `VisibilitySystem` and `VisibilityDataStorage` can be extended for:

### Frustum Culling
```cpp
// In VisibilitySystem::Update()
void VisibilitySystem::UpdateFrustumCulling(const Frustum& frustum)
{
    std::vector<Entity*> entities;
    world->GetAllEntities(entities);
    
    for (auto entity : entities)
    {
        auto vis_comp = entity->GetComponent<VisibilityComponent>();
        auto transform = entity->GetComponent<TransformComponent>();
        
        if (vis_comp && transform)
        {
            bool in_frustum = frustum.Contains(transform->GetWorldPosition());
            vis_comp->SetVisible(in_frustum);
        }
    }
}
```

### LOD System Integration
```cpp
// Automatically hide distant objects
void VisibilitySystem::UpdateLOD(const Vector3f& camera_pos, float max_distance)
{
    for (auto entity : entities_with_visibility)
    {
        auto transform = entity->GetComponent<TransformComponent>();
        float distance = glm::distance(camera_pos, transform->GetWorldPosition());
        
        entity->GetComponent<VisibilityComponent>()->SetVisible(distance < max_distance);
    }
}
```

### Cached Queries
```cpp
// Cache visible entities per frame
class VisibilityDataStorage
{
    std::vector<EntityID> cached_visible_entities;
    
    void UpdateCache(ECSContext* world)
    {
        cached_visible_entities.clear();
        // ... scan and cache visible entities ...
    }
    
    const std::vector<EntityID>& GetVisibleEntities() const
    {
        return cached_visible_entities;
    }
};
```

## Conclusion

The simplified VisibilitySystem provides an efficient, easy-to-understand solution for entity visibility control:

**Key Benefits:**
- ✅ Simple component-based design
- ✅ Direct integration with rendering pipeline  
- ✅ Minimal performance overhead
- ✅ No GPU synchronization complexity
- ✅ Easy to debug and maintain

**Trade-offs:**
- No GPU-side conditional rendering (acceptable for most use cases)
- Component query per entity during collection (negligible cost)

This design prioritizes **simplicity and maintainability** over GPU-side optimization, which is appropriate for the current use case (Gizmo visibility control).