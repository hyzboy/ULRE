#include<hgl/ecs/SubWorldComponent.h>
#include<hgl/ecs/Context.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/VisibilityComponent.h>
#include<hgl/ecs/TransformComponent.h>

// Forward declaration satisfied in header
namespace hgl::graph { class RenderCmdBuffer; }

namespace hgl::ecs
{
    SubWorldComponent::SubWorldComponent(const std::string& name)
        : Component(name)
    {
    }

    SubWorldComponent::~SubWorldComponent()
    {
        if (sub_world)
        {
            sub_world->Shutdown();
        }
    }

    bool SubWorldComponent::Initialize(ECSContext* parent_context)
    {
        if (!parent_context || !sub_world)
            return false;

        // Attach this context to parent (share TransformDataStorage)
        sub_world->AttachToParent(parent_context);

        // Initialize sub-world systems
        sub_world->Initialize();

        // Setup visibility inheritance linkage
        SetupVisibilityInheritance();

        return true;
    }

    void SubWorldComponent::UpdateSubWorld(float delta_time)
    {
        if (!sub_world || !sub_world->IsActive())
            return;

        sub_world->Tick(delta_time);
    }

    void SubWorldComponent::RenderSubWorld(graph::RenderCmdBuffer* cmd, float delta_time)
    {
        if (!sub_world || !sub_world->IsActive())
            return;

        sub_world->Render(cmd, delta_time);
    }

    void SubWorldComponent::ClearSubWorld()
    {
        if (sub_world)
        {
            sub_world->ClearEntities();
        }
    }

    void SubWorldComponent::OnAttach()
    {
        if (!owner_entity)
            owner_entity = GetOwner();

        // Create sub-world if not already created
        if (!sub_world)
        {
            std::string sub_world_name = owner_entity ? owner_entity->GetName() + "_SubWorld" : "SubWorld";
            sub_world = std::make_shared<ECSContext>(sub_world_name);

            // Get parent context through entity
            ECSContext* parent_context = nullptr;
            if (owner_entity)
            {
                parent_context = owner_entity->GetContext();
            }

            Initialize(parent_context);
        }
    }

    void SubWorldComponent::OnDetach()
    {
        if (sub_world)
        {
            sub_world->Shutdown();
            sub_world.reset();
        }
    }

    void SubWorldComponent::SetupVisibilityInheritance()
    {
        if (!sub_world || !owner_entity)
            return;

        // The visibility inheritance is automatically handled through:
        // 1. VisibilityDataStorage::IsInvisible() checks parent chain
        // 2. Parent-child relationships via TransformComponent
        // 3. Shared transform storage across contexts
        
        // Ensure owner entity has a VisibilityComponent if not already present
        auto owner_vis = owner_entity->GetComponent<VisibilityComponent>();
        if (!owner_vis)
        {
            owner_entity->AddComponent<VisibilityComponent>();
        }
    }
}//namespace hgl::ecs
