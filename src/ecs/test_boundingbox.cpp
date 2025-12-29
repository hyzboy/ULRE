// Test for BoundingBoxComponent with SOA storage
// Demonstrates bounding box management and SSBO-ready data layout

#include<hgl/ecs/World.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/BoundingBoxComponent.h>
#include<iostream>
#include<iomanip>

using namespace hgl::ecs;

int main()
{
    std::cout << "=== BoundingBox SOA Storage Test ===" << std::endl;

    // Create a world
    auto world = std::make_shared<World>("TestWorld");
    world->Initialize();
    std::cout << "Created world" << std::endl;

    // Create entities with bounding boxes
    std::cout << "\n=== Creating Entities with Bounding Boxes ===" << std::endl;

    // Entity 1: Small cube
    {
        auto entity = world->CreateEntity<Entity>("SmallCube");
        auto transform = entity->AddComponent<TransformComponent>();
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        
        auto bbox = entity->AddComponent<BoundingBoxComponent>();
        bbox->SetAABB(glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
        
        std::cout << "Created SmallCube with AABB: "
                  << "min(" << bbox->GetMinPoint().x << ", " << bbox->GetMinPoint().y << ", " << bbox->GetMinPoint().z << "), "
                  << "max(" << bbox->GetMaxPoint().x << ", " << bbox->GetMaxPoint().y << ", " << bbox->GetMaxPoint().z << ")"
                  << std::endl;
        std::cout << "  Center: (" << bbox->GetCenter().x << ", " << bbox->GetCenter().y << ", " << bbox->GetCenter().z << ")" << std::endl;
        std::cout << "  Extents: (" << bbox->GetExtents().x << ", " << bbox->GetExtents().y << ", " << bbox->GetExtents().z << ")" << std::endl;
    }

    // Entity 2: Large sphere approximation
    {
        auto entity = world->CreateEntity<Entity>("LargeSphere");
        auto transform = entity->AddComponent<TransformComponent>();
        transform->SetLocalPosition(glm::vec3(10.0f, 0.0f, 0.0f));
        
        auto bbox = entity->AddComponent<BoundingBoxComponent>();
        bbox->SetAABB(glm::vec3(-5.0f, -5.0f, -5.0f), glm::vec3(5.0f, 5.0f, 5.0f));
        
        std::cout << "Created LargeSphere with AABB: "
                  << "min(" << bbox->GetMinPoint().x << ", " << bbox->GetMinPoint().y << ", " << bbox->GetMinPoint().z << "), "
                  << "max(" << bbox->GetMaxPoint().x << ", " << bbox->GetMaxPoint().y << ", " << bbox->GetMaxPoint().z << ")"
                  << std::endl;
        std::cout << "  Center: (" << bbox->GetCenter().x << ", " << bbox->GetCenter().y << ", " << bbox->GetCenter().z << ")" << std::endl;
        std::cout << "  Extents: (" << bbox->GetExtents().x << ", " << bbox->GetExtents().y << ", " << bbox->GetExtents().z << ")" << std::endl;
    }

    // Entity 3: Flat ground plane
    {
        auto entity = world->CreateEntity<Entity>("GroundPlane");
        auto transform = entity->AddComponent<TransformComponent>();
        transform->SetLocalPosition(glm::vec3(0.0f, -10.0f, 0.0f));
        
        auto bbox = entity->AddComponent<BoundingBoxComponent>();
        bbox->SetAABB(glm::vec3(-100.0f, -0.1f, -100.0f), glm::vec3(100.0f, 0.1f, 100.0f));
        
        std::cout << "Created GroundPlane with AABB: "
                  << "min(" << bbox->GetMinPoint().x << ", " << bbox->GetMinPoint().y << ", " << bbox->GetMinPoint().z << "), "
                  << "max(" << bbox->GetMaxPoint().x << ", " << bbox->GetMaxPoint().y << ", " << bbox->GetMaxPoint().z << ")"
                  << std::endl;
        std::cout << "  Center: (" << bbox->GetCenter().x << ", " << bbox->GetCenter().y << ", " << bbox->GetCenter().z << ")" << std::endl;
        std::cout << "  Extents: (" << bbox->GetExtents().x << ", " << bbox->GetExtents().y << ", " << bbox->GetExtents().z << ")" << std::endl;
    }

    // Test SOA storage access
    std::cout << "\n=== SOA Storage Access Test ===" << std::endl;
    auto storage = BoundingBoxComponent::GetSharedStorage();
    std::cout << "SOA storage size: " << storage->GetSize() << " bounding boxes" << std::endl;

    // Access arrays directly (for SSBO upload simulation)
    const auto& minPoints = storage->GetAllMinPoints();
    const auto& maxPoints = storage->GetAllMaxPoints();
    const auto& centers = storage->GetAllCenters();
    const auto& extents = storage->GetAllExtents();

    std::cout << "\nDirect array access (SSBO-ready format):" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    for (size_t i = 0; i < minPoints.size(); ++i)
    {
        std::cout << "  BBox[" << i << "]: "
                  << "min(" << minPoints[i].x << ", " << minPoints[i].y << ", " << minPoints[i].z << "), "
                  << "max(" << maxPoints[i].x << ", " << maxPoints[i].y << ", " << maxPoints[i].z << "), "
                  << "center(" << centers[i].x << ", " << centers[i].y << ", " << centers[i].z << "), "
                  << "extents(" << extents[i].x << ", " << extents[i].y << ", " << extents[i].z << ")"
                  << std::endl;
    }

    // Test using CMMath's AABB type
    std::cout << "\n=== CMMath AABB Integration Test ===" << std::endl;
    auto testEntity = world->CreateEntity<Entity>("TestAABB");
    auto testBBox = testEntity->AddComponent<BoundingBoxComponent>();
    
    // Create AABB using CMMath (or minimal fallback)
    hgl::AABB cmathAABB(glm::vec3(-2.0f, -3.0f, -4.0f), glm::vec3(2.0f, 3.0f, 4.0f));
    testBBox->SetAABB(cmathAABB);
    
    // Retrieve as AABB
    hgl::AABB retrievedAABB = testBBox->GetAABB();
    std::cout << "Set AABB using CMMath, retrieved: "
              << "min(" << retrievedAABB.min_point.x << ", " << retrievedAABB.min_point.y << ", " << retrievedAABB.min_point.z << "), "
              << "max(" << retrievedAABB.max_point.x << ", " << retrievedAABB.max_point.y << ", " << retrievedAABB.max_point.z << ")"
              << std::endl;

    // Test dirty flags
    std::cout << "\n=== Dirty Flag Test ===" << std::endl;
    std::cout << "Is dirty after set: " << (testBBox->IsDirty() ? "true" : "false") << std::endl;
    testBBox->ClearDirty();
    std::cout << "Is dirty after clear: " << (testBBox->IsDirty() ? "true" : "false") << std::endl;
    testBBox->SetAABB(glm::vec3(-1.0f), glm::vec3(1.0f));
    std::cout << "Is dirty after another set: " << (testBBox->IsDirty() ? "true" : "false") << std::endl;

    // Simulate SSBO upload scenario
    std::cout << "\n=== SSBO Upload Simulation ===" << std::endl;
    std::cout << "Total bounding boxes: " << storage->GetSize() << std::endl;
    std::cout << "Memory layout (contiguous arrays, ready for glBufferData):" << std::endl;
    std::cout << "  minPoints array: " << minPoints.size() * sizeof(glm::vec3) << " bytes" << std::endl;
    std::cout << "  maxPoints array: " << maxPoints.size() * sizeof(glm::vec3) << " bytes" << std::endl;
    std::cout << "  centers array: " << centers.size() * sizeof(glm::vec3) << " bytes" << std::endl;
    std::cout << "  extents array: " << extents.size() * sizeof(glm::vec3) << " bytes" << std::endl;
    std::cout << "Total: " << (minPoints.size() + maxPoints.size() + centers.size() + extents.size()) * sizeof(glm::vec3) << " bytes" << std::endl;

    // Shutdown
    world->Shutdown();
    std::cout << "\n=== Test Complete ===" << std::endl;

    return 0;
}
