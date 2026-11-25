#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/System.h>
#include <hgl/ecs/core/SystemOrderPlanner.h>
#include <hgl/log/Log.h>
#include <algorithm>
#include <typeinfo>
#include <vector>

namespace hgl::ecs
{
    void ECSContext::SortTickSystems()
    {
        SortSystemList(tick_system_order, system_schedule.Dependencies(false),
                       system_schedule.Dirty(false), "Tick");
    }

    void ECSContext::SortRenderSystems()
    {
        SortSystemList(render_system_order, system_schedule.Dependencies(true),
                       system_schedule.Dirty(true), "Render");
    }

    void ECSContext::SortSystemList(std::vector<OrderedSystem>& order_list,
                                     const SystemScheduleState::DependencyMap& dependencies,
                                     bool& dirty_flag,
                                     const char* label)
    {
        if (!dirty_flag)
            return;

        if (order_list.empty())
        {
            dirty_flag = false;
            return;
        }

        SystemOrderPlanner planner;
        for (const auto &entry : order_list)
            planner.AddSystem(entry.key, entry.phase, entry.group_id, entry.insertion_order);

        for (const auto& pair : dependencies)
        {
            for (const size_t dependency_key : pair.second)
            {
                const auto result = planner.AddDependency(pair.first, dependency_key);
                #ifdef _DEBUG
                if (result == SystemOrderPlanner::DependencyResult::MissingDependency)
                    LogWarning("[ECSContext::SortSystemList] %s dependency missing for key %zu -> %zu",
                               label, pair.first, dependency_key);
                #else
                (void)result;
                #endif
            }
        }

        ValueArray<int> indices;
        const bool acyclic = planner.Build(indices);
        std::vector<OrderedSystem> sorted;
        sorted.reserve(order_list.size());
        for (const int index : indices)
            sorted.push_back(order_list[index]);
        order_list.swap(sorted);
        if (!acyclic)
            LogWarning("[ECSContext::SortSystemList] %s system dependencies contain a cycle. Falling back to phase/group/insertion order.",label);

        dirty_flag = false;
    }

    ECSContext::OrderedSystem* ECSContext::FindOrderedSystem(std::vector<OrderedSystem>& list, size_t key)
    {
        for (auto& entry : list)
        {
            if (entry.key == key)
                return &entry;
        }

        return nullptr;
    }

    void ECSContext::AddOrUpdateSystem(size_t key, const std::shared_ptr<System>& system)
    {
        if (!system)
            return;

        system->SetContext(this);

        // tick / render 分类的唯一权威 = 系统自己的 ExecutionPhase。
        // 旧版还收一个调用方声明的 `is_render`：相位不一致时只 LogWarning 一句就把系统
        // 静默换表（"纠正"），声明面因此是纯噪音 ⇒ 已整体删除（`Context.h` 的 `RegisterSystem`）。
        const int effective_phase = static_cast<int>(system->GetExecutionPhase());
        const bool is_render = effective_phase >= static_cast<int>(ExecutionPhase::RenderSwapchainNextImage);

        auto& sys_map = is_render ? render_systems : tick_systems;
        auto& order_list = is_render ? render_system_order : tick_system_order;
        auto& dirty_flag = system_schedule.Dirty(is_render);

        // ── 同键替换：旧实例必须先 Shutdown ──
        // RegisterSystem<T>() 重复注册（或外部替换实现）会覆盖 sys_map[key]；旧实例随后
        // 已不在世界容器里，Shutdown 再也不会被调用 ⇒ 它持有的 GPU 缓冲/纹理/监听器永不
        // 释放。旧实例还可能因执行相位变化落在另一张表（tick ↔ render），两张都要摘。
        auto retire_replaced_system = [this, key, &system](
            hgl::UnorderedMap<size_t, std::shared_ptr<System>> &map,
            std::vector<OrderedSystem> &order,
            bool &dirty)
        {
            std::shared_ptr<System> old_system;
            if (auto *slot = map.GetValuePointer(key))
            {
                old_system = *slot;
                map.DeleteByKey(key);
            }

            // 调度列表里的同键条目一并摘除：否则旧实例仍会每帧被调度。
            for (size_t i = 0; i < order.size();)
            {
                if (order[i].key == key)
                {
                    order.erase(order.begin() + i);
                    dirty = true;
                    continue;
                }
                ++i;
            }

            if (!old_system || old_system == system)
                return;

            LogDebug("[ECSContext] replacing system key=%zu: shutting down previous instance %s",
                     key, typeid(*old_system).name());

            // 旧实例在本调用返回后可能只剩调用方的 shared_ptr，这里先放掉它持有的
            // 世界资源（与 Shutdown 时对在册系统的处理一致）。
            old_system->Shutdown();
        };

        retire_replaced_system(tick_systems, tick_system_order, system_schedule.Dirty(false));
        retire_replaced_system(render_systems, render_system_order, system_schedule.Dirty(true));

        sys_map[key] = system;

        const AnsiString& element_type = system->GetRenderElementType();
        if (!element_type.IsEmpty())
        {
            auto& systems_list = systems_by_element_type[element_type.c_str()];

            {
                const SystemGroupID changed_group = SystemGroupFromName(element_type.c_str());

                if (changed_group != SystemGroupID::None)
                    group_systems[static_cast<uint32_t>(changed_group)].clear();
            }
            // 2026-10-11（§1 S1）：按**键**摘旧实例。原写法 `*it == system` 是"同一实例"比较 ⇒
            // `RegisterSystem<T>()` 再注册（替换同类型系统）时，列表里没有任何元素等于**新**实例，
            // 于是**旧实例永远留在列表里**；`SetSystemGroupEnabled` 从该列表重建快照
            // ⇒ 快照同时挂着新旧两个实例，组开关会去开关一个"已不在 sys_map 里"的僵尸实例
            // （`RenderGraph` 取该列表算组的相位区间时也会把僵尸算进去）。
            // 键 = `typeid(T).hash_code()`（见 `Context::RegisterSystem`）⇒ 同类型即同键，
            // 按类型比较等价于按键比较。
            systems_list.erase(
                std::remove_if(systems_list.begin(), systems_list.end(),
                               [&system](const std::shared_ptr<System> &candidate)
                               {
                                   return !candidate || typeid(*candidate) == typeid(*system);
                               }),
                systems_list.end());

            systems_list.push_back(system);
        }

        const int group_id = static_cast<int>(SystemGroupFromName(element_type.c_str()));

        if (auto *entry = FindOrderedSystem(order_list, key))
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
            new_entry.insertion_order = system_schedule.NextInsertionOrder();
            new_entry.system = system;
            order_list.push_back(std::move(new_entry));
            dirty_flag = true;
        }

        system_schedule.RemoveDependencies(is_render, key);

        const auto& deps_decl = system->GetDependencies();
        for (const auto& dep_type : deps_decl)
        {
            size_t dep_key = dep_type.hash_code();
            AddSystemDependency(is_render, key, dep_key);
        }
        if (active)
        {
            system->Initialize();
        }
    }

    void ECSContext::AddSystemDependency(bool is_render, size_t dependent_key, size_t dependency_key)
    {
        system_schedule.AddDependency(is_render, dependent_key, dependency_key);
    }

    void ECSContext::GetSystemsByElementType(const std::string& element_type, std::vector<std::shared_ptr<System>>& out_systems) const
    {
        out_systems.clear();
        auto it = systems_by_element_type.find(element_type);
        if (it != systems_by_element_type.end())
        {
            out_systems = it->second;
        }
    }

    void ECSContext::GetAllRenderElementTypes(std::vector<std::string>& out_element_types) const
    {
        out_element_types.clear();
        out_element_types.reserve(systems_by_element_type.size());

        for (const auto& pair : systems_by_element_type)
        {
            out_element_types.push_back(pair.first);
        }
    }

    void ECSContext::SetSystemGroupEnabled(SystemGroupID group, bool enabled)
    {
        if (group == SystemGroupID::None)
            return;

        const uint32_t index = static_cast<uint32_t>(group);

        if (!group_systems[index].empty())
        {
            for (auto& system : group_systems[index])
                if (system)
                    system->SetEnabled(enabled);

            return;
        }

        auto it = systems_by_element_type.find(kSystemGroupNames[index]);
        if (it == systems_by_element_type.end())
            return;

        auto& snapshot = group_systems[index];
        snapshot.clear();
        snapshot.reserve(it->second.size());

        for (auto& system : it->second)
        {
            if (system)
            {
                snapshot.push_back(system);
                system->SetEnabled(enabled);
            }
        }
    }

    bool ECSContext::IsSystemGroupInstalled(SystemGroupID group) const
    {
        if (group == SystemGroupID::None)
            return false;

        return system_group_activity.IsInstalled(group);
    }

    void ECSContext::MarkSystemGroupInstalled(SystemGroupID group)
    {
        if (group == SystemGroupID::None)
            return;

        system_group_activity.MarkInstalled(group);
    }

    void ECSContext::ResetComponentDrivenGroups()
    {
        system_group_activity.Reset();
        for (uint32_t g = 0; g < kSystemGroupCount; ++g)
            group_systems[g].clear();
    }
}
