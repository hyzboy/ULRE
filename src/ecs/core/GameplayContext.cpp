#include <hgl/ecs/core/GameplayContext.h>
#include <hgl/ecs/systems/tick/JoltPhysicsSystem.h>
#include <hgl/ecs/systems/tick/SpatialTransformSystem.h>

namespace hgl::ecs
{
    ECSGameplayContext::ECSGameplayContext(const AnsiString &name)
        : ECSCoreContext(name)
    {
        spatial_state.Initialize(this, nullptr);
    }

    ECSGameplayContext::~ECSGameplayContext()
    {
        Shutdown();
    }

    void ECSGameplayContext::ConfigureSystem(System *system)
    {
        ECSCoreContext::ConfigureSystem(system);

        if (auto *physics = dynamic_cast<JoltPhysicsSystem *>(system))
        {
            if (!physics->GetSpatialState())
                physics->SetSpatialState(&spatial_state);
        }
        else if (auto *spatial_tf = dynamic_cast<SpatialTransformSystem *>(system))
        {
            if (!spatial_tf->GetSpatialState())
                spatial_tf->SetSpatialState(&spatial_state);
        }
    }

    void ECSGameplayContext::OnEntityDestroying(const EntityID id)
    {
        spatial_state.OnEntityDestroy(id);
    }

    void ECSGameplayContext::Shutdown()
    {
        if (!CanMutateEntities() || shutdown_in_progress)
            return;

        shutdown_in_progress = true;
        const bool was_active = active;
        active = false;

        if (was_active)
            system_registry.ShutdownSystems();
        else
            system_registry.ClearRegistrations();

        ClearEntities();
        ClearComponentPools();
        system_registry.ClearElementIndex();
        system_registry.ClearDependencies();
        system_registry.ResetComponentDrivenGroups();
        spatial_state.Clear();

        shutdown_in_progress = false;
    }
}
