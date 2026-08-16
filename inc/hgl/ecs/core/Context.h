#pragma once

#include<hgl/vk/VK.h>
#include<hgl/ecs/core/Object.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/System.h>
#include<hgl/ecs/core/RenderGraph.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/core/EntityManager.h>
#include<hgl/ecs/core/SystemProfiler.h>
#include<hgl/log/Log.h>
#include<memory>
#include<functional>
#include<vector>
#include<map>
#include<set>
#include<unordered_map>
#include <hgl/type/UnorderedMap.h>
#include<typeinfo>
#include<type_traits>
#include<hgl/ecs/core/ShaderProgramPipelineKey.h>
#include<hgl/color/Color4f.h>

#ifndef ULRE_ECS_DEBUG_API
#define ULRE_ECS_DEBUG_API 1
#endif

namespace hgl {
    namespace graph {
        class RenderCmdBuffer;
        class CameraInfo;
        class IRenderTarget;
        class GraphicsContext;  // 图形资源管理器（原IGraphicsContext）
        class VulkanDevice;
        class RenderContext;
    }
}

namespace hgl
{
    namespace ecs
    {
        class CameraSystem;
        class RenderSystemCore;
        class RenderPipelineBase;
        class MaterialBatch;
        class RenderItem;

        struct RenderFrameCache
        {
            std::vector<std::unique_ptr<RenderItem>> renderItems;
            hgl::UnorderedMap<ShaderProgramPipelineKey, std::unique_ptr<MaterialBatch>> materialBatches;
            const graph::CameraInfo* cameraInfo = nullptr;
            uint32_t renderableCount = 0;

            RenderFrameCache() = default;
            ~RenderFrameCache();

            void BeginFrame();
        };

        /**
         * ECSContext manages all entities and systems
         * Acts as the main container for the ECS simulation
         */
        class ECSContext : public Object
        {
        public:

            ECSContext(const std::string& name = "World");
            ~ECSContext() override;

            enum class SystemOwnershipScope : uint8_t
            {
                Auto = 0,
                GlobalShared = 1,
                LocalIsolated = 2
            };

            enum class ContextRole : uint8_t
            {
                RootShared = 0,
                LocalSubWorld = 1
            };

        private:
            OBJECT_LOGGER

            std::unique_ptr<EntityManager> entity_manager;

            // 分类存储：更新系统与渲染系统分开
            hgl::UnorderedMap<size_t, std::shared_ptr<System>> tick_systems;
            hgl::UnorderedMap<size_t, std::shared_ptr<System>> render_systems;

            // 按render element type存储系统（用于运行时按名称查找和启用/禁用）
            std::map<std::string, std::vector<std::shared_ptr<System>>> systems_by_element_type;

            // Component-driven system-group activity tracking
            std::unordered_map<std::string, uint32_t> system_group_component_counts;
            std::set<std::string> known_system_groups;
            std::set<std::string> installed_system_groups;

            struct OrderedSystem
            {
                size_t key = 0;
                int phase = 0;          // ExecutionPhase as int
                uint64_t insertion_order = 0;  // For stable sorting
                std::shared_ptr<System> system;
            };

            using DependencyMap = hgl::UnorderedMap<size_t, std::vector<size_t>>;

            std::vector<OrderedSystem> tick_system_order;
            std::vector<OrderedSystem> render_system_order;
            bool tick_order_dirty = false;
            bool render_order_dirty = false;
            uint64_t next_system_order = 1;

            DependencyMap tick_dependencies;
            DependencyMap render_dependencies;

            // 组件注册表：按类型hash存储弱引用，便于系统快速查询
            hgl::UnorderedMap<size_t, std::vector<std::weak_ptr<Component>>> component_registry;

            struct ComponentQueryBase
            {
                size_t key = 0;
                std::function<bool(Component*)> matches;
            };

            std::vector<ComponentQueryBase> component_query_bases;

            // TransformComponent 分离列表
            std::vector<std::weak_ptr<TransformComponent>> static_transforms;
            std::vector<std::weak_ptr<TransformComponent>> movable_transforms;

            bool active = false;
            bool shutdown_in_progress = false;
            ContextRole context_role = ContextRole::RootShared;
            bool allow_render_system_registration = true;
            uint32_t rejected_render_system_registration_count = 0;
            bool rejected_render_system_registration_logged = false;
            uint32_t global_render_system_count = 0;
            uint32_t local_gameplay_system_count = 0;

            RenderFrameCache render_frame_cache;
            SystemProfiler profiler;
            bool system_profiling_enabled = true;
            uint32_t frame_index = 0;
            bool descriptor_contract_diag_log_enabled = false;
            uint64_t descriptor_contract_diag_last_log_ms = 0;
            uint32_t filtered_entity_count_last_frame = 0;

            /// Unified render pipeline registry: name → RenderPipelineBase
            /// Managed by SystemGroup installers (e.g., InstallPrimitiveGroup, InstallLineGroup)
            /// Supports dynamic enable/disable per SystemGroup
            std::unordered_map<std::string, std::unique_ptr<RenderPipelineBase>> render_pipelines;

            // ========== GPU 设备和资源管理（Phase 1 新增） ==========

            /// GPU 设备（从旧集中式入口迁移来）
            hgl::graph::VulkanDevice* gpu_device = nullptr;

            /// 用于渲染的目标
            hgl::graph::IRenderTarget* render_target = nullptr;

            /// 当前渲染 Pass 的命令缓冲区（在 Render() 执行期间有效）
            hgl::graph::RenderCmdBuffer* current_render_cmd = nullptr;

            std::unique_ptr<RenderSystemCore> render_core;
            bool wait_idle_enabled = false;
            hgl::Color4f clear_color{0,0,0,1};

            /// Cached default linear render graph (created once, reused every frame)
            mutable RenderGraph cached_default_render_graph;
            mutable bool default_render_graph_initialized = false;

            /// Cached adaptive render graph (auto-culls based on scene content)
            mutable RenderGraph cached_adaptive_render_graph;
            mutable uint64_t cached_adaptive_scene_hash = ~0ULL;  // sentinel: "not computed yet"
            mutable bool use_adaptive_render_graph = true;  // default: use adaptive mode

            /// Graphics context adapter (Phase 2) - now raw pointer
            hgl::graph::GraphicsContext* graphics_context = nullptr;
            hgl::graph::RenderContext* render_context = nullptr;

            /// Resource naming prefix for hierarchical tracking (e.g., "RenderToTexture:OffscreenRT")
            /// Used by systems when creating GPU resources for better leak tracking
            std::string resource_name_prefix;

        private:

            void SortTickSystems();
            void SortRenderSystems();
            void SortSystemList(std::vector<OrderedSystem>& order_list,
                                const DependencyMap& dependencies,
                                bool& dirty_flag,
                                const char* label);
            OrderedSystem* FindOrderedSystem(std::vector<OrderedSystem>& list, size_t key);
            void AddOrUpdateSystem(bool is_render, size_t key, const std::shared_ptr<System>& system);
            void AddSystemDependency(bool is_render, size_t dependent_key, size_t dependency_key);
            bool SetSystemEnabledByKey(size_t key, bool enabled);
            bool RemoveSystemByKey(size_t key);
            void RunRenderPhaseUpdates(ExecutionPhase phase, float deltaTime);
            void RunRenderUpdatesFrom(ExecutionPhase phase, float deltaTime);
            void RunRenderUpdatesRange(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime);
            void RunRenderSystemsInRange(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime);
            void RunSystemUpdate(System *system, float deltaTime);
            void RegisterComponentInstanceInternal(size_t type_hash, const std::shared_ptr<Component>& comp);
            bool EnsureRenderCoreInitialized();
            bool BeginManagedRenderFrame(float deltaTime);
            void EndManagedRenderFrame(float deltaTime);
            void RecordPreparedRenderPhaseRange(ExecutionPhase minPhase,
                                               ExecutionPhase maxPhase,
                                               float deltaTime,
                                               bool submit_transforms,
                                               const char *log_prefix);
            void ExecuteRenderGraphPasses(const RenderGraph& graph,
                                          float deltaTime,
                                          const std::function<void(float)> &pre_render);
        public:

            /// Initialize the world (基础初始化，获取 GPU 设备和渲染目标)
            /// @param device GPU 设备
            /// @param target 渲染目标
            /// @return 成功返回 true
            bool InitializeGraphics(hgl::graph::VulkanDevice* device, hgl::graph::IRenderTarget* target);

            /// Initialize the world (旧接口，保持向后兼容)
            void Initialize();

            /// Shut down the world
            void Shutdown();

            /// Tick all non-render systems and entities
            void Tick(float deltaTime);

            /// Compatibility entry for recording into an existing command buffer.
            /// Preferred public frame driver is Render(float).
            void Render(graph::RenderCmdBuffer *cmd, float deltaTime);

            /// Internal-style draw-only recording entry kept for compatibility with
            /// prepared-frame callers such as subworld/offscreen helpers.
            void RenderDrawOnly(graph::RenderCmdBuffer *cmd, float deltaTime);

            /// Run a full render frame (Begin/Render/End/Sync)
            void Render(float deltaTime);

            /// Run a full render frame with a pre-render callback
            void Render(float deltaTime, const std::function<void(float)> &pre_render);

            /// Run a full render frame using a custom RenderGraph (supports multi-RT, conditional passes)
            void Render(float deltaTime, const RenderGraph& graph);

            /// Run a full render frame using a custom RenderGraph with a pre-render callback
            void Render(float deltaTime, const RenderGraph& graph, const std::function<void(float)> &pre_render);

            /// Control whether to use adaptive RenderGraph (default: true)
            /// If true: automatically culls render passes based on scene content (no Primitives → skip collect/batch, etc.)
            /// If false: uses default linear graph regardless of scene content
            void SetAdaptiveRenderGraphEnabled(bool enabled) { use_adaptive_render_graph = enabled; }
            bool GetAdaptiveRenderGraphEnabled() const { return use_adaptive_render_graph; }

            /// Invalidate cached adaptive graph to force re-computation on next render
            void InvalidateAdaptiveRenderGraph() { cached_adaptive_scene_hash = ~0ULL; }

            /// Handle render target resize
            void OnResize(const VkExtent2D &extent);

            /// Run pre-begin-frame render updates (no command buffer).
            /// Covers RenderPreBeginFrame + RenderResourceSetup + RenderMaterialBind.
            void RenderPreBeginFrame(float deltaTime);

            /// Run lazy GPU resource-creation phase (QuadResourcePrepareSystem, etc.)
            void RenderResourceSetup(float deltaTime);

            /// Run per-entity material/texture binding phase (QuadMaterialBindingSystem, etc.)
            void RenderMaterialBind(float deltaTime);

            /// Run swapchain image acquisition updates (no command buffer)
            void RenderSwapchainNextImage(float deltaTime);

            /// Acquire swapchain image for current frame with ECS-system-first fallback.
            bool AcquireSwapchainImage(float deltaTime = 0.0f);

            /// Ensure render target viewport metadata is synced with current extent.
            void SyncRenderTargetViewport();

            /// Run begin-frame render updates (frame index available)
            void RenderBeginFrame(float deltaTime);

            /// Run explicit buffer commit cycle updates
            void RenderBufferCommit(float deltaTime);

            /// Run explicit buffer upload cycle updates (must run before render pass)
            void RenderBufferUpload(float deltaTime);

            /// Run frame-sync phase: sync UBOs/descriptors after upload
            /// (replaces RenderPostBeginFrame; runs before BeginRenderPass)
            void RenderFrameSync(float deltaTime);

            /// Orchestrate pre-pass render setup phases for a specific frame index.
            /// Strict order: BeginFrame → Collect → Batch → BufferCommit → BufferUpload → FrameSync
            void PrepareRenderPassSetup(uint32_t frameIndex, float deltaTime = 0.0f);

            /// Run frame submit updates (no command buffer)
            void RenderSubmit(float deltaTime);

            /// Submit current frame with ECS-system-first fallback.
            bool SubmitFrameToRenderTarget(float deltaTime = 0.0f);

            /// Clear all entities and component registries
            void ClearEntities();

            void SetSystemProfilingEnabled(bool enabled) { system_profiling_enabled = enabled; }
            bool IsSystemProfilingEnabled() const { return system_profiling_enabled; }
            SystemProfiler& GetSystemProfiler() { return profiler; }
            const SystemProfiler& GetSystemProfiler() const { return profiler; }

            bool GetDescriptorContractDiagnosticsExtended(uint32_t &materials_checked,
                                                         uint32_t &materials_unresolved,
                                                         uint32_t &required_missing,
                                                         uint32_t &optional_missing,
                                                         uint32_t &fallback_hits,
                                                         uint32_t &materials_registered,
                                                         uint32_t &binding_entries) const;

            void SetDescriptorContractDiagnosticsLogEnabled(bool enabled) { descriptor_contract_diag_log_enabled = enabled; }
            bool IsDescriptorContractDiagnosticsLogEnabled() const { return descriptor_contract_diag_log_enabled; }

            void SetFrameIndex(const uint32_t index);
            uint32_t GetFrameIndex() const { return frame_index; }

            void SetClearColor(const hgl::Color4f &color) { clear_color = color; }
            const hgl::Color4f &GetClearColor() const { return clear_color; }

            void SetWaitIdleEnabled(bool enabled) { wait_idle_enabled = enabled; }
            bool IsWaitIdleEnabled() const { return wait_idle_enabled; }

            // ========== GPU 设备和资源接口（Phase 1 新增） ==========

            /// 获取 GPU 设备
            hgl::graph::VulkanDevice* GetGPUDevice() { return gpu_device; }

            /// 设置渲染目标（用于窗口 resize 等场景重建 render target）
            void SetRenderTarget(hgl::graph::IRenderTarget* target) { render_target = target; }

            /// 获取渲染目标
            hgl::graph::IRenderTarget* GetRenderTarget() { return render_target; }

            /// 获取当前渲染命令缓冲区（仅在 Render() 执行期间有效）
            hgl::graph::RenderCmdBuffer* GetCurrentRenderCmd() { return current_render_cmd; }
            void SetCurrentRenderCmd(hgl::graph::RenderCmdBuffer* cmd) { current_render_cmd = cmd; }

            /// Graphics context adapter (Phase 2)
            void SetGraphicsContext(hgl::graph::GraphicsContext* ctx) { graphics_context = ctx; }
            hgl::graph::GraphicsContext* GetGraphicsContext() { return graphics_context; }
            const hgl::graph::GraphicsContext* GetGraphicsContext() const { return graphics_context; }

            /// Render context adapter (Phase 2)
            void SetRenderContext(hgl::graph::RenderContext* ctx) { render_context = ctx; }
            hgl::graph::RenderContext* GetRenderContext() { return render_context; }
            const hgl::graph::RenderContext* GetRenderContext() const { return render_context; }

            /// Asset world registry (Phase 3)

            /// Unified render pipeline registry
            /// Get a pipeline by name (e.g., "Primitive", "Text", "Line", "Quad")
            /// @param name: pipeline group name from SystemGroup
            /// @return pointer to RenderPipelineBase, or nullptr if not registered
            RenderPipelineBase* GetRenderPipeline(const std::string& name);

            /// Register a render pipeline (typically called during system group installation)
            /// @param name: pipeline group name
            /// @param pipeline: newly created pipeline instance
            void RegisterRenderPipeline(const std::string& name, std::unique_ptr<RenderPipelineBase> pipeline);

            /// Check if a render pipeline is registered and enabled
            bool IsRenderPipelineEnabled(const std::string& name) const;

            /// Get all registered pipeline names
            std::vector<std::string> GetRenderPipelineNames() const;

            /// Resource naming prefix for hierarchical GPU resource tracking
            /// Example: "RenderToTexture:OffscreenRT:IndirectDrawBuffer"
            void SetResourceNamePrefix(const std::string& prefix) { resource_name_prefix = prefix; }
            const std::string& GetResourceNamePrefix() const { return resource_name_prefix; }

            /// 注册组件实例（由 Entity::AddComponent 调用）
            void RegisterComponentInstance(size_t type_hash, const std::shared_ptr<Component>& comp);

            /// 反注册组件实例（由 Entity::RemoveComponent 调用）
            void UnregisterComponentInstance(size_t type_hash, Component* comp_ptr);

            /// Notify all systems that an entity gained a component (for cache invalidation)
            void NotifyComponentAdded(EntityID entity_id, const std::type_index& component_type);

            /// Notify all systems that an entity lost a component (for cache invalidation)
            void NotifyComponentRemoved(EntityID entity_id, const std::type_index& component_type);

            /// Notify all systems that an entity is being destroyed
            /// Used to remove residual query cache entries (e.g. manual participation)
            void NotifyEntityDestroyed(EntityID entity_id);

            /// Register a transform component (called by TransformComponent::OnAttach)
            void RegisterTransformComponent(const std::shared_ptr<TransformComponent>& comp, bool isMovable);

            /// Migrate transform between static and movable lists
            void MigrateTransformComponent(TransformComponent* comp_ptr, bool toMovable);

            /// Unregister a transform component (called by TransformComponent::OnDetach)
            void UnregisterTransformComponent(TransformComponent* comp_ptr);

            /// Get static transforms for offline baking
            const std::vector<std::weak_ptr<TransformComponent>>& GetStaticTransforms() const { return static_transforms; }

            /// Get movable transforms for runtime updates
            const std::vector<std::weak_ptr<TransformComponent>>& GetMovableTransforms() const { return movable_transforms; }

        public:

            /// Create a new entity in the world
            /// Returns raw pointer managed by EntityManager
            template<typename T = Entity, typename... Args>
            T* CreateEntity(Args&&... args)
            {
                static_assert(std::is_base_of_v<Entity, T>, "T must derive from hgl::ecs::Entity");

                if (!entity_manager)
                    return nullptr;

                auto instance = std::make_unique<T>(std::forward<Args>(args)...);
                EntityID id = entity_manager->CreateEntity(std::move(instance));
                Entity* entity = entity_manager->GetEntity(id);
                if (!entity)
                    return nullptr;

                entity->SetContext(this);
                entity->OnCreate();
                return static_cast<T*>(entity);
            }

            /// Descriptor for creating a child entity with a TransformComponent.
            struct ChildEntityDesc
            {
                const char *name     = nullptr;
                glm::vec3   position = glm::vec3(0.0f);
                glm::quat   rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                glm::vec3   scale    = glm::vec3(1.0f);
                Mobility    mobility = Mobility::Movable;
            };

            /// Create a named child entity with a TransformComponent already set up.
            /// Appends the new EntityID to out_entity_ids when provided.
            /// Returns the TransformComponent via out_transform when provided.
            Entity* CreateChildEntity(Entity *parent,
                                      const ChildEntityDesc &desc,
                                      std::vector<EntityID> *out_entity_ids = nullptr,
                                      std::shared_ptr<TransformComponent> *out_transform = nullptr);

            /// Get entity by ID
            Entity* GetEntity(EntityID id)
            {
                if (!entity_manager)
                    return nullptr;
                return entity_manager->GetEntity(id);
            }

            /// Get entity by ID (const version)
            const Entity* GetEntity(EntityID id) const
            {
                if (!entity_manager)
                    return nullptr;
                return entity_manager->GetEntity(id);
            }

            /// Destroy entity by ID
            void DestroyEntity(EntityID id)
            {
                if (entity_manager)
                    entity_manager->DestroyEntity(id);
            }

            /// Get all alive entity IDs
            void GetAllEntityIDs(std::vector<EntityID>& out_ids) const
            {
                if (entity_manager)
                    entity_manager->GetAllEntities(out_ids);
            }

            /// Get all alive entity pointers
            void GetAllEntities(std::vector<Entity*>& out_entities)
            {
                if (entity_manager)
                    entity_manager->GetAllEntityPointers(out_entities);
            }

            /// Get all alive entity pointers (const version)
            void GetAllEntities(std::vector<const Entity*>& out_entities) const
            {
                if (entity_manager)
                    entity_manager->GetAllEntityPointers(out_entities);
            }

            bool IsEntityTickEnabled(const Entity* entity) const;
            bool IsEntityRenderEnabled(const Entity* entity) const;
            uint32_t GetFilteredEntityCountLastFrame() const { return filtered_entity_count_last_frame; }

            /// Ensure CameraSystem exists in this context.
            /// If created after context activation, it is initialized immediately.
            std::shared_ptr<CameraSystem> EnsureCameraSystem();

            /// Register a system
            /// @param is_render true则放入渲染系统列表，否则为逻辑更新系统
            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterSystem(bool is_render, Args&&... args)
            {
                auto system = std::make_shared<T>(std::forward<Args>(args)...);
                const size_t key = typeid(T).hash_code();
                AddOrUpdateSystem(is_render, key, system);
                return system;
            }

            /// Register a tick (logic) system
            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterTickSystem(Args&&... args)
            {
                return RegisterTickSystemScoped<T>(SystemOwnershipScope::Auto, std::forward<Args>(args)...);
            }

            /// Register a render system
            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterRenderSystem(Args&&... args)
            {
                return RegisterRenderSystemScoped<T>(SystemOwnershipScope::Auto, std::forward<Args>(args)...);
            }

            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterTickSystemScoped(SystemOwnershipScope scope, Args&&... args)
            {
                if (!CanRegisterGameplaySystemInThisContext())
                {
                #if ULRE_ECS_DEBUG_API
                    LogWarning("[ECS] Tick system registration rejected by context gate. context='%s' system_type='%s'",
                               GetName().c_str(),
                               typeid(T).name());
                #endif
                    return nullptr;
                }

                if (scope == SystemOwnershipScope::GlobalShared && context_role == ContextRole::LocalSubWorld)
                {
                #if ULRE_ECS_DEBUG_API
                    LogWarning("[ECS] Tick system registration rejected by scope. context='%s' scope=GlobalShared system_type='%s'",
                               GetName().c_str(),
                               typeid(T).name());
                #endif
                    return nullptr;
                }

                if (scope == SystemOwnershipScope::LocalIsolated && context_role == ContextRole::RootShared)
                {
                #if ULRE_ECS_DEBUG_API
                    LogWarning("[ECS] Tick system registration rejected by scope. context='%s' scope=LocalIsolated system_type='%s'",
                               GetName().c_str(),
                               typeid(T).name());
                #endif
                    return nullptr;
                }

                return RegisterSystem<T>(false, std::forward<Args>(args)...);
            }

            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterRenderSystemScoped(SystemOwnershipScope scope, Args&&... args)
            {
                if (scope == SystemOwnershipScope::GlobalShared && context_role == ContextRole::LocalSubWorld)
                {
                    ++rejected_render_system_registration_count;

                #if ULRE_ECS_DEBUG_API
                    if (!rejected_render_system_registration_logged)
                    {
                        rejected_render_system_registration_logged = true;
                        LogWarning("[ECS] Render system registration rejected by scope. context='%s' scope=GlobalShared system_type='%s'",
                                   GetName().c_str(),
                                   typeid(T).name());
                    }
                #endif
                    return nullptr;
                }

                if (scope == SystemOwnershipScope::LocalIsolated && context_role == ContextRole::RootShared)
                {
                    ++rejected_render_system_registration_count;

                #if ULRE_ECS_DEBUG_API
                    if (!rejected_render_system_registration_logged)
                    {
                        rejected_render_system_registration_logged = true;
                        LogWarning("[ECS] Render system registration rejected by scope. context='%s' scope=LocalIsolated system_type='%s'",
                                   GetName().c_str(),
                                   typeid(T).name());
                    }
                #endif
                    return nullptr;
                }

                if (!CanRegisterRenderSystemInThisContext())
                {
                    ++rejected_render_system_registration_count;

                #if ULRE_ECS_DEBUG_API
                    if (!rejected_render_system_registration_logged)
                    {
                        rejected_render_system_registration_logged = true;
                        LogWarning("[ECS] Render system registration rejected before instantiate. context='%s' system_type='%s'",
                                   GetName().c_str(),
                                   typeid(T).name());
                    }
                #endif
                    return nullptr;
                }

                return RegisterSystem<T>(true, std::forward<Args>(args)...);
            }

            template<typename T>
            bool SetSystemEnabled(bool enabled)
            {
                return SetSystemEnabledByKey(typeid(T).hash_code(), enabled);
            }

            template<typename T>
            bool RemoveSystem()
            {
                return RemoveSystemByKey(typeid(T).hash_code());
            }

             /// Get a system by type
             template<typename T>
             std::shared_ptr<T> GetSystem() const
             {
                const size_t key = typeid(T).hash_code();

                if (auto *system = tick_systems.GetValuePointer(key))
                    return std::static_pointer_cast<T>(*system);

                if (auto *system = render_systems.GetValuePointer(key))
                    return std::static_pointer_cast<T>(*system);
                 return nullptr;
             }

            /// Get systems by render element type name (e.g., "Primitive", "Text", "SkySphere")
            void GetSystemsByElementType(const std::string& element_type, std::vector<std::shared_ptr<System>>& out_systems) const;

            /// Get all registered render element type names
            void GetAllRenderElementTypes(std::vector<std::string>& out_element_types) const;

            /// Set enabled state for all systems of a given render element type
            void SetElementTypeSystemsEnabled(const std::string& element_type, bool enabled);

            /// Get tracked component count for a system group
            uint32_t GetSystemGroupComponentCount(const std::string& group_name) const;

            /// Check whether the system group installer has already run for this context
            bool IsSystemGroupInstalled(const std::string& group_name) const;

            /// Mark a system group as installed in this context (idempotent)
            void MarkSystemGroupInstalled(const std::string& group_name);

            /// Get all known system group names (seen from component registration)
            void GetKnownSystemGroups(std::vector<std::string>& out_group_names) const;

            /// Disable all groups whose tracked component count is zero (systems stay resident)
            void DisableUnusedSystemGroups();

            /// Disable a specific system group (systems stay resident)
            bool DisableSystemGroup(const std::string& group_name);

            /// Cleanup a specific system group. When remove_systems=true, unregister the group's systems.
            bool CleanupSystemGroup(const std::string& group_name, bool remove_systems = false);

            /// Cleanup all unused groups (component count == 0). Returns cleaned group count.
            size_t CleanupUnusedSystemGroups(bool remove_systems = false);

        public:
            /// Get entity count
            size_t GetEntityCount() const
            {
                return entity_manager ? entity_manager->GetEntityCount() : 0;
            }

            RenderFrameCache& GetRenderFrameCache() { return render_frame_cache; }
            const RenderFrameCache& GetRenderFrameCache() const { return render_frame_cache; }

            /// Check if world is active
            bool IsActive() const { return active; }

            /// Gate for local render-system registration. Used by hybrid SubWorld policy.
            void SetRenderSystemRegistrationAllowed(bool value) { allow_render_system_registration = value; }
            bool IsRenderSystemRegistrationAllowed() const { return allow_render_system_registration; }
            bool CanRegisterRenderSystemInThisContext() const { return allow_render_system_registration; }
            bool CanRegisterGameplaySystemInThisContext() const { return true; }

            /// Context role used by scoped ownership registration checks.
            void SetContextRole(ContextRole value) { context_role = value; }
            ContextRole GetContextRole() const { return context_role; }
            bool IsLocalSubWorldContext() const { return context_role == ContextRole::LocalSubWorld; }

            /// Diagnostics: count how many render-system registration attempts were rejected.
            uint32_t GetRejectedRenderSystemRegistrationCount() const { return rejected_render_system_registration_count; }
            uint32_t GetGlobalRenderSystemCount() const { return global_render_system_count; }
            uint32_t GetLocalGameplaySystemCount() const { return local_gameplay_system_count; }
            void ResetRejectedRenderSystemRegistrationCount()
            {
                rejected_render_system_registration_count = 0;
                rejected_render_system_registration_logged = false;
            }
            void ResetHybridRegistrationDiagnostics()
            {
                ResetRejectedRenderSystemRegistrationCount();
                global_render_system_count = 0;
                local_gameplay_system_count = 0;
            }

            /// 获取指定类型的组件列表（自动清理已失效的弱引用）
            template<typename T>
            void GetComponents(std::vector<std::shared_ptr<T>>& out) const
            {
                out.clear();
                const size_t key = typeid(T).hash_code();
                auto *list = component_registry.GetValuePointer(key);
                if (!list)
                    return;

                for(auto &weak_comp : *list)
                {
                    if(auto comp = weak_comp.lock())
                    {
                        out.push_back(std::static_pointer_cast<T>(comp));
                    }
                }
            }

            /// Register a base type for component queries (supports derived components automatically).
            template<typename T>
            void RegisterComponentQueryBase()
            {
                const size_t key = typeid(T).hash_code();

                for (const auto& entry : component_query_bases)
                {
                    if (entry.key == key)
                        return;
                }

                ComponentQueryBase entry;
                entry.key = key;
                entry.matches = [](Component* comp) { return dynamic_cast<T*>(comp) != nullptr; };
                component_query_bases.push_back(entry);

                if (!component_registry.GetValuePointer(key))
                {
                    component_registry.Add(key, std::vector<std::weak_ptr<Component>>{});
                }

                // Backfill existing components into the new base list.
                for (const auto& pair : component_registry)
                {
                    for (const auto& weak_comp : pair.second)
                    {
                        if (auto comp = weak_comp.lock())
                        {
                            if (entry.matches(comp.get()))
                                RegisterComponentInstanceInternal(key, comp);
                        }
                    }
                }
            }
        };
    }//namespace ecs
}//namespace hgl
