#pragma once

#include<hgl/vk/VK.h>
#include<hgl/ecs/core/Object.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/System.h>
#include<hgl/ecs/core/RenderGraph.h>
#include<hgl/ecs/core/RenderFrameCache.h>
#include<hgl/ecs/core/RenderPassRequest.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/ecs/core/ScenePipelineMode.h>
#include<hgl/ecs/support/ComponentTypeTable.h>
#include<hgl/ecs/support/ComponentPool.h>
#include<hgl/ecs/support/EntityGPUTable.h>
#include<hgl/ecs/support/EntityRenderIDTable.h>
#include<hgl/ecs/support/LightRowTable.h>
#include<hgl/graph/ubo/ShadowInfo.h>
#include<hgl/type/ValueArray.h>
#include<hgl/ecs/support/TransformID.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/support/BoundingBoxAccessor.h>
#include<hgl/ecs/support/VisibilityDataStorage.h>
#include<hgl/ecs/support/MaterialVariantTable.h>
#include<hgl/ecs/support/MaterialRuntimeTable.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/core/EntityManager.h>
#include<hgl/graph/ubo/EnvironmentInfo.h>
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
        class DeviceBuffer;
        class EnvironmentManager;
        template<typename T> class StructView;
        struct WorldAddresses;
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
        class CameraInfoStorage;
        class MaterialData;
        class GeometryData;
        class ShadowProxy;
        class MaterialAuthoringSideTable;
        class MaterialRuntimePipelineTable;

        class ECSContext;
        void ReleaseRenderItemHandle(ECSContext &world, EntityID owner);

        /**
         * ECSContext manages all entities and systems
         * Acts as the main container for the ECS simulation
         */
        class ECSContext : public Object
        {
        public:

            ECSContext(const char* name) : ECSContext(AnsiString(name)) {}
            ECSContext(const AnsiString& name = "World");
            ECSContext(const std::string& name) : ECSContext(AnsiString(name.c_str())) {}
            ~ECSContext() override;

        private:
            OBJECT_LOGGER

            // ── T3b：实体行表（`EntityGPU` 的 GPU 派生视图；世界私有）──
            //   **必须声明在 `entity_manager` 之前**：成员按声明逆序析构 ⇒ 表比实体活得久
            //   （`~Entity` 会回写本表的 `Active` 位；表先死就会写到已释放内存）。
            EntityGPUTable entity_gpu_table;

            /// T3d-3a：实体 4-ID 表（世界私有；与 `entity_gpu_table` 同批声明 ⇒ 同一条析构逆序纪律，
            /// `~Entity` 经 `DeactivateEntityRow` 同时清两表，两表都必须比 `entity_manager` 活得久）
            EntityRenderIDTable entity_render_id_table;

            /// T6b：光源行表（`LightRow`，世界私有；**行 0 = 主方向光**）。
            /// 与上两表同域（世界的 GPU 行数据），但没有 `~Entity` 回写者 ⇒ 声明位置不涉及析构逆序。
            LightRowTable light_row_table;

            /// T6b/P4：影子数据（`graph::ShadowInfo`）的 **CPU 权威**（世界私有）。
            /// 以前它挂在 env profile（`EnvironmentInfo::shadow`）而 CSM 的 RT/控制器挂世界 ⇒
            /// 两个世界共享 default profile 时互相覆写；现在随世界。写者 = `MainLightShadowSystem`
            /// （经 `EditShadowInfo`），设备侧由 `CommitShadowInfo` 在提交点写本帧槽。
            graph::ShadowInfo shadow_info;

            std::unique_ptr<EntityManager> entity_manager;

            // 分类存储：更新系统与渲染系统分开
            hgl::UnorderedMap<size_t, std::shared_ptr<System>> tick_systems;
            hgl::UnorderedMap<size_t, std::shared_ptr<System>> render_systems;

            // 按 render element type 存储系统（**一次性注册路径**仍按名键控：组定义/安装器/管线）
            std::map<std::string, std::vector<std::shared_ptr<System>>> systems_by_element_type;

            // Component-driven system-group activity tracking（A2：枚举 + O(1) 数组，热路径无字符串）
            uint32_t system_group_component_counts[kSystemGroupCount] = {};
            bool     installed_system_groups[kSystemGroupCount] = {};

            /// 每组的系统快照（供启用/禁用走 O(1) 数组；系统注册时置空、下次启用重建）
            std::vector<std::shared_ptr<System>> group_systems[kSystemGroupCount];

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

            // ── T2-c：组件存储 = 世界私有的**类型化对象池**（按 ComponentType 索引）──
            //
            // 池**惰性创建**（首次挂载该类型时经 `EnsureTypedPool<T>()`，形态由
            // `ComponentStorageTraits<T>` 决定：裸行池 / 行内持 shared_ptr 的共享行池）；
            // `component_pools` 只是持有者，一切查询走 `component_pool_slots[type]`。
            //
            // 「池即活跃集」：`GetComponents<T>` 直接读池的活跃集视图（裸指针）。旧形态是
            // 「按类型 hash 的 weak_ptr 注册表」——每挂一个组件 push 一个 weak_ptr、每类型一次向量增长，
            // 而且它把 `shared_ptr<Component>` 钉在组件上（T2-c 拆掉的两样东西之一）。
            std::vector<std::unique_ptr<ComponentPoolBase>> component_pools;
            ComponentPoolBase *component_pool_slots[COMPONENT_TYPE_COUNT] = {};

            // 变换（T8：按 TransformID 记录，不再持组件指针；组件只是过渡期外壳）
            //
            // Stage B / B1b：这两个数组是**槽位表** —— 下标 = 稳定槽位，L2W 行号由它算出
            // （`静态: 1 + slot` / `动态: 1 + 预算static + frame × 预算dynamic + slot`），
            // **不再随名单顺序漂移**；空洞记 `INVALID_TRANSFORM_ID`，注册优先复用空洞。
            // 段容量 = 变换存储声明的预算（超限 = 一次性明确告警 + 拒绝，不扩容）。
            std::vector<TransformID> static_transforms;
            std::vector<TransformID> movable_transforms;
            std::vector<uint32_t>    static_free_slots;     ///< 空闲静态槽（后进先出；复用即得回稳定行号）
            std::vector<uint32_t>    movable_free_slots;    ///< 空闲动态槽
            bool                     transform_slot_overflow_warned = false;    ///< 段预算用尽：一次性告警闩
            bool                     transform_row_move_warned      = false;    ///< Mobility 换边（行号移动）：一次性告警闩
            uint32_t                 transform_row_move_count       = 0;        ///< 换边次数（可观测）
            /// 变换行存储（本世界独占拥有）。存储侧另持一条**指向本世界的非拥有反向指针**
            /// （构造期 `SetContext(this)` 回填）⇒ 变换行句柄（TransformAccessor）无需自带 context。
            std::unique_ptr<TransformDataStorage> transform_storage;

            // 世界私有包围盒行存储（原组件已删除；连续 SoA + 值类型句柄，与 transform_storage 同范式）。
            // 声明在 transform_storage 之后 ⇒ 先于它析构；两者互不引用，无"析构时用已销毁对象"问题。
            std::unique_ptr<BoundingBoxDataStorage> bounding_box_storage;

            // 世界私有可见性数据（A5a 起：可见性收敛为实体级唯一真值，组件级副本已删除）。
            // 可见性是 **CPU 域**状态（不在 v2 §3 的 5 类 GPU 可见组件里）⇒ 无需行 arena / SSBO，
            // 只保留"不可见实体集合"这一份真值；查询时按变换父链上溯（祖先不可见 ⇒ 后代不可见）。
            std::unique_ptr<VisibilityDataStorage> visibility_storage;

            std::unique_ptr<RenderItemDataStorage> render_item_storage;
            std::unique_ptr<DrawItemIDStorage> draw_item_id_storage;

            // A3 材质变体表（v2 §9.3）：按**静态键**去重的 program/recipe 运行期记录。
            // program 归本世界 GraphicsContext 的 ShaderProgramManager 缓存所有 ⇒ 表随世界
            // 存亡，不会跨 GraphicsContext 混用悬垂 program。访问范式同 visibility_storage /
            // draw_item_id_storage。
            std::unique_ptr<MaterialVariantTable> material_variant_table;

            // A4 材质运行期表（v2 §9.3）：**共享行**（同授权态多实体 interned 到同一行：
            // 变体 ID / 材质 SSBO 行与地址 / 纹理引用行与 hash / 已解析配方缓存）
            // + **每实例小记录 slot**（当前 pass/LOD/dither 选择器、D9 重试与降频计数、
            // 每实体授权代跟踪副本、脏标志）。行归零经 refcount 回收，回收时退休该行的
            // 纹理配置池行（GPU 绑定随行存亡）。访问范式同 visibility_storage /
            // material_variant_table。原材质运行期组件已在本步一并删除，无旧读法残留。
            std::unique_ptr<MaterialRuntimeTable> material_runtime_table;

            /// B4-iii-b：材质**重类型授权数据**旁表（配方覆盖 / 具名纹理资源），
            /// 按材质组件行行号索引；设备级行里只留 POD（授权位 + revision）。
            std::unique_ptr<MaterialAuthoringSideTable> material_authoring_side_table;


            /// B5-b：每实例 × 每 pass 的**运行期管线缓存**（slot 里不放指针 ⇒ 落这里；
            /// 见 support/MaterialRuntimePipelineTable.h）。
            std::unique_ptr<MaterialRuntimePipelineTable> material_runtime_pipeline_table;

            // 世界私有**相机表**：相机是**世界级资源**（16 槽 × 帧槽数，0 号槽恒为本世界默认相机）。
            // 行空间 + **槽账目（槽 → 宿主相机）** 都在这里，是"相机属于哪个世界 / 哪个槽"的唯一真源；
            // 定稿见 doc/world-addresses-and-camera-model-plan.md §2（A6 起组件不再自持槽号）。
            /// 相机表（**shared_ptr**：行池的持有者是世界；CameraSlotGuard 只持弱引用 ——
            /// 世界先销毁时它的析构自动变 no-op，不会触碰已释放的行池）
            std::shared_ptr<CameraInfoStorage> camera_info_storage;

            // 世界地址表（WorldAddresses）：世界私有地址的 SSBO 表（HGL_FRAME_SLOT_TOTAL 槽 ×
            // kWorldAddressesSlotStride），基址经 pc_root.addr_world_addresses 下发 ——
            // 多世界渲染只需换这一个指针（相机行表 / 渲染项表 / DrawItemID 表都随世界）。
            graph::StructView<graph::WorldAddresses> *world_addresses_table = nullptr;
            graph::DeviceBuffer *world_addresses_buffer = nullptr;
            uint64_t world_addresses_addr = 0;

            // ── T3c：实体行表（`EntityGPU`）的 **GPU 侧**（世界私有 SSBO）──
            //   CPU 权威 = `entity_gpu_table`（T3b）；这里只是它的设备镜像：
            //   BDA 基址经 `WorldAddresses.addr_entity_gpu` 下发（T3c-2 起按**脏行**上传，
            //   不再整表 512 KB 传）。三者与地址表同范式（`CreateSSBO` → `StructView` → 设备地址）。
            graph::StructView<EntityGPU> *entity_gpu_view = nullptr;
            graph::DeviceBuffer *entity_gpu_buffer = nullptr;
            uint64_t entity_gpu_table_addr = 0;

            // T3d-3a：实体 4-ID 表的设备镜像（骨架与上面三件套逐字同构）
            graph::StructView<graph::RenderItemDescriptor> *entity_render_id_view = nullptr;
            graph::DeviceBuffer *entity_render_id_buffer = nullptr;
            uint64_t entity_render_id_table_addr = 0;

            // T6b：光源行表的设备镜像（骨架与实体两表逐字同构：CreateSSBO → StructView → BDA）。
            // 按帧槽切份（光参数可逐帧改 ⇒ 行内容随帧变）；脏行上传写进每一份。
            graph::StructView<graph::LightRow> *light_row_view = nullptr;
            graph::DeviceBuffer *light_row_buffer = nullptr;
            uint64_t light_row_table_addr = 0;

            // T6b/P4：影子数据的设备镜像（世界私有；**按帧槽各一份** —— 地址 = 基址 + 帧槽 × 832 B）。
            graph::StructView<graph::ShadowInfo> *shadow_info_view = nullptr;
            graph::DeviceBuffer *shadow_info_buffer = nullptr;
            uint64_t shadow_info_addr = 0;

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

            /// 当前渲染 Pass 的活跃相机**槽号**（世界内；用于 pc_root.camera_row 的行号算术）
            uint32_t active_camera_id = CameraComponent::kDefaultSlot;

            /// 本世界的环境 profile（**Env 随世界**，C2）：显式覆盖（`SetEnvProfileID` /
            /// `CreateEnvProfile`）优先；未显式设置时**按需解析本世界 RT 的 env_profile**
            /// （作者侧在 `SetEnvironmentProfile`，可能发生在世界创建之后 ⇒ 不能只在 Initialize 取一次）。
            graph::EnvProfileID env_profile = graph::kEnvProfileDefault;
            bool env_profile_explicit = false;   ///< true ⇒ 用 `env_profile`；false ⇒ 跟随本世界 RT
            bool env_profile_owned = false;      ///< true ⇒ Shutdown 时 `Release()` 归还（本世界创建）

            /// 常驻 fallback 相机（三级解析第 ④ 级的产物）：世界内一个相机组件都没有时，渲染仍必须
            /// 有一个相机。它由世界直接持有，且**不是实体组件**（`CollectCameras()` 看不到它）。
            /// 注意：它**占没占 0 号槽由相机表说了算**（`camera_info_storage` 的槽账目）。
            /// （本世界默认相机 = 0 号槽的宿主，**不再**另存一份 weak_ptr：真值只有一份，相机销毁后
            ///  弱引用自动失效，不会让地址复用者"继承"默认相机身份。）
            std::shared_ptr<CameraComponent> fallback_camera;

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
            /// ⚠ 2026-10-10（审计 §1.1）：**setter 是真 API**（`SceneGraph/module/OffscreenWorld` + 7 个示例在用），
            /// 只读的 `GetResourceNamePrefix()` 才是零调用者（已删）—— 该成员目前「只写不读」，即
            /// 分层泄漏追踪的读取端尚未实现（审计原判）。
            std::string resource_name_prefix;

        private:

            void SortTickSystems();
            void SortRenderSystems();
            void SortSystemList(std::vector<OrderedSystem>& order_list,
                                const DependencyMap& dependencies,
                                bool& dirty_flag,
                                const char* label);
            OrderedSystem* FindOrderedSystem(std::vector<OrderedSystem>& list, size_t key);
            void AddOrUpdateSystem(size_t key, const std::shared_ptr<System>& system);
            /// `is_render` 是**相位推导出的路由位**（不是调用方声明）：决定写 tick 还是 render 依赖图。
            void AddSystemDependency(bool is_render, size_t dependent_key, size_t dependency_key);
            void RunRenderPhaseUpdates(ExecutionPhase phase, float deltaTime);
            void RunRenderPhaseUpdates(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime);
            void RunRenderSystemsInRange(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime);
            void RunSystemUpdate(System *system, float deltaTime);
            /// 释放全部组件行并丢弃池（世界 Shutdown / Clear）
            void ClearComponentPools();

            /// 组件行增/删 ⇒ 系统组计数（A2/P-4 的挂卸驱动路径；组归属取自类型表）
            void OnComponentRowAdded(ComponentType type);
            void OnComponentRowRemoved(ComponentType type);

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
            void SyncRenderTargetViewportToExtent();
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
            /// 顺序敏感消除——W3 起只有这一个初始化入口，不存在两步调用）
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

            /// 创建世界地址表（SSBO：HGL_FRAME_SLOT_TOTAL 槽 × kWorldAddressesSlotStride + 持久 BDA）。
            /// 世界 In,itialize 时调用；失败 = fail-fast（地址缺失会让 shader 解引用 0 = UB）。
            bool InitializeWorldAddressesTable();

            /// T3c-2：把实体行表的**脏行**上传到设备镜像（`SyncWorldAddresses` 内调用）。
            /// `frame_slot` = 本帧槽（T3c-3 起表按帧槽切份，每份一份世界私有 SSBO 区段）。
            void UploadEntityGPUDirtyRows(uint32_t frame_slot);

            /// T3d-3a：实体 4-ID 表脏行上传（与上面同批调用；同一条「值变才标脏」纪律）
            void UploadEntityRenderIDDirtyRows();

            /// **实体行表**（世界私有 SSBO：`EntityGPUTable::kCapacity × 128 B`）建表 + 取设备地址。
            /// 地址缺失 = 消费者解引用 0 基址 = UB ⇒ fail-fast（与地址表同款判据）。
            /// 建表时机：与地址表同批（同一个 BDA 前提 + 同一个"世界 GPU 表"生命周期）。
            bool InitializeEntityGPUTable();

            /// T3d-3a：实体 4-ID 表建表（与上面同批、同判据）
            bool InitializeEntityRenderIDTable();

            /// T6b：光源行表建表（与上面两表同批、同判据：`CreateSSBO` → `StructView` → 设备地址
            /// fail-fast）。行 0 = 主方向光；地址经 `WorldAddresses.addr_lights` 按帧槽下发。
            bool InitializeLightRowTable();

            /// T6b：光源行表脏行上传（与实体两表同批调用；同一条「值变才标脏」纪律）
            void UploadLightRowDirtyRows();

            /// T6b/P4：影子数据建表（与光源行表同批、同判据：`CreateSSBO` → `StructView` → 设备地址
            /// fail-fast）。世界私有 ⇒ 多世界同帧各写各的，不再经 env profile。
            bool InitializeShadowInfoTable();

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

            /// 本 pass 活跃相机的**槽号**（世界内；PushConstants 的 `camera_row` 由它算出行号）。
            /// 只由本世界的 pass 设置路径（`RenderTo`）写 —— 不做公开 setter，避免出现第二个写者。
            // 2026-10-10（审计 §1.1）：零调用者的 `GetActiveCameraID()` 已删除（成员 `active_camera_id` 照旧）。

            /// 当前相机的 CameraInfo 行号 = 槽号 * HGL_FRAME_SLOT_TOTAL + 本帧数据槽。
            /// 着色器端 `pc_root.camera_row` 收到的就是这个行号（scene_ubo.glsl 的 cameras[] 下标），
            /// 因此离屏 pass 与主帧各自的相机数据落在不相交的行上。
            uint32_t GetActiveCameraRow() const;

            /// **本 pass 生效相机**的 CameraInfo（组件自己的那份，没有世界共享载体）。
            /// 离屏 pass 期间 = `RenderTo(req.camera)` 的相机（如 CSM 光相机），主帧 = 本世界主相机。
            /// 剔除 / gizmo / Line 视锥 / shadow origin 统一读这里，不要各自缓存指针。
            const graph::CameraInfo* GetActiveCameraInfo();

            /// 本世界**生效**的 env profile：显式设置优先，否则取本世界 RT 的 env_profile，再退回
            /// 内置 default。世界表里的 `addr_ambient_env` / `addr_shadow` 按它解析。
            graph::EnvProfileID GetEnvProfileID() const;

            /// 选本世界的 env profile；`take_ownership=true` 表示"本世界创建的"（Shutdown 时归还）。
            void SetEnvProfileID(graph::EnvProfileID id, bool take_ownership = false);

            /// 建一个**本世界拥有**的 env profile 并立刻选用（Shutdown 时自动 `Release`）。
            /// 返回 `kEnvProfileInvalid` 表示管理不可用 / 创建失败。
            graph::EnvProfileID CreateEnvProfile(const AnsiString &name,
                                                 const graph::EnvironmentInfo &init = {});

            // ===== 相机（**世界级资源**：16 槽 × 帧槽，0 号槽恒为本世界默认相机）=====
            //
            // 「相机属于哪个世界 / 哪个槽」的**唯一真源 = 本世界的相机表**（`camera_info_storage`：
            // 行空间 + 槽账目「槽 → 宿主相机」）。组件侧**不再**持有槽号 / 归还挂钩；需要槽号一律走
            // 下面这组访问器。三级解析（0 号槽的默认相机 → 显式 `is_main_camera` → 最小 `EntityID`
            // → `(0,0,0)` 常驻 fallback）见 `CameraSystem::SelectMainCamera`，落点即"谁是 0 号槽的宿主"。

            /// 槽的宿主相机（0 号槽 = 本世界默认相机）；槽空闲 / 宿主已销毁 ⇒ nullptr
            CameraComponent* GetCamera(uint32_t slot) const;

            /// 本世界默认相机 = **0 号槽的宿主**；尚未解析出相机 / 相机已销毁时为 nullptr
            CameraComponent* GetDefaultCamera() const;

            /// 让某相机成为本世界默认相机（占 0 号槽）。0 号槽唯一 ⇒ 原宿主自动让位（无需显式释放）。
            bool SetDefaultCamera(const std::shared_ptr<CameraComponent>& camera);

            /// 相机在本世界相机表里的槽号（含 0 号槽）；不属于本世界 / 未认领 ⇒ `kInvalidSlot`
            uint32_t GetCameraSlot(const CameraComponent* camera) const;

            /// **实体 → 挂在它上面的相机**（无则 nullptr）——相机是**世界级资源**，实体只是宿主。
            /// 与 `GetMaterialData` / `GetGeometryData` 同族：**取相机一律走世界**，不走
            /// `Entity::GetComponent<...>`（组件级读法会成为第二个相机读取入口 ⇒ 双真值）。
            /// 与 `GetTransformByEntity` / `GetBoundingBoxByEntity` 同族命名；**不要**写成 `GetCamera(EntityID)`
            /// 重载——与 `GetCamera(uint32_t slot)` 会构成「相似转换」歧义（实测 error C2666）。
            /// 两种句柄类型是**不同的句柄**（不是兼容层）：
            ///   · `EntityID` 版给只持有 ID 的引擎内部代码（内部先解析一次 `GetEntity`）；
            ///   · `Entity*` 版给**手里已有实体**的作者/系统代码 ⇒ **省掉那次多余往返**。
            const CameraComponent* GetCameraByEntity(EntityID owner) const;
            CameraComponent* GetCameraByEntity(EntityID owner);
            const CameraComponent* GetCameraByEntity(const Entity* owner) const;
            CameraComponent* GetCameraByEntity(Entity* owner);

            /// **实体 → 相机**（无则创建；相机组件挂在实体上，实体只是它的宿主）。
            /// 作者侧建相机的**唯一入口**（A7c 起示例不再直接 `AddComponent<CameraComponent>`）：
            /// 与 `GetOrCreateMaterialData` / `GetOrCreateGeometryData` 同风格。
            /// stage B 会把相机改成世界侧行存储 + 值类型句柄，届时本入口随之变形。
            /// `Entity*` 版同上（创建必须能改实体 ⇒ 无 `const Entity*` 版）。
            CameraComponent* GetOrCreateCamera(EntityID owner, const std::string& name = "Camera");
            CameraComponent* GetOrCreateCamera(Entity* owner, const std::string& name = "Camera");

            /// **幂等**认领槽：`is_default=true` ⇒ 占 0 号槽（原持普通槽则先交回；常驻 fallback 自动让位）；
            /// 否则申请 1..15 的一个槽（槽耗尽 / 行池未就绪 ⇒ 报错并保持未认领 ⇒ 发布跳过 + 一次性告警）。
            bool EnsureCameraSlot(CameraComponent* camera, bool is_default = false);

            /// 归还相机占的槽（供拥有者显式归还：系统内建相机随系统状态开关）。
            /// 实体相机**不需要**调用：槽按宿主弱引用计占用期，相机销毁即空出。
            bool ReleaseCameraSlot(const CameraComponent* camera);

            /// 惰性创建**常驻** fallback 相机（位置 `(0,0,0)`、占 0 号槽）——
            /// 三级解析的第 ④ 级：世界内一个相机组件都没有时，渲染仍必须有一个相机。
            /// 创建后一直存在（不按需销毁），且不是 Entity 组件（不是实体组件）。
            CameraComponent* EnsureFallbackCamera();

            bool IsFallbackCamera(const CameraComponent* camera) const
            {
                return camera != nullptr && camera == fallback_camera.get();
            }

            CameraComponent* GetFallbackCamera() const { return fallback_camera.get(); }

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
            RenderPipelineBase* GetRenderPipeline(const char* name);
            RenderPipelineBase* GetRenderPipeline(const std::string& name);
            RenderPipelineBase* GetRenderPipeline(const AnsiString& name);

            /// Register a render pipeline (typically called during system group installation)
            /// @param name: pipeline group name
            /// @param pipeline: newly created pipeline instance
            void RegisterRenderPipeline(const char* name, std::unique_ptr<RenderPipelineBase> pipeline);
            void RegisterRenderPipeline(const std::string& name, std::unique_ptr<RenderPipelineBase> pipeline);
            void RegisterRenderPipeline(const AnsiString& name, std::unique_ptr<RenderPipelineBase> pipeline);

            /// Resource naming prefix for hierarchical GPU resource tracking
            /// Example: "RenderToTexture:OffscreenRT:IndirectDrawBuffer"
            /// ⚠ 读取端（`GetResourceNamePrefix()`）零调用者，2026-10-10 审计 §1.1 已删；写入端保留。
            void SetResourceNamePrefix(const std::string& prefix) { resource_name_prefix = prefix; }

            // ── T2-c：组件挂载 / 卸载 / 收集（世界侧唯一入口；实体只持行号）──
            //   `GetTypedPool` / `EnsureTypedPool` / `CreateComponent` / `GetComponentByRow` /
            //   `GetComponents` 的定义在本类内下方（`RenderFrameCache` 访问器之后的区块，
            //   声明与定义同处）；`DestroyComponent` 实现在 Context.cpp。

            /// 销组件行（运行期按类型索引；实体销毁与 `RemoveComponent<T>` 走它）
            void DestroyComponent(ComponentType type, uint32_t row);

            /// 登记/迁移/注销一个变换（按 TransformID；由作者 API 与过渡期的组件外壳调用）
            /// 登记：**返回是否成功**（段预算用尽 ⇒ false，调用方必须回收已分配的行 —— 批 1.1 B3）
            bool RegisterTransform(TransformID id, bool isMovable);
            /// 换段：**先取目标槽、成功后才交还旧槽**（失败即原状返回 false，原子拒绝 —— 批 1.1 B3）
            bool MigrateTransform(TransformID id, bool toMovable);
            void UnregisterTransform(TransformID id);

            /// 槽位分配（B1b）：段预算内取一个**稳定槽位**（优先复用空洞）；预算用尽返回 INVALID_TRANSFORM_SLOT
            uint32_t AcquireTransformSlot(bool movable);
            /// 归还槽位：置空洞 + 入空闲表（不整体平移，别的物体行号不受影响）
            void ReleaseTransformSlot(bool movable, uint32_t slot);
            /// 该段的声明式预算（真源 = 变换存储）
            uint32_t GetTransformSegmentBudget(bool movable) const;

            /// 建/销毁一个变换（entity 级 API 的基础：分配存储行 + 登记 owner/移动性）
            TransformID CreateTransform(EntityID owner, Mobility mobility);
            void        DestroyTransform(TransformID id);

            /// 实体 → 材质数据层组件 `MaterialData`（无则 nullptr）。
            /// **stage B 会换成值类型句柄**（与 TransformAccessor / BoundingBoxAccessor 同构），
            /// 届时不再返回裸指针。
            MaterialData* GetMaterialData(EntityID owner);
            const MaterialData* GetMaterialData(EntityID owner) const;
            MaterialData* GetMaterialData(Entity* owner);
            const MaterialData* GetMaterialData(const Entity* owner) const;

            /// 实体 → 材质数据层组件（无则 AddComponent 创建）。
            /// 与 GetOrCreateBoundingBox 同风格：渲染/作者侧需要"一定有数据层"时用。
            MaterialData* GetOrCreateMaterialData(EntityID owner);
            MaterialData* GetOrCreateMaterialData(Entity* owner);

            /// 实体 → 几何**资产侧**组件 `GeometryData`（无则 nullptr）。
            /// **stage B 会换成值类型句柄**（与 TransformAccessor / BoundingBoxAccessor 同构），
            /// 届时不再返回裸指针。
            GeometryData* GetGeometryData(EntityID owner);
            const GeometryData* GetGeometryData(EntityID owner) const;
            GeometryData* GetGeometryData(Entity* owner);
            const GeometryData* GetGeometryData(const Entity* owner) const;

            /// 实体 → 几何资产组件（无则 AddComponent 创建）。
            /// 与 GetOrCreateMaterialData 同风格：渲染/作者侧需要"一定有几何组件"时用。
            GeometryData* GetOrCreateGeometryData(EntityID owner);
            GeometryData* GetOrCreateGeometryData(Entity* owner);

            /// 实体 → 阴影属性组件 ShadowProxy（无则 nullptr）。
            /// A5b：阴影组件正名为 ShadowProxy，并成为**唯一**阴影组件；
            /// 未挂载时的缺省约定（可投射/可接收/距离 0/倍率 1.0）见
            /// components/ShadowProxy.h 的自由函数（唯一出处）。
            // 2026-10-10（审计 §1.1）：四个 `ECSContext::GetShadowProxy` 重载全仓零调用者 ⇒ 删除。
            // 取影子代理走组件侧自由函数 `hgl::ecs::GetShadowProxy(const Entity*)`（`components/ShadowProxy.h`）——
            // 它被 `CanCastShadow` / `GetShadowMaxDistance` / `CanReceiveShadow` / `GetShadowBiasMultiplier` 在用。

            /// 让某实体已解析的运行期管线缓存失效（几何/变体变化时调用）。
            /// A5b：管线缓存不再挂在渲染组件上，改由每实例 slot 承载 ⇒ 本入口清 slot 字段。
            void InvalidateEntityRuntimePipeline(EntityID owner);

            /// 实体 → 包围盒访问器（无则返回无效句柄）
            BoundingBoxAccessor GetBoundingBoxByEntity(EntityID owner) const;

            /// 实体 → 包围盒访问器（无则分配存储行 + 登记 owner）
            BoundingBoxAccessor GetOrCreateBoundingBox(EntityID owner);

            /// 销毁一行包围盒（按句柄；行属于世界，不挂在实体上）
            void DestroyBoundingBox(BoundingBoxDataStorage::HandleID id);

            /// Get world-level BoundingBoxDataStorage
            BoundingBoxDataStorage* GetBoundingBoxStorage() { return bounding_box_storage.get(); }
            const BoundingBoxDataStorage* GetBoundingBoxStorage() const { return bounding_box_storage.get(); }

            /// 世界级可见性存储（渲染热路径做 O(1) 查询；不可见集合 + 祖先链语义）
            VisibilityDataStorage* GetVisibilityStorage() { return visibility_storage.get(); }
            const VisibilityDataStorage* GetVisibilityStorage() const { return visibility_storage.get(); }

            // ── T3b：实体行表（`EntityGPU`）访问器与两条写入钩子 ──
            EntityGPUTable&       GetEntityGPUTable()       { return entity_gpu_table; }
            const EntityGPUTable& GetEntityGPUTable() const { return entity_gpu_table; }

            /// 实体销毁路径：清 `Active`（由 `~Entity` 调用 —— 实体生死是**单一入口**）
            /// T3d-3a：同入口清实体 4-ID 表行（两表的生死口径必须一致：消费者先判
            /// `EntityGPU.Active`、4-ID 行同时归零 ⇒ 不会出现"行表已死、id 还在"的残值）
            void DeactivateEntityRow(EntityID id)
            {
                entity_gpu_table.Deactivate(id);
                entity_render_id_table.Clear(id);
            }

            /// T3d-3a：实体 4-ID 表访问器（写者 = collect；消费者 = GPU 剔除 / CPU 参考对拍 / 形态 B）
            EntityRenderIDTable&       GetEntityRenderIDTable()       { return entity_render_id_table; }
            const EntityRenderIDTable& GetEntityRenderIDTable() const { return entity_render_id_table; }

            // ── T6b：光源行表（`LightRow`，世界私有；行 0 = 主方向光）──
            //   写者 = LightSystem 的编辑入口；本表只被世界的「脏行 → 设备镜像」上传播径读。
            LightRowTable&       GetLightRowTable()       { return light_row_table; }
            const LightRowTable& GetLightRowTable() const { return light_row_table; }

            // ── T6b/P4：影子数据（`ShadowInfo`，世界私有）──────────────────────────
            /// 编辑本世界的影子数据（CPU 权威；GPU 侧由 `CommitShadowInfo` 在提交点写本帧槽）
            graph::ShadowInfo       *EditShadowInfo()       { return &shadow_info; }
            // 2026-10-10（审计 §1.1）：只读 `GetShadowInfo()` 已删除 —— 它唯一的调用者是同样零外部调用者的
            // `MainLightShadowSystem::GetShadowInfo()`（一并删除）；读影子数据请经 `EditShadowInfo()`。

            void SetShadowInfo(const graph::ShadowInfo &info) { shadow_info = info; }

            /// 提交点（交换链帧 acquire 之后）把 CPU 权威的影子数据写进**本帧槽**的设备副本。
            void CommitShadowInfo(const uint32_t frame_slot);

            /// 组件卸载 / 全卸后重刷 `type[]/row[]`（由 `Entity` 的卸载路径调用）
            void RefreshEntityRow(Entity *entity) { entity_gpu_table.Refresh(entity); }

            /// 设置实体可见性 —— **唯一真值落在这里**（不再有组件内的第二份 bool）
            void SetEntityVisible(EntityID id, bool visible)
            {
                if (!visibility_storage)
                    return;

                if (visible)
                    visibility_storage->SetVisible(id);
                else
                    visibility_storage->SetInvisible(id);

                // T3b：镜像到实体行（唯一真值仍是 VisibilityDataStorage；本表是派生视图）
                entity_gpu_table.SetVisible(id, visible);
            }

            /// 实体是否可见：O(1) 直查不可见集合 + 祖先链上溯（祖先不可见 ⇒ 后代不可见）
            bool IsEntityVisible(EntityID id) const
            {
                return visibility_storage ? !visibility_storage->IsInvisible(id) : true;
            }

            // ── 变换名单的**结构性遍历**（唯一正确方式）─────────────────────────
            //   名单里存在**空洞**：销毁 / Mobility 换边会留下 `INVALID_TRANSFORM_ID`
            //   （见下方 movable_transforms 注释：「空洞记 INVALID_TRANSFORM_ID，注册优先复用空洞」）。
            //   裸遍历 `GetMovableTransforms()/GetStaticTransforms()` 会把空洞当成有效 id 传下去，
            //   以 (uint32)-1 索引行存储的平行数组 ⇒ 越界崩溃（2026-10-02 实测）。因此：
            //   **新增消费者一律用这两个入口**（内部自动跳洞，调用方不可能漏）。

            /// 结构性遍历：世界的**存活实体**（内部跳空槽、零分配）。
            /// 与 `ForEachMovableTransform/ForEachStaticTransform` **同族**：系统一律走世界的口子，
            /// 不直接摸 `entity_manager`（它是私有成员，且消费者不该关心名册的内部形态）。
            template<typename Fn>
            void ForEachAliveEntity(Fn &&fn) const
            {
                if (entity_manager)
                    entity_manager->ForEachAliveEntity(std::forward<Fn>(fn));
            }

            template<typename Fn>
            void ForEachMovableTransform(Fn &&fn) const
            {
                for (const TransformID id : movable_transforms)
                    if (IsValidTransformID(id))
                        fn(id);
            }

            template<typename Fn>
            void ForEachStaticTransform(Fn &&fn) const
            {
                for (const TransformID id : static_transforms)
                    if (IsValidTransformID(id))
                        fn(id);
            }

            /// 实体 → 变换行（无变换时返回 INVALID_TRANSFORM_ID）
            TransformID GetTransformID(EntityID owner) const;

            /// 变换访问器（薄句柄；读写直落本世界的存储）
            TransformAccessor GetTransform(TransformID id);

            const std::vector<TransformID>& GetStaticTransforms() const { return static_transforms; }
            const std::vector<TransformID>& GetMovableTransforms() const { return movable_transforms; }

            /// 变换的**稳定槽位**（B1b）：命中返回 true 并写 `out_slot`。
            /// 槽位是 L2W 行号的唯一来源（静态 `1+slot`、动态 `1+预算static + frame×预算dynamic + slot`），
            /// 一旦分配就不再随名单顺序变动（空洞复用，不整体平移）。
            bool TryGetTransformSlot(TransformID id, bool movable, uint32_t &out_slot) const;

            /// 该段**活着的**变换数（≠ 槽位表长度：表里有空洞）
            uint32_t GetLiveTransformCount(bool movable) const;

            /// Mobility 换边计数（每次换段 = 旧行号作废 ⇒ 计数增；可观测/契约用）
            uint32_t GetTransformRowMoveCount() const { return transform_row_move_count; }

            /// 实体 → 变换访问器（无变换时返回无效 accessor）
        TransformAccessor GetTransformByEntity(EntityID owner)
        {
            return GetTransform(GetTransformID(owner));
        }

        /// Get world-level TransformDataStorage
            TransformDataStorage* GetTransformStorage() { return transform_storage.get(); }
            const TransformDataStorage* GetTransformStorage() const { return transform_storage.get(); }

            /// Get world-level RenderItemDataStorage
            RenderItemDataStorage* GetRenderItemStorage() { return render_item_storage.get(); }
            const RenderItemDataStorage* GetRenderItemStorage() const { return render_item_storage.get(); }

            /// Get world-level DrawItemIDStorage
            DrawItemIDStorage* GetDrawItemIDStorage() { return draw_item_id_storage.get(); }
            const DrawItemIDStorage* GetDrawItemIDStorage() const { return draw_item_id_storage.get(); }

            /// A3 材质变体表（材质运行期 program/recipe 按静态键去重；见 support/MaterialVariantTable.h）
            MaterialVariantTable* GetMaterialVariantTable() { return material_variant_table.get(); }
            const MaterialVariantTable* GetMaterialVariantTable() const { return material_variant_table.get(); }

            /// A4 材质运行期表（共享行 interned + CoW + refcount + 世代；见 support/MaterialRuntimeTable.h）
            /// B5-b 运行期管线旁表（键 = 实体；按 pass 比对）
            MaterialRuntimePipelineTable& GetMaterialRuntimePipelineTable() { return *material_runtime_pipeline_table; }
            const MaterialRuntimePipelineTable& GetMaterialRuntimePipelineTable() const { return *material_runtime_pipeline_table; }


            MaterialAuthoringSideTable* GetMaterialAuthoringTable() { return material_authoring_side_table.get(); }
            const MaterialAuthoringSideTable* GetMaterialAuthoringTable() const { return material_authoring_side_table.get(); }

            MaterialRuntimeTable* GetMaterialRuntimeTable() { return material_runtime_table.get(); }
            const MaterialRuntimeTable* GetMaterialRuntimeTable() const { return material_runtime_table.get(); }

            /// 实体 → 材质运行期 slot（无则创建）。命名向 `GetOrCreateMaterialData` 看齐。
            /// ⚠ stage B 会换成**值类型句柄**（与 TransformAccessor / BoundingBoxAccessor 同构），
            /// 届时不再返回引用。
            MaterialRuntimeSlot& GetOrCreateMaterialRuntimeSlot(EntityID owner)
            {
                return material_runtime_table->GetOrCreateSlot(owner);
            }

            /// 指针版（作者/引擎手里已有实体）
            MaterialRuntimeSlot& GetOrCreateMaterialRuntimeSlot(Entity *owner)
            {
                return material_runtime_table->GetOrCreateSlot(owner->GetEntityID());
            }

            /// 实体 → 材质运行期 slot（无则 nullptr）
            MaterialRuntimeSlot* GetMaterialRuntimeSlot(EntityID owner)
            {
                return material_runtime_table->GetSlot(owner);
            }

            MaterialRuntimeSlot* GetMaterialRuntimeSlot(Entity *owner)
            {
                return material_runtime_table->GetSlot(owner->GetEntityID());
            }

            /// Get 世界私有相机行存储（相机 = 世界级观察者数据；0 号槽 = 本世界默认相机）
            CameraInfoStorage* GetCameraInfoStorage() { return camera_info_storage.get(); }
            const CameraInfoStorage* GetCameraInfoStorage() const { return camera_info_storage.get(); }

            /// 槽拥有者（CameraSlotGuard / 归还挂钩）用：弱引用 ⇒ 世界先销毁时 lock 失败 = 安全 no-op
            std::weak_ptr<CameraInfoStorage> GetCameraInfoStorageWeak() const { return camera_info_storage; }

            /// 世界地址表基址（**本帧槽**）：写入 pc_root.addr_world_addresses（0 = 尚未创建）
            uint64_t GetWorldAddressesAddress(uint32_t frame_slot) const;

            /// 把本世界的 world 私有地址（相机行表 / 渲染项表 / DrawItemID 表）同步进**当前帧槽**的世界表。
            /// 槽 = 当前 RT 的帧槽（主帧 [0,4) / 离屏 [4,8)），由 SetFrameIndex 触发。
            bool SyncWorldAddresses();

            /// 本世界的 EnvironmentManager（经 RenderContext/GraphicsContext 解析；不可用时 nullptr）
            hgl::graph::EnvironmentManager *ResolveEnvManager() const;

        public:

            /// Create a new entity in the world
            /// Returns raw pointer managed by EntityManager
            template<typename T = Entity, typename... Args>
            T* CreateEntity(Args&&... args)
            {
                static_assert(std::is_base_of_v<Entity, T>, "T must derive from hgl::ecs::Entity");

                if (!entity_manager)
                    return nullptr;

                // T2-a：实体由 EntityManager 在**块池**中就地构造（不再逐实体 make_unique）
                T* entity = entity_manager->CreateEntity<T>(std::forward<Args>(args)...);
                if (!entity)
                    return nullptr;

                entity->SetContext(this);

                // T3b：实体行表置 `Active`（`type[]/row[]` 随组件挂卸由 `Refresh` 同步）
                entity_gpu_table.Activate(entity->GetEntityID());

                return entity;
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
            /// —— 同时释放它的变换行：行属于世界（不挂在实体上），实体没了没人回收的话
            ///    残行会留在静态/可动列表里，把渲染侧的实例→行索引映射整体串位。
            void DestroyEntity(EntityID id)
            {
                const TransformID transform = GetTransformID(id);

                if (IsValidTransformID(transform))
                    DestroyTransform(transform);

                // 与变换行同理回收包围盒行：行属于世界（不挂在实体上），实体没了没人回收的话，
                // 残行会留在 entity_rows 反查表里，让同索引的新实体"继承"旧的包围盒。
                if (bounding_box_storage)
                {
                    const BoundingBoxDataStorage::HandleID bbox = bounding_box_storage->FindByOwner(id);

                    if (bbox != BoundingBoxDataStorage::INVALID_HANDLE)
                        DestroyBoundingBox(bbox);
                }

                // 可见性同理：不可见集合按实体 ID 记录，实体没了必须摘掉，
                // 否则同索引的新实体会"继承"旧的不可见状态（T8 同类副作用）。
                if (visibility_storage)
                    visibility_storage->SetVisible(id);

                // 材质运行期 slot 同理：slot 持共享行的引用计数，实体没了必须释放
                // （否则共享行永远不归零 ⇒ 纹理配置池行泄漏、行表只增不减）。
                // A5b：render_item 句柄与多实例连续槽位也归 slot ⇒ 先释放再销毁 slot。
                ReleaseRenderItemHandle(*this,id);

                if (material_runtime_table)
                    material_runtime_table->DestroySlot(id);

                if (entity_manager)
                    entity_manager->DestroyEntity(id);
            }

            /// Ensure CameraSystem exists in this context.
            /// If created after context activation, it is initialized immediately.
            std::shared_ptr<CameraSystem> EnsureCameraSystem();

            /// 注册一个系统。
            /// **tick / render 的分类不接受调用方声明**：唯一权威是该系统自己的 `ExecutionPhase`
            /// （分界 = `RenderSwapchainNextImage`）。曾经的 `is_render` 形参与
            /// `RegisterTickSystem` / `RegisterRenderSystem` 两个包装都不决定任何列表，
            /// 只在相位误配时打一句告警就把系统静默换表 ⇒ 整条声明面已删除（2026-10-09）。
            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterSystem(Args&&... args)
            {
                auto system = std::make_shared<T>(std::forward<Args>(args)...);
                const size_t key = typeid(T).hash_code();
                AddOrUpdateSystem(key, system);
                return system;
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

            /// 启用/禁用某个系统组的全部系统（A2：按枚举 O(1)，不查字符串）
            void SetSystemGroupEnabled(SystemGroupID group, bool enabled);

            /// 该组的安装器是否已在本世界跑过（A2：枚举）
            bool IsSystemGroupInstalled(SystemGroupID group) const;

            /// 标记该组已安装（幂等）；**同时快照该组的系统**（供上面的 O(1) 启用路径）
            void MarkSystemGroupInstalled(SystemGroupID group);

            /// 重置组件驱动的组状态（计数 / 已安装 / 快照）—— 世界 Shutdown 用
            void ResetComponentDrivenGroups();

        public:
            RenderFrameCache& GetRenderFrameCache() { return render_frame_cache; }
            const RenderFrameCache& GetRenderFrameCache() const { return render_frame_cache; }

            /// Check if world is active
            bool IsActive() const { return active; }

            /// 该类型的池（**只查不建** ⇒ 读路径无副作用）；类型无载体 / 未创建 ⇒ nullptr
            template<typename T>
            typename ComponentStorageTraits<T>::PoolType *GetTypedPool() const
            {
                constexpr ComponentType type = ComponentTypeOf<T>::value;

                if (type == ComponentType::None)
                    return nullptr;

                return static_cast<typename ComponentStorageTraits<T>::PoolType *>(
                           component_pool_slots[static_cast<uint32_t>(type)]);
            }

            /// 该类型的池（不存在则按 `ComponentStorageTraits<T>` 建）；
            /// `ComponentTypeOf<T> == None`（无类型号）⇒ **编译期拒绝**（实体槽 = 按类型行号）
            template<typename T>
            typename ComponentStorageTraits<T>::PoolType *EnsureTypedPool()
            {
                constexpr ComponentType type = ComponentTypeOf<T>::value;

                static_assert(type != ComponentType::None,
                              "组件必须有类型号（T2-c：实体槽 = 按类型行号）—— 请在组件头里特化 ComponentTypeOf<T>");

                const uint32_t index = static_cast<uint32_t>(type);

                if (!component_pool_slots[index])
                {
                    auto pool = std::make_unique<typename ComponentStorageTraits<T>::PoolType>();

                    component_pool_slots[index] = pool.get();
                    component_pools.push_back(std::move(pool));
                }

                return static_cast<typename ComponentStorageTraits<T>::PoolType *>(component_pool_slots[index]);
            }

            /// 挂载组件（返回池内行地址；已有同类型 ⇒ 先卸旧行，语义与旧 `ReplaceComponent` 一致）
            template<typename T, typename... Args>
            T *CreateComponent(Entity *owner, Args &&...args)
            {
                if (!owner)
                    return nullptr;

                constexpr ComponentType type = ComponentTypeOf<T>::value;

                auto *pool = EnsureTypedPool<T>();

                // 替换语义：旧行先销（卸载钩子 + 析构），再挂新行 —— 与原 `ReplaceComponent` 同序
                const uint32_t old_row = owner->SetComponentRow(type, 0);

                if (old_row)
                {
                    pool->DestroyRow(old_row);
                    OnComponentRowRemoved(type);
                }

                const uint32_t row = pool->Create(owner, std::forward<Args>(args)...);

                owner->SetComponentRow(type, row);
                OnComponentRowAdded(type);
                entity_gpu_table.Refresh(owner);        // T3b：实体行表随挂载同步
                owner->MarkSceneDirty();

                return pool->Row(row);
            }

            /// 取某行的组件（行号 0 / 无池 / 已释放 ⇒ nullptr）
            template<typename T>
            T *GetComponentByRow(const uint32_t row) const
            {
                auto *pool = GetTypedPool<T>();

                return pool ? pool->Row(row) : nullptr;
            }

            /// 该类型的**活跃集视图**（池即活跃集；裸指针，生命期 = 池行生命期，不跨帧缓存对象）
            template<typename T>
            void GetComponents(ValueArray<T*> &out) const
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
        };

        // ── Entity 组件挂载/卸载（T2-c：实现在这里 —— 模板体需要完整的 ECSContext）──

        template<typename T, typename... Args>
        T *Entity::AddComponent(Args &&...args)
        {
            return context ? context->CreateComponent<T>(this, std::forward<Args>(args)...) : nullptr;
        }

        template<typename T>
        T *Entity::GetComponent() const
        {
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
                context->RefreshEntityRow(this);        // T3b：实体行表随卸载同步
            }

            MarkSceneDirty();
        }
    }//namespace ecs
}//namespace hgl
