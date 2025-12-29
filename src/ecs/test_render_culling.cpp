/**
 * Test for RenderCollector frustum culling with BoundingBoxComponent
 * 
 * This test demonstrates:
 * - Setting up entities with TransformComponent, RenderableComponent, and BoundingBoxComponent
 * - Frustum culling using AABB (more accurate than sphere)
 * - Fallback to sphere culling when no BoundingBoxComponent exists
 */

#include<hgl/ecs/World.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/RenderCollector.h>
#include<hgl/ecs/BoundingBoxComponent.h>
#include<iostream>
#include<memory>

using namespace hgl::ecs;

// Simple test renderable component
class TestRenderableComponent : public RenderableComponent
{
public:
    TestRenderableComponent() : RenderableComponent("TestRenderable") {}
    
    void Render(const glm::mat4& worldMatrix) override
    {
        // Simple render implementation
        std::cout << "  Rendering entity" << std::endl;
    }
};

int main()
{
    std::cout << "=== RenderCollector Frustum Culling with BoundingBox Test ===" << std::endl;

    // Create world
    auto world = std::make_shared<World>("TestWorld");
    world->Initialize();
    
    // Register RenderCollector system
    auto renderCollector = world->RegisterSystem<RenderCollector>();
    renderCollector->SetWorld(world);
    renderCollector->Initialize();
    
    // Set up camera (looking down -Z axis)
    // Note: This uses a minimal CameraInfo struct for testing
    // In real code, this would come from CMMath
    hgl::graph::CameraInfo camera;
    camera.eye = glm::vec3(0.0f, 0.0f, 10.0f);
    camera.target = glm::vec3(0.0f, 0.0f, 0.0f);
    camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
    
    // Create view and projection matrices
    camera.vp_matrix = glm::lookAt(camera.eye, camera.target, camera.up);
    camera.vp_matrix = glm::perspective(glm::radians(60.0f), 16.0f/9.0f, 0.1f, 100.0f) * camera.vp_matrix;
    
    renderCollector->SetCameraInfo(&camera);
    
    std::cout << "\n=== Creating Test Entities ===" << std::endl;
    
    // Entity 1: Inside frustum with AABB (should be visible)
    auto entity1 = world->CreateEntity<Entity>("InsideWithAABB");
    auto transform1 = entity1->AddComponent<TransformComponent>();
    transform1->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    auto renderable1 = entity1->AddComponent<TestRenderableComponent>();
    auto bbox1 = entity1->AddComponent<BoundingBoxComponent>();
    bbox1->SetAABB(glm::vec3(-1.0f), glm::vec3(1.0f));
    std::cout << "Created 'InsideWithAABB' at (0, 0, 0) with AABB [-1,-1,-1] to [1,1,1]" << std::endl;
    
    // Entity 2: Outside frustum (far away) with AABB (should be culled)
    auto entity2 = world->CreateEntity<Entity>("OutsideWithAABB");
    auto transform2 = entity2->AddComponent<TransformComponent>();
    transform2->SetLocalPosition(glm::vec3(0.0f, 0.0f, -200.0f)); // Beyond far plane
    auto renderable2 = entity2->AddComponent<TestRenderableComponent>();
    auto bbox2 = entity2->AddComponent<BoundingBoxComponent>();
    bbox2->SetAABB(glm::vec3(-1.0f), glm::vec3(1.0f));
    std::cout << "Created 'OutsideWithAABB' at (0, 0, -200) with AABB (should be culled)" << std::endl;
    
    // Entity 3: Inside frustum without AABB (uses sphere culling, should be visible)
    auto entity3 = world->CreateEntity<Entity>("InsideWithSphere");
    auto transform3 = entity3->AddComponent<TransformComponent>();
    transform3->SetLocalPosition(glm::vec3(2.0f, 0.0f, -5.0f));
    auto renderable3 = entity3->AddComponent<TestRenderableComponent>();
    renderable3->SetBoundingRadius(1.0f);
    std::cout << "Created 'InsideWithSphere' at (2, 0, -5) with sphere radius 1.0" << std::endl;
    
    // Entity 4: Outside frustum (to the side) with AABB (should be culled)
    auto entity4 = world->CreateEntity<Entity>("OutsideLeft");
    auto transform4 = entity4->AddComponent<TransformComponent>();
    transform4->SetLocalPosition(glm::vec3(-100.0f, 0.0f, 0.0f)); // Way to the left
    auto renderable4 = entity4->AddComponent<TestRenderableComponent>();
    auto bbox4 = entity4->AddComponent<BoundingBoxComponent>();
    bbox4->SetAABB(glm::vec3(-1.0f), glm::vec3(1.0f));
    std::cout << "Created 'OutsideLeft' at (-100, 0, 0) with AABB (should be culled)" << std::endl;
    
    // Entity 5: Inside frustum, invisible flag set (should be filtered out)
    auto entity5 = world->CreateEntity<Entity>("InvisibleEntity");
    auto transform5 = entity5->AddComponent<TransformComponent>();
    transform5->SetLocalPosition(glm::vec3(0.0f, 1.0f, -3.0f));
    auto renderable5 = entity5->AddComponent<TestRenderableComponent>();
    renderable5->SetVisible(false);
    auto bbox5 = entity5->AddComponent<BoundingBoxComponent>();
    bbox5->SetAABB(glm::vec3(-0.5f), glm::vec3(0.5f));
    std::cout << "Created 'InvisibleEntity' at (0, 1, -3) but marked invisible" << std::endl;
    
    std::cout << "\n=== Collecting Renderables (with frustum culling) ===" << std::endl;
    renderCollector->SetFrustumCullingEnabled(true);
    renderCollector->SetDistanceSortingEnabled(true);
    renderCollector->CollectRenderables();
    
    const auto& renderItems = renderCollector->GetRenderItems();
    std::cout << "Total entities collected: " << renderItems.size() << std::endl;
    
    int visibleCount = 0;
    int culledCount = 0;
    
    for (const auto& item : renderItems)
    {
        std::cout << "\nEntity: " << item.entity->GetName() << std::endl;
        std::cout << "  Position: (" 
                  << item.transform->GetWorldPosition().x << ", "
                  << item.transform->GetWorldPosition().y << ", "
                  << item.transform->GetWorldPosition().z << ")" << std::endl;
        std::cout << "  Distance to camera: " << item.distanceToCamera << std::endl;
        std::cout << "  Visible: " << (item.isVisible ? "YES" : "NO (culled)") << std::endl;
        
        auto bbox = item.entity->GetComponent<BoundingBoxComponent>();
        if (bbox)
        {
            std::cout << "  Culling method: AABB" << std::endl;
        }
        else
        {
            std::cout << "  Culling method: Sphere (radius: " 
                      << item.renderable->GetBoundingRadius() << ")" << std::endl;
        }
        
        if (item.isVisible)
        {
            visibleCount++;
        }
        else
        {
            culledCount++;
        }
    }
    
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Visible entities: " << visibleCount << std::endl;
    std::cout << "Culled entities: " << culledCount << std::endl;
    
    // Test disabling frustum culling
    std::cout << "\n=== Testing with frustum culling disabled ===" << std::endl;
    renderCollector->SetFrustumCullingEnabled(false);
    renderCollector->CollectRenderables();
    
    const auto& allItems = renderCollector->GetRenderItems();
    visibleCount = 0;
    for (const auto& item : allItems)
    {
        if (item.isVisible)
            visibleCount++;
    }
    std::cout << "All entities visible (culling disabled): " << visibleCount << std::endl;
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    
    return 0;
}
