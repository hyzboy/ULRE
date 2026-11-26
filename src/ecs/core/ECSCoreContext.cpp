#include <hgl/ecs/core/ECSCoreContext.h>
#include <hgl/ecs/support/storage/FrameProfiler.h>
#include <hgl/time/Time.h>
#include <typeinfo>

namespace hgl::ecs
{
    ECSCoreContext::ECSCoreContext(const char *name)
        : Object(name), system_registry(this)
    {
        entity_manager = new EntityManager();
    }

    ECSCoreContext::ECSCoreContext(const AnsiString &name)
        : Object(name), system_registry(this)
    {
        entity_manager = new EntityManager();
    }

    ECSCoreContext::~ECSCoreContext()
    {
        Shutdown();
    }

    void ECSCoreContext::Shutdown()
    {
        if (shutdown_in_progress)
            return;
        shutdown_in_progress = true;
        active = false;

        ClearEntities();
        ClearComponentPools();

        shutdown_in_progress = false;
    }

    void ECSCoreContext::ClearComponentPools()
    {
        component_pool_registry.Clear();
    }

    Entity *ECSCoreContext::GetEntity(EntityID id)
    {
        return entity_manager ? entity_manager->GetEntity(id) : nullptr;
    }

    const Entity *ECSCoreContext::GetEntity(EntityID id) const
    {
        const EntityManager *em = entity_manager;
        return em ? em->GetEntity(id) : nullptr;
    }

    void ECSCoreContext::DestroyEntity(EntityID id)
    {
        Entity *entity = GetEntity(id);
        if (!entity || entity->destroying)
            return;

        entity->destroying = true;
        OnEntityDestroying(id);
        entity->DetachAllComponents();
        entity_manager->DestroyEntity(id);
        OnEntityDestroyed(id);
    }

    void ECSCoreContext::ClearEntities()
    {
        ValueArray<EntityID> ids;
        ForEachAliveEntity([&ids](const Entity *entity)
        {
            ids.Add(entity->GetEntityID());
        });

        for (int i = 0; i < ids.GetCount(); ++i)
            DestroyEntity(ids[i]);
    }

    void ECSCoreContext::DestroyEntity(Entity *entity)
    {
        if (entity)
            DestroyEntity(entity->GetEntityID());
    }

    void ECSCoreContext::DestroyComponent(const ComponentType type, const uint32_t row)
    {
        if (row == 0 || !IsValidComponentType(type))
            return;

        ComponentPoolBase *pool = component_pool_registry.Get(type);
        if (!pool)
            return;

        pool->DestroyRow(row);
        OnComponentRowRemoved(type);
    }

    void ECSCoreContext::OnEntityDestroyed(EntityID id)
    {
        OnEntityDeactivated(id);
    }

    void ECSCoreContext::OnEntityDeactivated(EntityID)
    {
    }

    void ECSCoreContext::OnEntityRowRefreshed(Entity *)
    {
    }

    void ECSCoreContext::OnComponentRowDestroyed(ComponentType type, uint32_t row)
    {
        DestroyComponent(type, row);
    }

    void ECSCoreContext::OnSceneStructureDirty()
    {
    }

    void ECSCoreContext::SetSystemGroupEnabled(SystemGroupID group, bool enabled)
    {
        system_registry.SetGroupEnabled(group, enabled);
    }

    void ECSCoreContext::RunSystemUpdate(System *system, float deltaTime)
    {
        if (!system)
            return;
        if (!system->IsEnabled())
            return;

        if (FrameProfiler::IsEnabled())
        {
            const uint64_t t0 = GetUptimeUs();
            system->Update(deltaTime);
            AnsiString name = system->GetName();
            if (name.IsEmpty())
                name = AnsiString(typeid(*system).name());
            SystemProfiler::Add(name, system->GetRenderElementType(),
                                static_cast<uint32_t>(system->GetExecutionPhase()),
                                GetUptimeUs() - t0);
        }
        else
        {
            system->Update(deltaTime);
        }
    }

    void ECSCoreContext::Tick(float deltaTime)
    {
        if (!active)
            return;

        system_registry.SortTick();

        for (auto &entry : system_registry.TickOrder())
        {
            if (entry.system)
                RunSystemUpdate(entry.system.get(), deltaTime);
        }
    }
}
