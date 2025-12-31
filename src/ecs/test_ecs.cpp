// Simple test for ECS architecture
// This file demonstrates basic usage of the ECS system including SOA storage

#include<hgl/ecs/World.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/System.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/TransformDataStorage.h>
#include<iostream>

using namespace hgl::ecs;

// Example custom component
class VelocityComponent : public Component
{
public:
    glm::vec3 velocity;

    VelocityComponent(const glm::vec3& vel = glm::vec3(0.0f))
        : Component("Velocity")
        , velocity(vel)
    {
    }

    void OnUpdate(float deltaTime) override
    {
        // Movement logic would go here
    }
};

// Example custom system
class MovementSystem : public System
{
public:
    MovementSystem() : System("MovementSystem") {}

    void Initialize() override
    {
        std::cout << "MovementSystem initialized" << std::endl;
        SetInitialized(true);
    }

    void Update(float deltaTime) override
    {
        // System update logic would go here
    }

    void Shutdown() override
    {
        std::cout << "MovementSystem shut down" << std::endl;
    }
};

int main()
{
    std::cout << "=== ECS Test ===" << std::endl;

    // Create a world
    auto world = std::make_shared<World>("TestWorld");
    std::cout << "Created world: " << world->GetName() << std::endl;

    // Register a system
    auto movementSystem = world->RegisterSystem<MovementSystem>();
    std::cout << "Registered MovementSystem" << std::endl;

    // Initialize the world
    world->Initialize();
    std::cout << "World initialized, active: " << (world->IsActive() ? "true" : "false") << std::endl;

    // Create an entity
    auto entity1 = world->CreateEntity<Entity>("Player");
    std::cout << "Created entity: " << entity1->GetName() << " (ID: " << entity1->GetID() << ")" << std::endl;

    // Add components to entity
    auto transform = entity1->AddComponent<TransformComponent>();
    transform->SetLocalPosition(glm::vec3(10.0f, 20.0f, 30.0f));
    std::cout << "Added TransformComponent" << std::endl;

    auto velocity = entity1->AddComponent<VelocityComponent>(glm::vec3(1.0f, 0.0f, 0.0f));
    std::cout << "Added VelocityComponent" << std::endl;

    // Test getting components
    auto retrievedTransform = entity1->GetComponent<TransformComponent>();
    if (retrievedTransform)
    {
        glm::vec3 pos = retrievedTransform->GetLocalPosition();
        std::cout << "Entity position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
    }

    // Test component count
    std::cout << "Entity has " << entity1->GetComponentCount() << " components" << std::endl;

    // Test has component
    std::cout << "Has TransformComponent: " << (entity1->HasComponent<TransformComponent>() ? "true" : "false") << std::endl;
    std::cout << "Has VelocityComponent: " << (entity1->HasComponent<VelocityComponent>() ? "true" : "false") << std::endl;

    // Create another entity to test hierarchy
    auto entity2 = world->CreateEntity<Entity>("Child");
    auto transform2 = entity2->AddComponent<TransformComponent>();
    transform2->SetLocalPosition(glm::vec3(5.0f, 0.0f, 0.0f));

    // Set up parent-child relationship
    transform2->SetParent(entity1);
    std::cout << "Set entity2 as child of entity1" << std::endl;

    // Test world position calculation
    glm::vec3 worldPos = transform2->GetWorldPosition();
    std::cout << "Child world position: (" << worldPos.x << ", " << worldPos.y << ", " << worldPos.z << ")" << std::endl;

    // Update world (simulate one frame)
    world->Update(0.016f); // 16ms ~= 60fps
    std::cout << "World updated" << std::endl;

    // Test entity count
    std::cout << "World has " << world->GetEntityCount() << " entities" << std::endl;

    // Test mobility
    transform->SetMovable(false);
    std::cout << "Transform set to static: " << (transform->IsStatic() ? "true" : "false") << std::endl;

    // Test SOA storage access
    std::cout << "\n=== SOA Storage Test ===" << std::endl;
    auto storage = TransformComponent::GetSharedStorage();
    std::cout << "SOA storage size: " << storage->GetSize() << " transforms" << std::endl;
    
    // Access storage handle
    auto handle = transform->GetStorageHandle();
    std::cout << "Transform storage handle: " << handle << std::endl;
    
    // Direct storage access
    glm::vec3 storedPos = storage->GetPosition(handle);
    std::cout << "Position from storage: (" << storedPos.x << ", " << storedPos.y << ", " << storedPos.z << ")" << std::endl;

    // Test batch operations with SOA storage
    std::cout << "\n=== Batch Operations Test ===" << std::endl;
    int dirtyCount = 0;
    storage->UpdateAllDirtyMatrices([&dirtyCount](TransformDataStorage::HandleID id, glm::vec3 pos, glm::quat rot, glm::vec3 scale) {
        dirtyCount++;
        std::cout << "Updated transform " << id << " at position (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
    });
    std::cout << "Total dirty transforms updated: " << dirtyCount << std::endl;

    // Shutdown
    world->Shutdown();
    std::cout << "\nWorld shut down" << std::endl;

    std::cout << "\n=== Test Complete ===" << std::endl;

    return 0;
}
