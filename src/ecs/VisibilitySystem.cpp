#include<hgl/ecs/VisibilitySystem.h>
#include<hgl/ecs/VisibilityDataStorage.h>
#include<hgl/ecs/VisibilityComponent.h>
#include<hgl/ecs/Entity.h>

namespace hgl::ecs
{
    VisibilitySystem::VisibilitySystem(const std::string& name)
        : System(name)
        , world(nullptr)
        , visibility_storage(nullptr)
    {
        SetSystemType(SystemType::Unknown);
        SetExecutionOrder(ExecutionPhase::RenderBufferCommit);

        visibility_storage = new VisibilityDataStorage();
    }

    VisibilitySystem::~VisibilitySystem()
    {
        delete visibility_storage;
    }

    void VisibilitySystem::Initialize()
    {
        System::Initialize();
        
        if (!world)
            return;

        // Set storage reference for all existing VisibilityComponents
        std::vector<std::shared_ptr<VisibilityComponent>> components;
        world->GetComponents<VisibilityComponent>(components);
        
        for (auto& comp : components)
        {
            if (comp)
            {
                comp->SetStorage(visibility_storage);
                // Sync initial state
                if (!comp->IsVisible())
                {
                    visibility_storage->SetInvisible(comp->GetOwnerID());
                }
            }
        }
    }

    void VisibilitySystem::OnDependenciesReady()
    {
        System::OnDependenciesReady();
    }

    void VisibilitySystem::Update(float /*deltaTime*/)
    {
        // No per-frame work needed - VisibilityComponent updates storage directly
    }
}//namespace hgl::ecs
