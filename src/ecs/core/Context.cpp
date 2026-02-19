#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/EntityManager.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/VisibilitySystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/components/SubWorldComponent.h>
#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/support/ECSTransformAssignmentBuffer.h>
#include<hgl/ecs/systems/render/RenderSystemCore.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/log/Log.h>
#include<hgl/object/ObjectTracker.h>
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

            // 清空批次内容，保留对象以供重用
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
        
        bool ECSContext::InitializeGraphics(hgl::graph::VulkanDevice* device, hgl::graph::IRenderTarget* target) {
            if (!device || !target) {
                // Phase 1 debug: device or target is null
                return false;
            }
            
            gpu_device = device;
            render_target = target;
            
            // Phase 1: Initialized with GPU device and render target
            return true;
        }

        void ECSContext::Initialize()
        {
            RegisterComponentQueryBase<RenderableComponent>();
            RegisterComponentQueryBase<PrimitiveComponent>();

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
            if (auto *device = GetGPUDevice())
                device->WaitIdle();

            // Release render-frame items first  (only clears renderItems, keeps materialBatches for reuse)
            render_frame_cache.renderItems.clear();
            render_frame_cache.cameraInfo = nullptr;
            render_frame_cache.renderableCount = 0;

            if (!active)
            {
                // 即使未激活，也要清空materialBatches以释放GPU资源
                std::cout << "[DEBUG] ECSContext::Shutdown() (inactive) - releasing " 
                          << render_frame_cache.materialBatches.GetCount() << " material batches" << std::endl;
                render_frame_cache.materialBatches.Clear();
                return;
            }

            // Destroy all systems FIRST(before clearing materialBatches)
            SortTickSystems();
            SortRenderSystems();

            for (auto& entry : tick_system_order)
            {
                if (entry.system)
                    entry.system->Shutdown();
            }
            tick_system_order.clear();

            for (auto& entry : render_system_order)
            {
                if (entry.system)
                    entry.system->Shutdown();
            }
            render_system_order.clear();

            tick_systems.Clear();
            render_systems.Clear();

            // Destroy all entities
            if (entity_manager)
            {
                entity_manager->Clear();
            }

            component_registry.Clear();
            static_transforms.clear();
            movable_transforms.clear();

            // Finally, clear materialBatches after all systems/entities are destroyed
            std::cout << "[DEBUG] ECSContext::Shutdown() - releasing " 
                      << render_frame_cache.materialBatches.GetCount() << " material batches" << std::endl;
            render_frame_cache.materialBatches.Clear();
            std::cout << "[DEBUG] ECSContext::Shutdown() - material batches cleared" << std::endl;

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
                    RunSystemUpdate(entry.system.get(), deltaTime);
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
            
            // (Phase 1) 设置当前渲染命令缓冲区（如果没有由 RenderSystemCore 设置）
            if (!current_render_cmd && cmd) {
                current_render_cmd = cmd;
            }

            RunRenderUpdatesRange(ExecutionPhase::RenderCollect, ExecutionPhase::RenderPostProcess, deltaTime);

            if (auto transform_system = GetSystem<TransformSystem>())
            {
                transform_system->SubmitTransformUpdates();
            }

            for (auto& entry : render_system_order)
            {
                if (!entry.system)
                    continue;

                if (entry.phase < static_cast<int>(ExecutionPhase::RenderCollect))
                    continue;

                if (entry.phase > static_cast<int>(ExecutionPhase::RenderPostProcess))
                    continue;

                if (entry.system)
                {
                    HGL_CAPTURE_SCOPE();
                    GLogDebug("[ECS] Render Begin: %s", entry.system->GetName().c_str());
                    entry.system->Render(cmd, deltaTime);
                    GLogDebug("[ECS] Render End: %s", entry.system->GetName().c_str());
                }
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
            
            // (Phase 1) 清除当前命令缓冲区（如果是我们设置的）
            if (current_render_cmd == cmd) {
                current_render_cmd = nullptr;
            }
        }

        void ECSContext::OnResize(const VkExtent2D &extent)
        {
            HGL_CAPTURE_SCOPE();

            if (!active)
                return;

            // Log resize event
            GLogInfo("[ECSContext] OnResize: %s %ux%u", 
                     GetName().c_str(), extent.width, extent.height);

            // Notify RenderTargetSystem to sync viewport and dependent systems
            auto render_target_system = GetSystem<RenderTargetSystem>();
            if (render_target_system)
            {
                // RenderTargetSystem will sync CameraSystem viewport
                render_target_system->SetRenderTarget(render_target);
            }
            else
            {
                // Fallback: directly update CameraSystem if no RenderTargetSystem
                auto camera_system = GetSystem<CameraSystem>();
                if (camera_system && render_target)
                    camera_system->SetViewportInfo(render_target->GetViewportInfo());

                // CN: LineRenderSystem 会在 Render 时延迟初始化
                // EN: LineRenderSystem lazy-inits on first Render
            }
        }

        void ECSContext::Render(float deltaTime)
        {
            Render(deltaTime, nullptr);
        }

        void ECSContext::Render(float deltaTime, const std::function<void(float)> &pre_render)
        {
            if (!active)
                return;

            GLogInfo("[ECS RENDER] ===== Frame Start =====");

            if (!render_core)
            {
                render_core = std::make_unique<RenderSystemCore>(this);
                if (!render_core->Initialize())
                {
                    render_core.reset();
                    return;
                }
            }

            if (auto *rt = GetRenderTarget())
            {
                GLogInfo("[ECS RENDER] Calling WaitFence");
                if (!rt->WaitFence())
                {
                    GLogWarning("[ECS RENDER] WaitFence FAILED");
                    return;
                }
            }

            GLogInfo("[ECS RENDER] Calling BeginFrame");
            if (!render_core->BeginFrame())
            {
                GLogWarning("[ECS RENDER] BeginFrame FAILED");
                return;
            }

            render_core->SetClearColor(clear_color);

            if (pre_render)
                pre_render(deltaTime);

            GLogInfo("[ECS RENDER] Calling Render(cmd)");
            Render(render_core->GetRenderCmd(), deltaTime);
            
            GLogInfo("[ECS RENDER] Calling EndFrame");
            render_core->EndFrame();

            if (wait_idle_enabled)
            {
                GLogInfo("[ECS RENDER] Calling WaitIdle");
                if (auto *device = GetGPUDevice())
                    device->WaitIdle();
            }
            
            GLogInfo("[ECS RENDER] ===== Frame End =====");
        }

        void ECSContext::RenderPreBeginFrame(float deltaTime)
        {
            if (!active)
                return;

            RunRenderPhaseUpdates(ExecutionPhase::RenderPreBeginFrame, deltaTime);
        }

        void ECSContext::RenderSwapchainNextImage(float deltaTime)
        {
            if (!active)
                return;

            RunRenderPhaseUpdates(ExecutionPhase::RenderSwapchainNextImage, deltaTime);
        }

        void ECSContext::RenderBeginFrame(float deltaTime)
        {
            if (!active)
                return;

            RunRenderPhaseUpdates(ExecutionPhase::RenderBeginFrame, deltaTime);
        }

        void ECSContext::RenderPostBeginFrame(float deltaTime)
        {
            if (!active)
                return;

            RunRenderPhaseUpdates(ExecutionPhase::RenderPostBeginFrame, deltaTime);
        }

        void ECSContext::RenderSubmit(float deltaTime)
        {
            if (!active)
                return;

            RunRenderPhaseUpdates(ExecutionPhase::RenderSubmit, deltaTime);
        }

        void ECSContext::RunRenderPhaseUpdates(ExecutionPhase phase, float deltaTime)
        {
            SortRenderSystems();

            for (auto& entry : render_system_order)
            {
                if (!entry.system)
                    continue;

                if (entry.phase != static_cast<int>(phase))
                    continue;

                RunSystemUpdate(entry.system.get(), deltaTime);
            }
        }

        void ECSContext::RunRenderUpdatesFrom(ExecutionPhase phase, float deltaTime)
        {
            SortRenderSystems();

            const int min_phase = static_cast<int>(phase);

            for (auto& entry : render_system_order)
            {
                if (!entry.system)
                    continue;

                if (entry.phase < min_phase)
                    continue;

                RunSystemUpdate(entry.system.get(), deltaTime);
            }
        }

        void ECSContext::RunRenderUpdatesRange(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime)
        {
            SortRenderSystems();

            const int min_phase = static_cast<int>(minPhase);
            const int max_phase = static_cast<int>(maxPhase);

            for (auto& entry : render_system_order)
            {
                if (!entry.system)
                    continue;

                if (entry.phase < min_phase || entry.phase > max_phase)
                    continue;

                RunSystemUpdate(entry.system.get(), deltaTime);
            }
        }

        void ECSContext::RunSystemUpdate(System *system, float deltaTime)
        {
            if (!system)
                return;

            HGL_CAPTURE_SCOPE();
            GLogDebug("[ECS] Update Begin: %s", system->GetName().c_str());

            if (system_profiling_enabled)
                profiler.Begin(system);
            system->Update(deltaTime);
            if (system_profiling_enabled)
                profiler.End(system);

            GLogDebug("[ECS] Update End: %s", system->GetName().c_str());
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

            RegisterComponentInstanceInternal(type_hash, comp);

            for (const auto& entry : component_query_bases)
            {
                if (entry.key == type_hash)
                    continue;

                if (entry.matches && entry.matches(comp.get()))
                    RegisterComponentInstanceInternal(entry.key, comp);
            }
        }

        void ECSContext::UnregisterComponentInstance(size_t type_hash, Component* comp_ptr)
        {
            (void)type_hash;
            if (!comp_ptr)
                return;

            for (auto& pair : component_registry)
            {
                auto& vec = pair.second;
                vec.erase(std::remove_if(vec.begin(), vec.end(), [comp_ptr](const std::weak_ptr<Component>& w){
                    auto sp = w.lock();
                    return !sp || sp.get()==comp_ptr;
                }), vec.end());
            }
        }

        void ECSContext::RegisterComponentInstanceInternal(size_t type_hash, const std::shared_ptr<Component>& comp)
        {
            if (!comp)
                return;

            auto* vec = component_registry.GetValuePointer(type_hash);
            if (!vec)
            {
                component_registry.Add(type_hash, std::vector<std::weak_ptr<Component>>{});
                vec = component_registry.GetValuePointer(type_hash);
            }

            for (const auto& weak_comp : *vec)
            {
                if (auto existing = weak_comp.lock())
                {
                    if (existing.get() == comp.get())
                        return;
                }
            }

            vec->push_back(comp);
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


