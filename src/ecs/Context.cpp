#include<hgl/ecs/Context.h>
#include<hgl/ecs/EntityManager.h>
#include<hgl/ecs/TransformSystem.h>
#include<hgl/ecs/VisibilitySystem.h>
#include<hgl/ecs/InputSystem.h>
#include<hgl/ecs/SubWorldComponent.h>
#include<hgl/ecs/MaterialBatch.h>
#include<hgl/ecs/PrimitiveRenderItem.h>
#include"ECSTransformAssignmentBuffer.h"
#include<algorithm>

namespace hgl
{
    namespace ecs
    {
        RenderFrameCache::~RenderFrameCache() = default;

        void RenderFrameCache::BeginFrame()
        {
            renderItems.clear();
            renderableCount = 0;

            for (auto& pair : materialBatches)
            {
                if (pair.second)
                    pair.second->Clear();
            }
        }

        ECSContext::ECSContext(const std::string& name)
            : Object(name)
            , entity_manager(std::make_unique<EntityManager>(1000))
            , active(false)
        {
        }

        ECSContext::~ECSContext()
        {
            Shutdown();
        }

        void ECSContext::AttachToParent(ECSContext* parent)
        {
            if (!parent)
                return;

            parent_context = parent;
            owns_transform_storage = false;

            // Don't create our own transform storage if we have a parent
            // The TransformComponent will use the shared storage from the parent
        }

        void ECSContext::Initialize()
        {
            // Ensure TransformSystem is registered and bound to this world
            {
                auto transform_system = GetSystem<TransformSystem>();
                if (!transform_system)
                {
                    transform_system = RegisterTickSystem<TransformSystem>();
                }

                if (transform_system)
                {
                    transform_system->SetWorld(this);
                }
            }

            // Ensure VisibilitySystem is registered
            {
                auto visibility_system = GetSystem<VisibilitySystem>();
                if (!visibility_system)
                {
                    visibility_system = RegisterTickSystem<VisibilitySystem>();
                }

                if (visibility_system)
                {
                    visibility_system->SetWorld(this);
                    // VulkanDevice will be set later when available
                }
            }

                SortTickSystems();
                SortRenderSystems();

            // Initialize all systems
                for (auto& entry : tick_system_order)
                {
                    if (entry.system)
                    {
                        entry.system->OnDependenciesReady();
                        entry.system->Initialize();
                    }
                }

                for (auto& entry : render_system_order)
                {
                    if (entry.system)
                    {
                        entry.system->OnDependenciesReady();
                        entry.system->Initialize();
                    }
                }

            active = true;
            OnCreate();
        }

        void ECSContext::Shutdown()
        {
            if (!active)
                return;

            // Destroy all entities
            if (entity_manager)
            {
                entity_manager->Clear();
            }

            component_registry.Clear();
            static_transforms.clear();
            movable_transforms.clear();

            // Shutdown all systems
            SortTickSystems();
            SortRenderSystems();

            for (auto& entry : tick_system_order)
            {
                if (entry.system)
                    entry.system->Shutdown();
            }
            tick_systems.Clear();
            tick_system_order.clear();

            for (auto& entry : render_system_order)
            {
                if (entry.system)
                    entry.system->Shutdown();
            }
            render_systems.Clear();
            render_system_order.clear();

            active = false;
            OnDestroy();
        }

        void ECSContext::Tick(float deltaTime)
        {
            if (!active)
                return;

            SortTickSystems();

            // Update non-render systems
            for (auto& entry : tick_system_order)
            {
                if (entry.system)
                {
                    if (system_profiling_enabled)
                        profiler.Begin(entry.system.get());
                    entry.system->Update(deltaTime);
                    if (system_profiling_enabled)
                        profiler.End(entry.system.get());
                }
            }

            // Update all entities
            if (entity_manager)
            {
                std::vector<Entity*> entities;
                entity_manager->GetAllEntityPointers(entities);
                for (auto entity : entities)
                {
                    if (entity)
                        entity->OnUpdate(deltaTime);
                }
            }

            // Update sub-worlds attached via SubWorldComponent
            {
                std::vector<std::shared_ptr<SubWorldComponent>> sub_worlds;
                GetComponents(sub_worlds);
                for (const auto& sub_world : sub_worlds)
                {
                    if (sub_world)
                        sub_world->UpdateSubWorld(deltaTime);
                }
            }

            if (auto input_system = GetSystem<InputSystem>())
            {
                input_system->EndFrame();
            }
        }

        void ECSContext::Render(graph::RenderCmdBuffer *cmd, float deltaTime)
        {
            if (!active)
                return;

            SortRenderSystems();

            for (auto& entry : render_system_order)
            {
                if (entry.system)
                {
                    if (system_profiling_enabled)
                        profiler.Begin(entry.system.get());
                    entry.system->Update(deltaTime);
                    if (system_profiling_enabled)
                        profiler.End(entry.system.get());
                }
            }

            if (auto transform_system = GetSystem<TransformSystem>())
            {
                transform_system->SubmitTransformUpdates();
            }

            for (auto& entry : render_system_order)
            {
                if (entry.system)
                    entry.system->Render(cmd, deltaTime);
            }

            // Render sub-worlds attached via SubWorldComponent
            {
                std::vector<std::shared_ptr<SubWorldComponent>> sub_worlds;
                GetComponents(sub_worlds);
                for (const auto& sub_world : sub_worlds)
                {
                    if (sub_world)
                        sub_world->RenderSubWorld(cmd, deltaTime);
                }
            }
        }

        void ECSContext::ClearEntities()
        {
            if (entity_manager)
                entity_manager->Clear();

            component_registry.Clear();
            static_transforms.clear();
            movable_transforms.clear();
        }

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
            index_map.SetMaxCount(order_list.size());

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
                        std::cout << "[ECSContext::SortSystemList] WARNING: " << label
                                  << " dependency missing for key " << dependent_key
                                  << " -> " << dependency_key << std::endl;
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
                // First sort by phase
                if (lhs.phase != rhs.phase)
                    return lhs.phase < rhs.phase;
                // Then sort by priority within phase
                if (lhs.priority != rhs.priority)
                    return lhs.priority < rhs.priority;
                // Finally use insertion order for stable sort
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
                        if (a.priority != b.priority)
                            return a.priority < b.priority;
                        return a.insertion_order < b.insertion_order;
                    });

                std::cout << "[ECSContext::SortSystemList] WARNING: " << label
                          << " system dependencies contain a cycle. Falling back to phase/priority order." << std::endl;
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

        void ECSContext::AddOrUpdateSystem(bool is_render, size_t key, const std::shared_ptr<System>& system, int priority)
        {
            auto& sys_map = is_render ? render_systems : tick_systems;
            auto& order_list = is_render ? render_system_order : tick_system_order;
            auto& dirty_flag = is_render ? render_order_dirty : tick_order_dirty;

            sys_map[key] = system;

            // Set the context for the system so it can access entities and create queries
            if (system)
            {
                system->SetContext(this);
            }

            // Extract phase and priority from system
            int effective_phase = static_cast<int>(system->GetExecutionPhase());
            int effective_priority = static_cast<int>(system->GetExecutionPriority());

            if (auto *entry = FindOrderedSystem(order_list, key))
            {
                entry->system = system;
                entry->phase = effective_phase;
                entry->priority = effective_priority;
                dirty_flag = true;
            }
            else
            {
                OrderedSystem new_entry;
                new_entry.key = key;
                new_entry.phase = effective_phase;
                new_entry.priority = effective_priority;
                new_entry.insertion_order = next_system_order++;
                new_entry.system = system;
                order_list.push_back(std::move(new_entry));
                dirty_flag = true;
            }
            
            // Automatically register dependencies declared by the system
            if (system)
            {
                const auto& deps = system->GetDependencies();
                for (const auto& dep_type : deps)
                {
                    size_t dep_key = dep_type.hash_code();
                    AddSystemDependency(is_render, key, dep_key);
                }
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

        void ECSContext::SetFrameIndex(const uint32_t index)
        {
            frame_index = index;
            ECSTransformAssignmentBuffer::SetFrameIndex(index);
        }

        void ECSContext::RegisterComponentInstance(size_t type_hash, const std::shared_ptr<Component>& comp)
        {
            if(!comp)
                return;

            auto* vec = component_registry.GetValuePointer(type_hash);
            if (!vec)
            {
                component_registry.Add(type_hash, std::vector<std::weak_ptr<Component>>{});
                vec = component_registry.GetValuePointer(type_hash);
            }
            vec->push_back(comp);
        }

        void ECSContext::UnregisterComponentInstance(size_t type_hash, Component* comp_ptr)
        {
            auto* vec = component_registry.GetValuePointer(type_hash);
            if (!vec)
                return;

            vec->erase(std::remove_if(vec->begin(), vec->end(), [comp_ptr](const std::weak_ptr<Component>& w){
                auto sp = w.lock();
                return !sp || sp.get()==comp_ptr;
            }), vec->end());
        }

        void ECSContext::RegisterTransformComponent(const std::shared_ptr<TransformComponent>& comp, bool isMovable)
        {
            if (!comp)
                return;

            if (isMovable)
            {
                movable_transforms.push_back(comp);
            }
            else
            {
                static_transforms.push_back(comp);
            }
        }

        void ECSContext::MigrateTransformComponent(TransformComponent* comp_ptr, bool toMovable)
        {
            if (!comp_ptr)
                return;

            auto remove_from_list = [comp_ptr](std::vector<std::weak_ptr<TransformComponent>>& list)
            {
                list.erase(std::remove_if(list.begin(), list.end(),
                    [comp_ptr](const std::weak_ptr<TransformComponent>& w)
                    {
                        auto sp = w.lock();
                        return !sp || sp.get() == comp_ptr;
                    }), list.end());
            };

            // Remove from current list
            if (toMovable)
            {
                remove_from_list(static_transforms);
            }
            else
            {
                remove_from_list(movable_transforms);
            }

            // Add to new list
            std::shared_ptr<TransformComponent> comp_shared;

            if (auto owner = comp_ptr->GetOwner())
            {
                comp_shared = owner->GetComponent<TransformComponent>();
            }

            if (!comp_shared)
                return;

            auto add_unique = [&comp_ptr](std::vector<std::weak_ptr<TransformComponent>>& list,
                                          const std::shared_ptr<TransformComponent>& comp)
            {
                for (const auto& weak_comp : list)
                {
                    if (auto existing = weak_comp.lock())
                    {
                        if (existing.get() == comp_ptr)
                            return;
                    }
                }
                list.push_back(comp);
            };

            if (toMovable)
                add_unique(movable_transforms, comp_shared);
            else
                add_unique(static_transforms, comp_shared);
        }

        void ECSContext::UnregisterTransformComponent(TransformComponent* comp_ptr)
        {
            if (!comp_ptr)
                return;

            auto remove_from_list = [comp_ptr](std::vector<std::weak_ptr<TransformComponent>>& list)
            {
                list.erase(std::remove_if(list.begin(), list.end(),
                    [comp_ptr](const std::weak_ptr<TransformComponent>& w)
                    {
                        auto sp = w.lock();
                        return !sp || sp.get() == comp_ptr;
                    }), list.end());
            };

            remove_from_list(static_transforms);
            remove_from_list(movable_transforms);
        }

        void ECSContext::NotifyComponentAdded(EntityID entity_id, const std::type_index& component_type)
        {
            // Get entity for predicate checking
            Entity* entity = GetEntity(entity_id);
            if (!entity)
                return;

            // Notify all tick systems - directly push entity to matching queries
            for (auto& [key, system] : tick_systems)
            {
                if (system && system->GetCache())
                {
                    system->GetCache()->OnComponentAdded(entity_id, component_type, entity);
                }
            }

            // Notify all render systems - directly push entity to matching queries
            for (auto& [key, system] : render_systems)
            {
                if (system && system->GetCache())
                {
                    system->GetCache()->OnComponentAdded(entity_id, component_type, entity);
                }
            }
        }

        void ECSContext::NotifyComponentRemoved(EntityID entity_id, const std::type_index& component_type)
        {
            // Notify all tick systems - remove entity from affected queries
            for (auto& [key, system] : tick_systems)
            {
                if (system && system->GetCache())
                {
                    system->GetCache()->OnComponentRemoved(entity_id, component_type);
                }
            }

            // Notify all render systems - remove entity from affected queries
            for (auto& [key, system] : render_systems)
            {
                if (system && system->GetCache())
                {
                    system->GetCache()->OnComponentRemoved(entity_id, component_type);
                }
            }
        }
    }//namespace ecs
}//namespace hgl

