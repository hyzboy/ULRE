#include <hgl/ecs/core/system/SystemRegistry.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/system/SystemOrderPlanner.h>
#include <hgl/log/Log.h>
#include <typeinfo>

namespace hgl::ecs
{
    void SystemRegistry::SortTick()
    {
        SortSystemList(tick_system_order, schedule.Dependencies(false),
                       schedule.Dirty(false), "Tick");
    }

    void SystemRegistry::SortRender()
    {
        SortSystemList(render_system_order, schedule.Dependencies(true),
                       schedule.Dirty(true), "Render");
    }

    void SystemRegistry::SortSystemList(PtrArray<OrderedSystem>& order_list,
                                        const SystemScheduleState::DependencyMap& dependencies,
                                        bool& dirty_flag,
                                        const char* label)
    {
        if (!dirty_flag)
            return;

        if (order_list.IsEmpty())
        {
            dirty_flag = false;
            return;
        }

        SystemOrderPlanner planner;
        for (const auto& entry : order_list)
            planner.AddSystem(entry.key, entry.phase, entry.group_id, entry.insertion_order);

        for (const auto& pair : dependencies)
        {
            for (const size_t dependency_key : pair.second)
            {
                const auto result = planner.AddDependency(pair.first, dependency_key);
                #ifdef _DEBUG
                if (result == SystemOrderPlanner::DependencyResult::MissingDependency)
                    GLogWarning("[SystemRegistry::SortSystemList] %s dependency missing for key %zu -> %zu",
                                label, pair.first, dependency_key);
                #else
                (void)result;
                #endif
            }
        }

        ValueArray<int> indices;
        const bool acyclic = planner.Build(indices);
        PtrArray<OrderedSystem> sorted;
        sorted.Reserve(order_list.GetCount());
        for (const int index : indices)
            sorted.Append(order_list[index]);
        order_list = std::move(sorted);
        if (!acyclic)
            GLogWarning("[SystemRegistry::SortSystemList] %s system dependencies contain a cycle. Falling back to phase/group/insertion order.", label);

        dirty_flag = false;
    }

    SystemRegistry::OrderedSystem* SystemRegistry::FindOrderedSystem(PtrArray<OrderedSystem>& list, size_t key)
    {
        for (auto& entry : list)
        {
            if (entry.key == key)
                return &entry;
        }

        return nullptr;
    }

    void SystemRegistry::AddOrUpdate(size_t key, const std::shared_ptr<System>& system)
    {
        if (!system)
            return;

        system->SetContext(context);

        // tick / render 分类的唯一权威 = 系统自己的 ExecutionPhase。
        // 旧版还收一个调用方声明的 `is_render`：相位不一致时只 LogWarning 一句就把系统
        // 静默换表（"纠正"），声明面因此是纯噪音 ⇒ 已整体删除（`Context.h` 的 `RegisterSystem`）。
        const int effective_phase = static_cast<int>(system->GetExecutionPhase());
        const bool is_render = effective_phase >= static_cast<int>(ExecutionPhase::RenderSwapchainNextImage);

        auto& sys_map = is_render ? render_systems : tick_systems;
        auto& order_list = is_render ? render_system_order : tick_system_order;
        auto& dirty_flag = schedule.Dirty(is_render);

        // ── 同键替换：旧实例必须先 Shutdown ──
        // RegisterSystem<T>() 重复注册（或外部替换实现）会覆盖 sys_map[key]；旧实例随后
        // 已不在世界容器里，Shutdown 再也不会被调用 ⇒ 它持有的 GPU 缓冲/纹理/监听器永不
        // 释放。旧实例还可能因执行相位变化落在另一张表（tick ↔ render），两张都要摘。
        auto retire_replaced_system = [key, &system](
            hgl::UnorderedMap<size_t, std::shared_ptr<System>>& map,
            PtrArray<OrderedSystem>& order,
            bool& dirty)
        {
            std::shared_ptr<System> old_system;
            if (auto* slot = map.GetValuePointer(key))
            {
                old_system = *slot;
                map.DeleteByKey(key);
            }

            // 调度列表里的同键条目一并摘除：否则旧实例仍会每帧被调度。
            for (int i = 0; i < order.GetCount();)
            {
                if (order[i].key == key)
                {
                    order.Delete(i);
                    dirty = true;
                    continue;
                }
                ++i;
            }

            if (!old_system || old_system == system)
                return;

            GLogDebug("[SystemRegistry] replacing system key=%zu: shutting down previous instance %s",
                      key, typeid(*old_system).name());

            // 旧实例在本调用返回后可能只剩调用方的 shared_ptr，这里先放掉它持有的
            // 世界资源（与 Shutdown 时对在册系统的处理一致）。
            old_system->Shutdown();
        };

        retire_replaced_system(tick_systems, tick_system_order, schedule.Dirty(false));
        retire_replaced_system(render_systems, render_system_order, schedule.Dirty(true));

        sys_map[key] = system;

        const AnsiString& element_type = system->GetRenderElementType();
        if (!element_type.IsEmpty())
        {
            auto& systems_list = systems_by_element_type[element_type];

            {
                const SystemGroupID changed_group = SystemGroupFromName(element_type.c_str());

                if (changed_group != SystemGroupID::None)
                    group_systems[static_cast<uint32_t>(changed_group)].Clear();
            }
            // 2026-10-11（§1 S1）：按**键**摘旧实例。原写法 `*it == system` 是"同一实例"比较 ⇒
            // `RegisterSystem<T>()` 再注册（替换同类型系统）时，列表里没有任何元素等于**新**实例，
            // 于是**旧实例永远留在列表里**；`SetSystemGroupEnabled` 从该列表重建快照
            // ⇒ 快照同时挂着新旧两个实例，组开关会去开关一个"已不在 sys_map 里"的僵尸实例
            // （`RenderGraph` 取该列表算组的相位区间时也会把僵尸算进去）。
            // 键 = `typeid(T).hash_code()`（见 `Context::RegisterSystem`）⇒ 同类型即同键，
            // 按类型比较等价于按键比较。
            for (int i = 0; i < systems_list.GetCount();)
            {
                const auto& candidate = systems_list[i];
                if (!candidate || typeid(*candidate) == typeid(*system))
                {
                    systems_list.Delete(i);
                    continue;
                }
                ++i;
            }

            systems_list.Append(system);
        }

        const int group_id = static_cast<int>(SystemGroupFromName(element_type.c_str()));

        if (auto* entry = FindOrderedSystem(order_list, key))
        {
            entry->system = system;
            entry->phase = effective_phase;
            entry->group_id = group_id;
            dirty_flag = true;
        }
        else
        {
            OrderedSystem new_entry;
            new_entry.key = key;
            new_entry.phase = effective_phase;
            new_entry.group_id = group_id;
            new_entry.insertion_order = schedule.NextInsertionOrder();
            new_entry.system = system;
            order_list.Append(new_entry);
            dirty_flag = true;
        }

        schedule.RemoveDependencies(is_render, key);

        const auto& deps_decl = system->GetDependencies();
        for (const auto& dep_type : deps_decl)
        {
            size_t dep_key = dep_type.hash_code();
            schedule.AddDependency(is_render, key, dep_key);
        }
        if (context->IsActive())
        {
            system->Initialize();
        }
    }

    std::shared_ptr<System> SystemRegistry::Find(size_t key) const
    {
        if (const auto* slot = tick_systems.GetValuePointer(key))
            return *slot;
        if (const auto* slot = render_systems.GetValuePointer(key))
            return *slot;
        return nullptr;
    }

    void SystemRegistry::SetGroupEnabled(SystemGroupID group, bool enabled)
    {
        if (group == SystemGroupID::None)
            return;

        const uint32_t index = static_cast<uint32_t>(group);

        if (!group_systems[index].IsEmpty())
        {
            for (auto& system : group_systems[index])
                if (system)
                    system->SetEnabled(enabled);

            return;
        }

        const auto* systems_list = FindByElementType(AnsiString(kSystemGroupNames[index]));
        if (!systems_list)
            return;

        auto& snapshot = group_systems[index];
        snapshot.Clear();
        snapshot.Reserve(systems_list->GetCount());

        for (const auto& system : *systems_list)
        {
            if (system)
            {
                snapshot.Append(system);
                system->SetEnabled(enabled);
            }
        }
    }

    void SystemRegistry::ShutdownSystems()
    {
        SortTick();
        SortRender();

        for (auto& entry : tick_system_order)
            if (entry.system)
                entry.system->Shutdown();
        tick_system_order.Clear();

        for (auto& entry : render_system_order)
            if (entry.system)
                entry.system->Shutdown();
        render_system_order.Clear();

        tick_systems.Clear();
        render_systems.Clear();
    }
}