// Test for RenderCollector with frustum culling and distance sorting
// NOTE: This test requires CMMath to be properly linked for CameraInfo and Frustum
// The test demonstrates the ECS RenderCollector API but may not compile without CMMath

#include<hgl/ecs/World.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/RenderCollector.h>
#include<iostream>
#include<iomanip>

using namespace hgl::ecs;
using namespace hgl::graph;  // For CameraInfo

// Example renderable component
class MeshRenderable : public RenderableComponent
{
public:
    std::string meshName;

    MeshRenderable(const std::string& name = "Mesh")
        : RenderableComponent(name)
        , meshName("DefaultMesh")
    {
    }

    void Render(const glm::mat4& worldMatrix) override
    {
        // Actual rendering would happen here
    }
};

int main()
{
    std::cout << "=== RenderCollector Test ===" << std::endl;

    // Create a world
    auto world = std::make_shared<World>("TestWorld");
    world->Initialize();

    // Register RenderCollector system
    auto renderCollector = world->RegisterSystem<RenderCollector>();
    renderCollector->SetWorld(world);
    renderCollector->Initialize();

    std::cout << "Created world and RenderCollector" << std::endl;

    // Set up camera
    CameraInfo camera;
    camera.position = glm::vec3(0.0f, 0.0f, 10.0f);
    camera.forward = glm::vec3(0.0f, 0.0f, -1.0f);
    camera.fov = 60.0f;
    camera.nearPlane = 0.1f;
    camera.farPlane = 100.0f;

    // Create view and projection matrices
    camera.viewMatrix = glm::lookAt(
        camera.position,
        camera.position + camera.forward,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    camera.projectionMatrix = glm::perspective(
        glm::radians(camera.fov),
        16.0f / 9.0f,  // aspect ratio
        camera.nearPlane,
        camera.farPlane
    );

    camera.UpdateViewProjection();
    renderCollector->SetCameraInfo(camera);

    std::cout << "Camera configured at position: ("
              << camera.position.x << ", "
              << camera.position.y << ", "
              << camera.position.z << ")" << std::endl;

    // Create several entities with varying distances and positions
    std::vector<std::shared_ptr<Entity>> entities;

    // Entity 1: Close to camera
    {
        auto entity = world->CreateEntity<Entity>("NearObject");
        auto transform = entity->AddComponent<TransformComponent>();
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 5.0f));
        
        auto renderable = entity->AddComponent<RenderableComponent>();
        renderable->SetBoundingRadius(1.0f);
        
        entities.push_back(entity);
        std::cout << "Created NearObject at (0, 0, 5)" << std::endl;
    }

    // Entity 2: Medium distance
    {
        auto entity = world->CreateEntity<Entity>("MidObject");
        auto transform = entity->AddComponent<TransformComponent>();
        transform->SetLocalPosition(glm::vec3(2.0f, 0.0f, 0.0f));
        
        auto renderable = entity->AddComponent<RenderableComponent>();
        renderable->SetBoundingRadius(1.0f);
        
        entities.push_back(entity);
        std::cout << "Created MidObject at (2, 0, 0)" << std::endl;
    }

    // Entity 3: Far from camera
    {
        auto entity = world->CreateEntity<Entity>("FarObject");
        auto transform = entity->AddComponent<TransformComponent>();
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, -20.0f));
        
        auto renderable = entity->AddComponent<RenderableComponent>();
        renderable->SetBoundingRadius(1.0f);
        
        entities.push_back(entity);
        std::cout << "Created FarObject at (0, 0, -20)" << std::endl;
    }

    // Entity 4: Very far (should be culled if outside frustum)
    {
        auto entity = world->CreateEntity<Entity>("VeryFarObject");
        auto transform = entity->AddComponent<TransformComponent>();
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, -150.0f));
        
        auto renderable = entity->AddComponent<RenderableComponent>();
        renderable->SetBoundingRadius(1.0f);
        
        entities.push_back(entity);
        std::cout << "Created VeryFarObject at (0, 0, -150)" << std::endl;
    }

    // Entity 5: Invisible (should not be collected)
    {
        auto entity = world->CreateEntity<Entity>("InvisibleObject");
        auto transform = entity->AddComponent<TransformComponent>();
        transform->SetLocalPosition(glm::vec3(1.0f, 1.0f, 1.0f));
        
        auto renderable = entity->AddComponent<RenderableComponent>();
        renderable->SetVisible(false);
        
        entities.push_back(entity);
        std::cout << "Created InvisibleObject (invisible)" << std::endl;
    }

    std::cout << "\nTotal entities created: " << world->GetEntityCount() << std::endl;

    // Collect renderables with frustum culling and distance sorting
    std::cout << "\n=== Collection Test (Frustum Culling + Distance Sorting) ===" << std::endl;
    renderCollector->SetFrustumCullingEnabled(true);
    renderCollector->SetDistanceSortingEnabled(true);
    renderCollector->CollectRenderables();

    const auto& renderItems = renderCollector->GetRenderItems();
    std::cout << "Collected " << renderItems.size() << " visible items (after frustum culling)" << std::endl;

    std::cout << std::fixed << std::setprecision(2);
    for (size_t i = 0; i < renderItems.size(); ++i)
    {
        const auto& itemPtr = renderItems[i];
        if (itemPtr && itemPtr->isVisible)
        {
            auto entity = itemPtr->GetEntity();
            auto transform = itemPtr->GetTransform();
            if (entity && transform)
            {
                glm::vec3 pos = transform->GetWorldPosition();
                std::cout << "  [" << i << "] " << entity->GetName()
                          << " at (" << pos.x << ", " << pos.y << ", " << pos.z << ")"
                          << " | Distance: " << itemPtr->distanceToCamera << std::endl;
            }
        }
    }

    // Test without frustum culling
    std::cout << "\n=== Collection Test (No Frustum Culling) ===" << std::endl;
    renderCollector->SetFrustumCullingEnabled(false);
    renderCollector->CollectRenderables();

    const auto& allItems = renderCollector->GetRenderItems();
    std::cout << "Collected " << allItems.size() << " visible items (no culling)" << std::endl;

    for (size_t i = 0; i < allItems.size(); ++i)
    {
        const auto& itemPtr = allItems[i];
        if (itemPtr)
        {
            auto entity = itemPtr->GetEntity();
            auto transform = itemPtr->GetTransform();
            if (entity && transform)
            {
                glm::vec3 pos = transform->GetWorldPosition();
                std::cout << "  [" << i << "] " << entity->GetName()
                          << " at (" << pos.x << ", " << pos.y << ", " << pos.z << ")"
                          << " | Distance: " << itemPtr->distanceToCamera
                          << " | Visible: " << (itemPtr->isVisible ? "true" : "false") << std::endl;
            }
        }
    }

    // Shutdown
    world->Shutdown();
    std::cout << "\n=== Test Complete ===" << std::endl;

    return 0;
}
