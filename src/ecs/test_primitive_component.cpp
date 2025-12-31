/**
 * Test file for PrimitiveComponent in ECS architecture
 * 
 * This file demonstrates how to use the new PrimitiveComponent
 * designed for the hgl::ecs architecture, derived from RenderableComponent.
 * 
 * Usage example:
 * 
 * ```cpp
 * #include<hgl/ecs/World.h>
 * #include<hgl/ecs/Entity.h>
 * #include<hgl/ecs/TransformComponent.h>
 * #include<hgl/ecs/PrimitiveComponent.h>
 * 
 * using namespace hgl::ecs;
 * 
 * // Create a world
 * auto world = std::make_shared<World>("GameWorld");
 * 
 * // Create an entity
 * auto entity = world->CreateEntity<Entity>("MeshObject");
 * 
 * // Add TransformComponent
 * auto transform = entity->AddComponent<TransformComponent>();
 * transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
 * 
 * // Add PrimitiveComponent
 * auto primitive = entity->AddComponent<PrimitiveComponent>();
 * 
 * // Assuming you have a hgl::graph::Primitive* from somewhere
 * // (loaded from file, created procedurally, etc.)
 * hgl::graph::Primitive* myPrimitive = LoadPrimitiveFromFile("mesh.obj");
 * 
 * // Set the primitive
 * primitive->SetPrimitive(myPrimitive);
 * 
 * // Optional: Override the material
 * hgl::graph::MaterialInstance* customMaterial = CreateCustomMaterial();
 * primitive->SetOverrideMaterial(customMaterial);
 * 
 * // Check if it can render
 * if (primitive->CanRender())
 * {
 *     // The component is ready to render
 *     auto material = primitive->GetMaterial();
 *     auto pipeline = primitive->GetPipeline();
 *     
 *     // Use with RenderCollector
 *     auto renderCollector = world->GetSystem<RenderCollector>();
 *     renderCollector->CollectRenderables();
 * }
 * 
 * // Get bounding box for spatial queries
 * hgl::math::AABB aabb;
 * if (primitive->GetLocalAABB(aabb))
 * {
 *     // Use the AABB for collision detection, frustum culling, etc.
 * }
 * 
 * // Lifecycle management
 * // When the entity is destroyed or component is removed,
 * // OnDetach() is called automatically, which clears references
 * // but doesn't delete the Primitive (it's managed externally)
 * ```
 * 
 * Key Features:
 * 
 * 1. **Derived from RenderableComponent**: Integrates with the ECS rendering pipeline
 * 
 * 2. **Primitive Management**: 
 *    - SetPrimitive() / GetPrimitive() - manage the geometry+material resource
 *    - Automatically updates bounding radius for frustum culling
 * 
 * 3. **Material Override**:
 *    - SetOverrideMaterial() - temporarily replace material without changing Primitive
 *    - GetMaterialInstance() - returns override or primitive's material
 *    - ClearOverrideMaterial() - remove override
 * 
 * 4. **Rendering Support**:
 *    - CanRender() - checks if component has valid data and is visible
 *    - Render() - called by rendering systems with world matrix
 *    - GetPipeline() - access to rendering pipeline
 * 
 * 5. **Spatial Queries**:
 *    - GetLocalAABB() - get axis-aligned bounding box in local space
 *    - Bounding radius automatically calculated for frustum culling
 * 
 * 6. **Component Lifecycle**:
 *    - OnAttach() - called when added to entity
 *    - OnUpdate() - called each frame
 *    - OnDetach() - called when removed (cleans up references)
 * 
 * Comparison with Old PrimitiveComponent:
 * 
 * Old (Component/Data/Manager pattern):
 * - PrimitiveComponentData - holds the Primitive pointer
 * - PrimitiveComponentManager - manages component lifecycle
 * - PrimitiveComponent - the component itself
 * - Uses SharedPtr<ComponentData>
 * - Complex inheritance: RenderComponent → GeometryComponent → SceneComponent → Component
 * 
 * New (ECS pattern):
 * - Single PrimitiveComponent class (no separate Data/Manager)
 * - Simpler inheritance: PrimitiveComponent → RenderableComponent → Component
 * - Uses standard C++ smart pointers
 * - Data stored directly in component or SOA storage
 * - Compatible with RenderCollector and World systems
 * 
 * Benefits of New Design:
 * - Simpler, more maintainable code
 * - Better performance through SOA (for other components like Transform)
 * - More flexible composition
 * - Easier to understand and extend
 * - Standard C++ patterns (shared_ptr, weak_ptr)
 */

#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/World.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/RenderCollector.h>

// This is a conceptual test - it won't compile without the full project
// but demonstrates the intended usage of the new PrimitiveComponent

int main()
{
    // Example usage would go here
    // See the documentation above
    
    return 0;
}
