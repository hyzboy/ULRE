/**
 * Example: Using the new ECS RenderCollector with PrimitiveComponent
 * 
 * This example demonstrates the redesigned RenderCollector that supports
 * material/pipeline batching for efficient rendering of PrimitiveComponents.
 */

#include<hgl/ecs/World.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/RenderCollector.h>
#include<hgl/graph/CameraInfo.h>
#include<iostream>

using namespace hgl::ecs;
using namespace hgl::graph;

int main()
{
    std::cout << "=== ECS RenderCollector with PrimitiveComponent Example ===" << std::endl;

    // 1. Create a world
    auto world = std::make_shared<World>("GameWorld");
    world->Initialize();

    // 2. Create and register RenderCollector system
    auto renderCollector = world->RegisterSystem<RenderCollector>();
    renderCollector->SetWorld(world);
    renderCollector->Initialize();

    // 3. Set up camera
    CameraInfo camera;
    camera.position = glm::vec3(0.0f, 5.0f, 10.0f);
    camera.forward = glm::vec3(0.0f, 0.0f, -1.0f);
    camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
    camera.fov = 60.0f;
    camera.aspect = 16.0f / 9.0f;
    camera.nearPlane = 0.1f;
    camera.farPlane = 100.0f;

    // Create view and projection matrices
    camera.viewMatrix = glm::lookAt(
        camera.position,
        camera.position + camera.forward,
        camera.up
    );

    camera.projectionMatrix = glm::perspective(
        glm::radians(camera.fov),
        camera.aspect,
        camera.nearPlane,
        camera.farPlane
    );

    camera.UpdateViewProjection();
    renderCollector->SetCameraInfo(&camera);

    std::cout << "Camera configured at position: ("
              << camera.position.x << ", "
              << camera.position.y << ", "
              << camera.position.z << ")" << std::endl;

    // 4. Enable rendering features
    renderCollector->SetFrustumCullingEnabled(true);
    renderCollector->SetDistanceSortingEnabled(true);
    renderCollector->SetBatchingEnabled(true);  // Enable material batching

    // 5. Create entities with PrimitiveComponents
    // In a real application, you would load meshes and materials from files
    // Here we simulate the structure
    
    std::cout << "\n=== Creating Entities with PrimitiveComponents ===" << std::endl;

    // Example: Creating multiple entities with different primitives
    for (int i = 0; i < 5; ++i)
    {
        auto entity = world->CreateEntity<Entity>("MeshObject_" + std::to_string(i));
        
        // Add TransformComponent
        auto transform = entity->AddComponent<TransformComponent>();
        transform->SetLocalPosition(glm::vec3(i * 2.0f - 4.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(glm::vec3(0.0f, glm::radians(45.0f * i), 0.0f)));
        transform->SetLocalScale(glm::vec3(1.0f));

        // Add PrimitiveComponent
        auto primitiveComp = entity->AddComponent<PrimitiveComponent>();
        
        // In a real app, you would:
        // Primitive* primitive = LoadPrimitive("mesh.obj", "material.json");
        // primitiveComp->SetPrimitive(primitive);
        
        // Optionally override material:
        // MaterialInstance* customMaterial = CreateMaterial(...);
        // primitiveComp->SetOverrideMaterial(customMaterial);

        std::cout << "Created entity: " << entity->GetName()
                  << " at position (" << transform->GetLocalPosition().x << ", "
                  << transform->GetLocalPosition().y << ", "
                  << transform->GetLocalPosition().z << ")" << std::endl;
    }

    std::cout << "Total entities: " << world->GetEntityCount() << std::endl;

    // 6. Collect renderables
    std::cout << "\n=== Collecting Renderables ===" << std::endl;
    renderCollector->CollectRenderables();

    std::cout << "Renderable count: " << renderCollector->GetRenderableCount() << std::endl;
    std::cout << "Empty: " << (renderCollector->IsEmpty() ? "Yes" : "No") << std::endl;

    // 7. Access render items (Method 1: Direct access)
    std::cout << "\n=== Method 1: Direct Render Items Access ===" << std::endl;
    const auto& renderItems = renderCollector->GetRenderItems();
    std::cout << "Total render items: " << renderItems.size() << std::endl;

    for (size_t i = 0; i < renderItems.size(); ++i)
    {
        const auto& itemPtr = renderItems[i];
        if (itemPtr && itemPtr->isVisible)
        {
            auto entity = itemPtr->GetEntity();
            std::cout << "  [" << i << "] " << entity->GetName()
                      << " | Distance: " << itemPtr->distanceToCamera
                      << " | Visible: " << (itemPtr->isVisible ? "Yes" : "No") << std::endl;
        }
    }

    // 8. Access material batches (Method 2: Batched rendering - more efficient)
    std::cout << "\n=== Method 2: Material Batches Access ===" << std::endl;
    const auto& materialBatches = renderCollector->GetMaterialBatches();
    std::cout << "Total material batches: " << materialBatches.size() << std::endl;

    int batchIndex = 0;
    for (const auto& pair : materialBatches)
    {
        const MaterialPipelineKey& key = pair.first;
        const MaterialBatch& batch = *pair.second;

        std::cout << "Batch [" << batchIndex++ << "]:" << std::endl;
        std::cout << "  Material: " << key.material << std::endl;
        std::cout << "  Pipeline: " << key.pipeline << std::endl;
        std::cout << "  Items count: " << batch.GetCount() << std::endl;

        // In a real rendering loop, you would:
        // 1. Bind the material and pipeline
        // 2. Iterate through batch items and render each one
        /*
        for (RenderItem* item : batch.GetItems())
        {
            if (item && item->isVisible)
            {
                glm::mat4 worldMatrix = item->GetWorldMatrix();
                Primitive* primitive = item->GetPrimitive();
                
                // Render the primitive with the world matrix
                // RenderPrimitive(primitive, worldMatrix, ...);
            }
        }
        */
    }

    // 9. Update transforms (when objects move)
    std::cout << "\n=== Updating Transforms ===" << std::endl;
    
    // Simulate moving an entity
    const auto& entities = world->GetEntities();
    if (!entities.empty())
    {
        auto entity = entities[0];
        auto transform = entity->GetComponent<TransformComponent>();
        if (transform)
        {
            glm::vec3 newPos = transform->GetLocalPosition() + glm::vec3(1.0f, 0.0f, 0.0f);
            transform->SetLocalPosition(newPos);
            std::cout << "Moved entity to: (" << newPos.x << ", " << newPos.y << ", " << newPos.z << ")" << std::endl;
        }
    }

    // Update all render items' transform data
    renderCollector->UpdateTransformData();
    std::cout << "Transform data updated" << std::endl;

    // 10. Update material instance (when material changes)
    // This would be called when you change a primitive's material at runtime
    /*
    PrimitiveComponent* comp = ...;
    comp->SetOverrideMaterial(newMaterial);
    renderCollector->UpdateMaterialInstanceData(comp);
    */

    // 11. Clear and re-collect (next frame)
    std::cout << "\n=== Next Frame ===" << std::endl;
    renderCollector->CollectRenderables();  // Automatically clears and re-collects
    std::cout << "Re-collected " << renderCollector->GetRenderableCount() << " items" << std::endl;

    // 12. Shutdown
    world->Shutdown();
    std::cout << "\n=== Example Complete ===" << std::endl;

    return 0;
}

/*
 * Expected Output:
 * 
 * === ECS RenderCollector with PrimitiveComponent Example ===
 * Camera configured at position: (0, 5, 10)
 * 
 * === Creating Entities with PrimitiveComponents ===
 * Created entity: MeshObject_0 at position (-4, 0, 0)
 * Created entity: MeshObject_1 at position (-2, 0, 0)
 * Created entity: MeshObject_2 at position (0, 0, 0)
 * Created entity: MeshObject_3 at position (2, 0, 0)
 * Created entity: MeshObject_4 at position (4, 0, 0)
 * Total entities: 5
 * 
 * === Collecting Renderables ===
 * Renderable count: 5
 * Empty: No
 * 
 * === Method 1: Direct Render Items Access ===
 * Total render items: 5
 *   [0] MeshObject_0 | Distance: ... | Visible: Yes
 *   [1] MeshObject_1 | Distance: ... | Visible: Yes
 *   ...
 * 
 * === Method 2: Material Batches Access ===
 * Total material batches: 2  (depending on materials)
 * Batch [0]:
 *   Material: 0x...
 *   Pipeline: 0x...
 *   Items count: 3
 * Batch [1]:
 *   Material: 0x...
 *   Pipeline: 0x...
 *   Items count: 2
 * 
 * === Updating Transforms ===
 * Moved entity to: (-3, 0, 0)
 * Transform data updated
 * 
 * === Next Frame ===
 * Re-collected 5 items
 * 
 * === Example Complete ===
 */
