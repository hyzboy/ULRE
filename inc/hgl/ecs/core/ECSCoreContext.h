#pragma once

#include <hgl/ecs/core/entity/Object.h>
#include <hgl/ecs/core/entity/ECSEntityObserver.h>
#include <hgl/ecs/core/entity/EntityManager.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/ecs/core/system/SystemRegistry.h>
#include <hgl/ecs/support/storage/ComponentPoolRegistry.h>
#include <hgl/ecs/support/storage/ComponentPool.h>
#include <hgl/type/Smart.h>
#include <hgl/type/ValueArray.h>
#include <hgl/log/Log.h>
#include <cstdint>
#include <utility>

namespace hgl::ecs
{
    class CameraComponent;

    /**
     * ECSCoreContext —— 纯净的 ECS 核心上下文。
     *
     * 职责：
     * - 实体生命周期管理（EntityManager / Entity）
     * - 组件池生命周期管理（ComponentPoolRegistry / ComponentPool）
     * - 系统注册与调度（SystemRegistry / System）
     * - 实体生命周期观察者分发（IECSEntityObserver）
     *
     * 边界约束：
     * - 零 Vulkan / 零图形 / 零渲染管线依赖
     * - 属于 ULRE.ECS.Core
     */
    class ECSCoreContext : public Object, public IECSEntityObserver
    {
        friend class Entity;
    protected:
        OBJECT_LOGGER

        AutoDelete<EntityManager> entity_manager;
        SystemRegistry system_registry;
        ComponentPoolRegistry component_pool_registry;

        bool active = false;
        bool shutdown_in_progress = false;

    public:
        explicit ECSCoreContext(const char *name = "CoreWorld");
        explicit ECSCoreContext(const AnsiString &name = "CoreWorld");
        ~ECSCoreContext() override;

        bool IsActive() const { return active; }
        void SetActive(bool a) { active = a; }

        virtual void Shutdown();

        // ── 实体管理 ──
        Entity *GetEntity(EntityID id);
        const Entity *GetEntity(EntityID id) const;
        virtual void DestroyEntity(EntityID id);
        virtual void DestroyEntity(Entity *entity);

        template<typename Fn>
        void ForEachAliveEntity(Fn &&fn) const
        {
            const EntityManager *em = entity_manager;
            if (em)
                em->ForEachAliveEntity(std::forward<Fn>(fn));
        }

        template<typename T = Entity, typename... Args>
        T *CreateEntity(Args &&...args)
        {
            static_assert(std::is_base_of_v<Entity, T>, "T must derive from hgl::ecs::Entity");

            if (!entity_manager || shutdown_in_progress)
                return nullptr;

            T *entity = entity_manager->CreateEntity<T>(std::forward<Args>(args)...);
            if (!entity)
                return nullptr;

            entity->SetContext(this);
            OnEntityCreated(entity);
            return entity;
        }

    private:
        // ── 组件存储接口（T2-c）──
        template<typename T>
        typename ComponentStorageTraits<T>::PoolType *GetTypedPool() const
        {
            return component_pool_registry.Get<T>();
        }

        template<typename T>
        typename ComponentStorageTraits<T>::PoolType *EnsureTypedPool()
        {
            return component_pool_registry.Ensure<T>();
        }

    public:
        template<typename T, typename... Args>
        T *CreateComponent(Entity *owner, Args &&...args)
        {
            if (!owner || owner->destroying || owner->GetCoreContext() != this
                || GetEntity(owner->GetEntityID()) != owner)
                return nullptr;

            constexpr ComponentType type = ComponentTypeOf<T>::value;
            auto *pool = EnsureTypedPool<T>();

            const uint32_t old_row = owner->SetComponentRow(type, 0);
            if (old_row)
            {
                pool->DestroyRow(old_row);
                OnComponentRowRemoved(type);
            }

            const uint32_t row = pool->Create(owner, std::forward<Args>(args)...);
            owner->SetComponentRow(type, row);
            OnComponentRowAdded(type);
            OnEntityRowRefreshed(owner);
            owner->MarkSceneDirty();

            return pool->Row(row);
        }

        template<typename T>
        T *GetComponentByRow(const uint32_t row) const
        {
            auto *pool = GetTypedPool<T>();
            return pool ? pool->Row(row) : nullptr;
        }

        template<typename T>
        void GetComponents(ValueArray<T *> &out) const
        {
            out.Clear();
            auto *pool = GetTypedPool<T>();
            if (!pool)
                return;

            const uint32_t count = pool->GetActiveCount();
            out.Reserve(int(count));
            for (uint32_t i = 0; i < count; ++i)
                out.Add(pool->GetActiveRow(i));
        }

    protected:
        virtual void ClearComponentPools();
        virtual void DestroyComponent(ComponentType type, uint32_t row);

    public:
        // ── 虚生命周期与变更钩子（派生类扩展渲染副作用）──
        virtual void OnEntityCreated(Entity *entity) {}
        virtual void OnEntityDestroying(EntityID id) {}
        virtual void OnComponentRowAdded(ComponentType type) {}
        virtual void OnComponentRowRemoved(ComponentType type) {}

        // IECSEntityObserver 实现
        void OnEntityDestroyed(EntityID id) override;
        void OnEntityDeactivated(EntityID id) override;
        void OnEntityRowRefreshed(Entity *entity) override;
        void OnComponentRowDestroyed(ComponentType type, uint32_t row) override;
        void OnSceneStructureDirty() override;

        // ── 系统管理 ──
        SystemRegistry &GetSystemRegistry() { return system_registry; }
        const SystemRegistry &GetSystemRegistry() const { return system_registry; }

        template<typename T>
        std::shared_ptr<T> RegisterSystem(std::shared_ptr<T> system)
        {
            if (!system)
                return nullptr;
            const size_t key = typeid(T).hash_code();
            system_registry.AddOrUpdate(key, system);
            return system;
        }

        template<typename T, typename... Args>
        std::shared_ptr<T> RegisterSystem(Args &&...args)
        {
            auto system = std::make_shared<T>(std::forward<Args>(args)...);
            const size_t key = typeid(T).hash_code();
            system_registry.AddOrUpdate(key, system);
            return system;
        }

        template<typename T>
        std::shared_ptr<T> GetSystem() const
        {
            const size_t key = typeid(T).hash_code();
            return std::static_pointer_cast<T>(system_registry.Find(key));
        }

        virtual void SetSystemGroupEnabled(SystemGroupID group, bool enabled);

        virtual void Tick(float deltaTime);

    protected:
        void ClearEntities();
        virtual void RunSystemUpdate(System *system, float deltaTime);
    };

    // ── Entity 组件挂载/卸载内联定义 ──

    template<typename T, typename... Args>
    T *Entity::AddComponent(Args &&...args)
    {
        static_assert(!std::is_same_v<T, CameraComponent>,
                      "相机不能经 Entity::AddComponent 挂载：走世界级入口 ECSContext::GetOrCreateCamera");

        return context ? context->CreateComponent<T>(this, std::forward<Args>(args)...) : nullptr;
    }

    template<typename T>
    T *Entity::GetComponent() const
    {
        static_assert(!std::is_same_v<T, CameraComponent>,
                      "相机不能经 Entity::GetComponent 读取：走世界级入口 ECSContext::GetCameraByEntity");

        return context ? context->GetComponentByRow<T>(GetComponentRow(ComponentTypeOf<T>::value)) : nullptr;
    }

    template<typename T>
    bool Entity::HasComponent() const
    {
        return GetComponentRow(ComponentTypeOf<T>::value) != 0;
    }

    template<typename T>
    void Entity::RemoveComponent()
    {
        constexpr ComponentType type = ComponentTypeOf<T>::value;
        static_assert(type != ComponentType::None,
                      "组件必须有类型号（T2-c：实体槽 = 按类型行号）");

        const uint32_t row = SetComponentRow(type, 0);
        if (!row)
            return;

        if (context)
        {
            context->DestroyComponent(type, row);
            if (observer)
                observer->OnEntityRowRefreshed(this);
        }

        MarkSceneDirty();
    }
}
