#pragma once

#include<hgl/vk/VK.h>
#include<hgl/ecs/core/Object.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/System.h>
#include<hgl/ecs/core/RenderGraph.h>
#include<hgl/ecs/core/RenderPassRequest.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/ecs/core/ScenePipelineMode.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/core/EntityManager.h>
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
#include<glm/glm.hpp>

namespace hgl {
    namespace graph {
        class RenderCmdBuffer;
        class CameraInfo;
        class IRenderTarget;
        class GraphicsContext;  // 图形资源管理器（原IGraphicsContext）
        class VulkanDevice;
        class RenderContext;
        struct RenderPassOptions;
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
        class RenderItemDataStorage;
        class DrawItemIDStorage;

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

            // TransformComponent 分离列表与 World 变换存储
            std::vector<std::weak_ptr<TransformComponent>> static_transforms;
            std::vector<std::weak_ptr<TransformComponent>> movable_transforms;
            std::unique_ptr<TransformDataStorage> transform_storage;
            std::unique_ptr<RenderItemDataStorage> render_item_storage;
            std::unique_ptr<DrawItemIDStorage> draw_item_id_storage;

            bool active = false;
            bool shutdown_in_progress = false;

            RenderFrameCache render_frame_cache;
            uint32_t frame_index = 0;
            uint64_t render_submission_serial = 0;
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

            /// 当前渲染 Pass 的活跃相机 SSBO 行号（用于 PushConstants 索引多相机）
            uint32_t active_camera_id = 0;

            // ---- A1 车道等待列表（per-frame，见 RenderOptions.h 的车道说明）----

            static constexpr uint32_t MAX_LANE_WAITS = 16;

            graph::SemaphoreSubmit frame_lane_waits[MAX_LANE_WAITS];   ///<本帧已提交的各离屏 RT 车道值
            uint32_t frame_lane_wait_count = 0;

            graph::SemaphoreSubmit submit_waits[MAX_LANE_WAITS];       ///<下一次提交的额外等待列表
            uint32_t submit_wait_count = 0;

            /// 当前渲染 Pass 的物体移动性过滤（-1 = 全部，0 = 仅静态 Static，1 = 仅动态 Movable）
            int active_mobility_filter = -1;

            /// 静态场景 revision：任何 Static transform 被检出变更时递增（A3）。
            /// 消费方（如 EnvironmentSystem 的静态级联阴影缓存）记录上次消费值，
            /// 与当前值不等即知"静态场景已变"，做对应的缓存失效。只增不减。
            uint64_t static_scene_revision = 0;

            /// 当前 Pass 是否为阴影贴图生成 Pass
            bool is_current_pass_shadow = false;
            glm::vec3 current_pass_shadow_origin{0.0f};
            bool has_shadow_origin = false;

            /// 场景渲染工作流模式（默认 StandardLitCSM：标准 3D 陆地主光级联阴影）
            ScenePipelineMode scene_pipeline_mode = ScenePipelineMode::StandardLitCSM;

            std::unique_ptr<RenderSystemCore> render_core;

            /// Cached adaptive render graph (auto-culls based on scene content)
            mutable RenderGraph cached_adaptive_render_graph;
            mutable uint64_t cached_adaptive_scene_hash = ~0ULL;  // sentinel: "not computed yet"
            mutable bool scene_structure_dirty = true;  // 场景结构（entity/component 增删）变化标记，下次 Render 重 gather

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
            void RunRenderPhaseUpdates(ExecutionPhase phase, float deltaTime);
            void RunRenderPhaseUpdates(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime);
            void RunRenderSystemsInRange(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime);
            void RunSystemUpdate(System *system, float deltaTime);
            void RegisterComponentInstanceInternal(size_t type_hash, const std::shared_ptr<Component>& comp);
            bool EnsureRenderCoreInitialized();
            void ExecuteScenePrePassWorkflow(float deltaTime);
            bool BeginManagedRenderFrame(float deltaTime, bool need_swapchain_acquire = true, const graph::RenderPassOptions *options = nullptr);
            void EndManagedRenderFrame(float deltaTime);
            void RecordPreparedRenderPhaseRange(ExecutionPhase minPhase,
                                               ExecutionPhase maxPhase,
                                               float deltaTime,
                                               bool submit_transforms,
                                               const char *log_prefix);
            void ExecuteRenderGraphPasses(const RenderGraph& graph,
                                          float deltaTime,
                                          const std::function<void(float)> &pre_render);

            // ========== 帧驱动内部编排（应用层勿直接调用） ==========
            // 公共入口只有 Render(dt, pre_render) 与 RenderTo(rt, clear, dt)；
            // 以下相位方法仅由上述两者与托管帧流程按 ExecutionPhase 顺序编排。

            void RenderDrawOnly(graph::RenderCmdBuffer *cmd, float deltaTime);
            void Render(float deltaTime, const RenderGraph& graph, const std::function<void(float)> &pre_render);

            /// Run pre-begin-frame render updates (no command buffer).
            void RenderPreBeginFrame(float deltaTime);

            void RenderSwapchainNextImage(float deltaTime);
            bool AcquireSwapchainImage(float deltaTime = 0.0f);
            void SyncRenderTargetViewport();
            void RenderBufferCommit(float deltaTime);
            void RenderBufferUpload(float deltaTime);
            void RenderFrameSync(float deltaTime);

            /// Orchestrate pre-pass render setup phases for a specific frame index.
            /// Strict order: BeginFrame → Collect → Batch → BufferCommit → BufferUpload → FrameSync
            void PrepareRenderPassSetup(uint32_t frameIndex, float deltaTime = 0.0f);

            void RenderSubmit(float deltaTime);
            bool SubmitFrameToRenderTarget(float deltaTime = 0.0f);
            bool RecreateSwapchainIfNeeded();

        public:

            /// 初始化世界（W3 合并：GPU 设备/渲染目标绑定 + 系统注册与初始化，
            /// 顺序敏感消除——旧 InitializeGraphics/Initialize 两步调用的兼容残留）
            /// @param device GPU 设备
            /// @param target 渲染目标
            /// @return 成功返回 true
            bool Initialize(hgl::graph::VulkanDevice* device, hgl::graph::IRenderTarget* target);

            /// Shut down the world
            void Shutdown();

            /// Tick all non-render systems and entities
            void Tick(float deltaTime);

            /// 把本世界的一帧渲染到指定 RenderTarget（离屏/子 pass 的标准入口，
            /// RT 标准化 §3.4 的一等 pass 描述）
            ///
            /// 内部复用 BeginManagedRenderFrame + RenderDrawOnly + EndManagedRenderFrame，
            /// 与主窗口路径**共用同一套帧驱动**，不新增平行实现。
            /// request.camera 非空时本 pass 用它解算共享相机数据（pass 级
            /// 相机覆盖，shadow map 等多相机场景的标准做法）。
            ///
            /// @return 成功返回 true
            /// @note 会跳过 swapchain 图像获取（离屏 RT 无 swapchain 图像）
            bool RenderTo(const RenderPassRequest &req);

            /// 便捷重载：显式清屏色（等价于 request{target=rt, clear, use_target_clear=false}）
            bool RenderTo(graph::IRenderTarget *rt, const hgl::Color4f &clear, float deltaTime = 0.0f, CullMode cull_mode = CullMode::Inherit);

            /// Run a full render frame with a pre-render callback
            /// （主窗口帧驱动唯一公共入口，由 WorkManager 调用）
            void Render(float deltaTime, const std::function<void(float)> &pre_render);

            /// Mark scene structure (entity/component add/remove) as changed.
            /// Called automatically by component attach/detach; next Render re-gathers
            /// stats only when dirty, avoiding a per-frame full entity scan.
            void MarkSceneStructureDirty() const { scene_structure_dirty = true; }

            /// Handle render target resize
            void OnResize(const VkExtent2D &extent);

            void SetFrameIndex(const uint32_t index);
            uint32_t GetFrameIndex() const { return frame_index; }

            int GetActiveMobilityFilter() const { return active_mobility_filter; }

            /// Static transform 变更检出时递增（由 TransformSystem 调用；A3 静态缓存失效链）
            void BumpStaticSceneRevision() { ++static_scene_revision; }
            uint64_t GetStaticSceneRevision() const { return static_scene_revision; }

            bool IsCurrentPassShadow() const { return is_current_pass_shadow; }
            bool HasShadowOrigin() const { return has_shadow_origin; }
            const glm::vec3 &GetShadowOrigin() const { return current_pass_shadow_origin; }

            void SetScenePipelineMode(ScenePipelineMode mode) { scene_pipeline_mode = mode; }
            ScenePipelineMode GetScenePipelineMode() const { return scene_pipeline_mode; }

            void SetRenderSubmissionSerial(const uint64_t serial)
            {
                render_submission_serial = serial;
            }
            uint64_t GetRenderSubmissionSerial() const
            {
                return render_submission_serial;
            }

            // ========== GPU 设备和资源接口（Phase 1 新增） ==========

            /// 获取 GPU 设备
            hgl::graph::VulkanDevice* GetGPUDevice() { return gpu_device; }

            /// 设置渲染目标（用于窗口 resize 等场景重建 render target）
            void SetRenderTarget(hgl::graph::IRenderTarget* target) { render_target = target; }

            /// [唯一权威 getter] 获取本世界绑定的渲染目标。
            ///
            /// 应用代码取 RT 一律走此入口。其余入口
            /// （RenderContext::GetCurrentRenderTarget / AppFramework::GetSwapchainRenderTarget /
            ///  SwapchainModule::GetRenderTarget / RenderTargetSystem::GetRenderTarget）
            /// 均为框架内部接线或系统内缓存，不应在应用层直接依赖。
            hgl::graph::IRenderTarget* GetRenderTarget() { return render_target; }

            /// 获取当前渲染命令缓冲区（仅在 Render() 执行期间有效）
            hgl::graph::RenderCmdBuffer* GetCurrentRenderCmd() { return current_render_cmd; }
            void SetCurrentRenderCmd(hgl::graph::RenderCmdBuffer* cmd) { current_render_cmd = cmd; }

            /// 获取与设置当前活跃相机 SSBO 行号（用于 PushConstants 索引多相机）
            uint32_t GetActiveCameraID() const { return active_camera_id; }
            void SetActiveCameraID(uint32_t id) { active_camera_id = id; }

            // ---- A1 车道等待列表 ----

            /// 设定下一次提交的等待列表（RenderTo / 主帧收尾用）
            void SetSubmitWaits(const graph::SemaphoreSubmit *waits,uint32_t count);
            /// 累积本帧离屏 RT 的车道值（主帧提交 await 它们）
            void AddFrameLaneWait(graph::Semaphore *lane,uint64_t value);
            /// 把当前累积复制为下一次提交的等待列表
            void SetSubmitWaitsFromFrameLanes();
            /// 提交系统取用等待列表；is_main_frame=true 时同时收尾清空本帧累积
            const graph::SemaphoreSubmit *TakeSubmitWaits(uint32_t &count,bool is_main_frame);

            /// Graphics context adapter (Phase 2)
            void SetGraphicsContext(hgl::graph::GraphicsContext* ctx) { graphics_context = ctx; }
            // W7：封装 render_ctx fallback（实现见 Context.cpp——RenderContext
            // 前向声明，内联体无法调用成员函数）——此前各系统复制三段样板
            hgl::graph::GraphicsContext* GetGraphicsContext();
            const hgl::graph::GraphicsContext* GetGraphicsContext() const;

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

            /// Resource naming prefix for hierarchical GPU resource tracking
            /// Example: "RenderToTexture:OffscreenRT:IndirectDrawBuffer"
            void SetResourceNamePrefix(const std::string& prefix) { resource_name_prefix = prefix; }
            const std::string& GetResourceNamePrefix() const { return resource_name_prefix; }

            /// 注册组件实例（由 Entity::AddComponent 调用）
            void RegisterComponentInstance(size_t type_hash, const std::shared_ptr<Component>& comp);

            /// 反注册组件实例（由 Entity::RemoveComponent 调用）
            void UnregisterComponentInstance(size_t type_hash, Component* comp_ptr);

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

            /// Get world-level TransformDataStorage
            TransformDataStorage* GetTransformStorage() { return transform_storage.get(); }
            const TransformDataStorage* GetTransformStorage() const { return transform_storage.get(); }

            /// Get world-level RenderItemDataStorage
            RenderItemDataStorage* GetRenderItemStorage() { return render_item_storage.get(); }
            const RenderItemDataStorage* GetRenderItemStorage() const { return render_item_storage.get(); }

            /// Get world-level DrawItemIDStorage
            DrawItemIDStorage* GetDrawItemIDStorage() { return draw_item_id_storage.get(); }
            const DrawItemIDStorage* GetDrawItemIDStorage() const { return draw_item_id_storage.get(); }

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
                return static_cast<T*>(entity);
            }

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

            /// Get all alive entity pointers (const version)
            void GetAllEntities(std::vector<const Entity*>& out_entities) const
            {
                if (entity_manager)
                    entity_manager->GetAllEntityPointers(out_entities);
            }

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
                return RegisterSystem<T>(false, std::forward<Args>(args)...);
            }

            /// Register a render system
            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterRenderSystem(Args&&... args)
            {
                return RegisterSystem<T>(true, std::forward<Args>(args)...);
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

            /// Check whether the system group installer has already run for this context
            bool IsSystemGroupInstalled(const std::string& group_name) const;

            /// Mark a system group as installed in this context (idempotent)
            void MarkSystemGroupInstalled(const std::string& group_name);

        public:
            RenderFrameCache& GetRenderFrameCache() { return render_frame_cache; }
            const RenderFrameCache& GetRenderFrameCache() const { return render_frame_cache; }

            /// Check if world is active
            bool IsActive() const { return active; }

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
