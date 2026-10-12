#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/System.h>
#include <hgl/log/Log.h>
#include <algorithm>
#include <typeinfo>
#include <vector>

namespace hgl::ecs
{
    void ECSContext::SortTickSystems()
    {
        SortSystemList(tick_system_order, tick_dependencies, tick_order_dirty, "Tick");
    }

    void ECSContext::SortRenderSystems()
    {
        SortSystemList(render_system_order, render_dependencies, render_order_dirty, "Render");
    }

    void ECSContext::SortSystemList(std::vector<OrderedSystem>& order_list,
                                     const DependencyMap& dependencies,
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

        hgl::UnorderedMap<size_t, size_t> index_map;
        index_map.Reserve(order_list.size());

        for (size_t i = 0; i < order_list.size(); ++i)
        {
            index_map[order_list[i].key] = i;
        }

        std::vector<size_t> indegree(order_list.size(), 0);
        std::vector<std::vector<size_t>> adj(order_list.size());

        for (const auto& pair : dependencies)
        {
            const size_t dependent_key = pair.first;
            const size_t* dependent_index = index_map.GetValuePointer(dependent_key);
            if (!dependent_index)
                continue;

            for (const size_t dependency_key : pair.second)
            {
                const size_t* dependency_index = index_map.GetValuePointer(dependency_key);
                if (!dependency_index)
                {
                #ifdef _DEBUG
                    LogWarning("[ECSContext::SortSystemList] %s dependency missing for key %zu -> %zu",
                               label,
                               dependent_key,
                               dependency_key);
                #endif
                    continue;
                }

                adj[*dependency_index].push_back(*dependent_index);
                ++indegree[*dependent_index];
            }
        }

        auto order_compare = [&order_list](size_t a, size_t b)
        {
            const auto& lhs = order_list[a];
            const auto& rhs = order_list[b];
            if (lhs.phase != rhs.phase)
                return lhs.phase < rhs.phase;
            return lhs.insertion_order < rhs.insertion_order;
        };

        std::vector<size_t> available;
        available.reserve(order_list.size());

        for (size_t i = 0; i < order_list.size(); ++i)
        {
            if (indegree[i] == 0)
                available.push_back(i);
        }

        std::vector<OrderedSystem> sorted;
        sorted.reserve(order_list.size());

        while (!available.empty())
        {
            std::sort(available.begin(), available.end(), order_compare);
            const size_t current = available.front();
            available.erase(available.begin());

            sorted.push_back(order_list[current]);

            for (const size_t next : adj[current])
            {
                if (--indegree[next] == 0)
                    available.push_back(next);
            }
        }

        if (sorted.size() != order_list.size())
        {
            std::stable_sort(order_list.begin(), order_list.end(),
                [](const OrderedSystem& a, const OrderedSystem& b)
                {
                    if (a.phase != b.phase)
                        return a.phase < b.phase;
                    return a.insertion_order < b.insertion_order;
                });

            LogWarning("[ECSContext::SortSystemList] %s system dependencies contain a cycle. Falling back to phase/insertion order.",label);
        }
        else
        {
            order_list.swap(sorted);
        }

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
        auto& dirty_flag = is_render ? render_order_dirty : tick_order_dirty;
        auto& deps = is_render ? render_dependencies : tick_dependencies;

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

        if (auto *entry = FindOrderedSystem(order_list, key))
        {
            entry->system = system;
            entry->phase = effective_phase;
            dirty_flag = true;
        }
        else
        {
            OrderedSystem new_entry;
            new_entry.key = key;
            new_entry.phase = effective_phase;
            new_entry.insertion_order = next_system_order++;
            new_entry.system = system;
            order_list.push_back(std::move(new_entry));
            dirty_flag = true;
        }

        deps.DeleteByKey(key);

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
        if (dependent_key == dependency_key)
            return;

        auto& deps = is_render ? render_dependencies : tick_dependencies;
        auto& order_dirty = is_render ? render_order_dirty : tick_order_dirty;

        auto* list = deps.GetValuePointer(dependent_key);
        if (!list)
        {
            deps.Add(dependent_key, std::vector<size_t>{});
            list = deps.GetValuePointer(dependent_key);
        }

        const auto it = std::find(list->begin(), list->end(), dependency_key);
        if (it == list->end())
        {
            list->push_back(dependency_key);
            order_dirty = true;
        }
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

        return installed_system_groups[static_cast<uint32_t>(group)];
    }

    void ECSContext::MarkSystemGroupInstalled(SystemGroupID group)
    {
        if (group == SystemGroupID::None)
            return;

        installed_system_groups[static_cast<uint32_t>(group)] = true;
    }

    void ECSContext::ResetComponentDrivenGroups()
    {
        for (uint32_t g = 0; g < kSystemGroupCount; ++g)
        {
            system_group_component_counts[g] = 0;
            installed_system_groups[g] = false;
            group_systems[g].clear();
        }
    }
}
