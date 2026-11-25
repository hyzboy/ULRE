#pragma once

#include <hgl/ecs/support/storage/ComponentPool.h>
#include <hgl/type/ManagedArray.h>

namespace hgl::ecs
{
    class ComponentPoolRegistry
    {
        ManagedArray<ComponentPoolBase> pools;
        ComponentPoolBase *slots[COMPONENT_TYPE_COUNT] = {};

    public:
        ComponentPoolBase *Get(ComponentType type) const
        {
            return IsValidComponentType(type) ? slots[static_cast<uint32_t>(type)] : nullptr;
        }

        template<typename T>
        typename ComponentStorageTraits<T>::PoolType *Get() const
        {
            return static_cast<typename ComponentStorageTraits<T>::PoolType *>(
                Get(ComponentTypeOf<T>::value));
        }

        template<typename T>
        typename ComponentStorageTraits<T>::PoolType *Ensure()
        {
            constexpr ComponentType type = ComponentTypeOf<T>::value;
            static_assert(type != ComponentType::None, "Component type must be registered");

            const uint32_t index = static_cast<uint32_t>(type);
            if (!slots[index])
                slots[index] = pools.template CreateDerived<typename ComponentStorageTraits<T>::PoolType>();
            return static_cast<typename ComponentStorageTraits<T>::PoolType *>(slots[index]);
        }

        void Clear()
        {
            for (int i = 0; i < pools.GetCount(); ++i)
                if (auto *pool = pools[i])
                    pool->Clear();
            pools.Clear();
            for (auto &slot : slots)
                slot = nullptr;
        }
    };
}
