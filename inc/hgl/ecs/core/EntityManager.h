#pragma once

#include<hgl/ecs/core/EntityHandle.h>
#include<hgl/ecs/core/Entity.h>          // T2-a：块池要 placement-new（需完整类型）
#include<hgl/log/Log.h>
#include<vector>
#include<memory>
#include<string>
#include<new>
#include<cstddef>
#include<type_traits>

namespace hgl::ecs
{
    /// Manages entity lifecycle using ID system
    /// Maintains a pool of entities with generation-based handle validation
    class EntityManager
    {
    private:
        OBJECT_LOGGER

        /// 实体按**块**分配（T2-a「取消逐 new」）：块内地址稳定 ⇒ 全仓持有的 `Entity*` 永不作废；
        /// 旧形态是每实体一个 `unique_ptr<Entity>`（每实体一次堆分配 + 一次控制块）。
        static constexpr uint32_t kEntityBlockSize = 64;

        static_assert(alignof(Entity) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
                      "Entity 的对齐超过默认 new 对齐：块池需改用 aligned new（T2-a）");

        struct EntitySlot
        {
            Entity  *entity = nullptr;   ///< 指向块内对象（**不拥有**；析构由本类显式调用）
            uint16_t generation = 0;
            // 2026-10-10（审计 §2.2）：`bool alive` 已删除 —— 它与 `entity != nullptr` **是同一事实**
            // （创建/销毁/清空三处都成对写），而每个读取点都在重复判 `slot.alive && slot.entity`。
            // 现在"有实体指针"就是"活着"的唯一来源。
        };

        std::vector<EntitySlot> slots;
        std::vector<uint32_t>   free_indices;
        std::vector<std::unique_ptr<std::byte[]>> blocks;   ///< 实体块池（地址稳定，随 Clear 释放）
        uint32_t next_index = 0;
        uint32_t max_entities = 0;

    public:
        EntityManager(uint32_t capacity = 1000);
        ~EntityManager();

        /// 在块池中**就地构造**实体。签名与原 `CreateEntity<T>` 一致 ⇒ 调用点零改动。
        /// 只支持 `T = Entity`：块池按 Entity 定尺寸（实测全仓 ~140 个调用点均为 Entity，无子类）。
        template<typename T = Entity, typename... Args>
        T *CreateEntity(Args&&... args)
        {
            static_assert(std::is_same_v<T, Entity>,
                          "EntityManager 的块池按 Entity 定尺寸（T2-a）：出现子类须先重新设计池");

            uint32_t index;
            uint16_t generation = 0;

            if (!free_indices.empty())
            {
                index = free_indices.back();
                free_indices.pop_back();
                generation = slots[index].generation;
            }
            else
            {
                index = next_index;

                if (index >= slots.size())
                {
                    if (index >= max_entities)
                    {
                        ExpandSlots(max_entities * 2);
                    }
                    slots.resize(index + 1);
                }

                next_index++;
            }

            Entity *entity = BlockEntityAt(index);
            new (entity) T(std::forward<Args>(args)...);

            const EntityID id(index, generation);
            entity->SetID(id);

            slots[index].entity = entity;
            slots[index].generation = generation;

        #if ULRE_ECS_DEBUG_API
            LogDebug("[EntityManager] CreateEntity name='%s' id=(%u,%u)",
                     entity->GetName().c_str(), id.index, id.generation);
        #endif

            return static_cast<T*>(entity);
        }

        /// Destroy entity by ID
        void DestroyEntity(EntityID id);

        /// Get entity pointer by ID, returns nullptr if invalid
        Entity* GetEntity(EntityID id);
        const Entity* GetEntity(EntityID id) const;

        /// Check if ID is valid and points to alive entity
        bool IsValidID(EntityID id) const;

        /// **结构性遍历**：只访问存活实体（跳空槽），不分配、不裸遍历 `slots`。
        /// 与 `ECSContext::ForEachMovableTransform/ForEachStaticTransform` 同族 ——
        /// 新增消费者一律用它（原先那条「收集全部实体指针」的接口每条实体要一个 vector 元素，
        /// 每帧调用 = 每帧一次堆分配，与「取消逐帧分配」的方向相反；2026-10-11 审计已整体删除）。
        template<typename Fn>
        void ForEachAliveEntity(Fn &&fn) const
        {
            for (const EntitySlot &slot : slots)
                if (slot.entity)
                    fn(slot.entity);
        }

        /// Clear all entities（块池一并释放）
        void Clear();

    private:
        /// 确保第 block_index 块存在，返回槽 index 在块内的对象地址（就地构造/析构用）
        Entity *BlockEntityAt(uint32_t index);

        void ExpandSlots(uint32_t new_capacity);
        void AddFreeIndex(uint32_t index);
    };
}
