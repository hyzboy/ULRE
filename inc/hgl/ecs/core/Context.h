#pragma once

#include<hgl/vk/VK.h>
#include<hgl/ecs/core/ECSCoreContext.h>
#include<hgl/ecs/core/entity/Object.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/core/system/System.h>
#include<hgl/ecs/core/system/SystemGroupActivity.h>
#include<hgl/ecs/core/system/SystemRegistry.h>
#include<hgl/ecs/core/FrameLaneWaitSet.h>
#include<hgl/ecs/core/RenderPassScopeState.h>
#include<hgl/ecs/core/MainFrameTopologyState.h>
#include<hgl/ecs/core/AdaptiveRenderGraphCache.h>
#include<hgl/ecs/core/render/RenderGraph.h>
#include<hgl/ecs/core/render/RenderFrameCache.h>
#include<hgl/ecs/core/render/RenderPassRequest.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/ecs/core/render/ScenePipelineMode.h>
#include<hgl/ecs/support/storage/ComponentTypeTable.h>
#include<hgl/ecs/support/storage/ComponentPool.h>
#include<hgl/ecs/support/storage/ComponentPoolRegistry.h>
#include<hgl/ecs/support/storage/EntityGPUTable.h>
#include<hgl/ecs/support/storage/EntityRenderIDTable.h>
#include<hgl/ecs/support/storage/PrimitiveTopologyLedger.h>
#include<hgl/ecs/support/storage/MainFrameTopologySnapshot.h>
#include<hgl/ecs/support/storage/LightRowTable.h>
#include<hgl/graph/ubo/ShadowInfo.h>
#include<hgl/type/ValueArray.h>
#include<hgl/ecs/support/transform/TransformID.h>
#include<hgl/ecs/support/transform/TransformSlotTable.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/support/cull/BoundingBoxAccessor.h>
#include<hgl/ecs/support/cull/VisibilityDataStorage.h>
#include<hgl/ecs/support/spatial/WorldSpatialState.h>
#include<hgl/ecs/support/render/WorldRenderData.h>
#include<hgl/ecs/support/render/WorldGPUResources.h>
#include<hgl/ecs/support/render/WorldRenderRunner.h>
#include<hgl/ecs/support/material/MaterialVariantTable.h>
#include<hgl/ecs/support/material/MaterialRuntimeTable.h>
#include<hgl/ecs/support/material/ModelTemplateMaterialTable.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/support/material/MaterialAuthoringSideTable.h>
#include<hgl/ecs/core/entity/EntityManager.h>
#include<hgl/graph/ubo/EnvironmentInfo.h>
#include<hgl/graph/render/RenderTargetObserver.h>
#include<hgl/log/Log.h>
#include<memory>
#include<functional>
#include<vector>
#include <hgl/type/UnorderedMap.h>
#include<typeinfo>
#include<type_traits>
#include<hgl/ecs/core/render/ShaderProgramPipelineKey.h>
#include<hgl/color/Color4f.h>
#include<glm/glm.hpp>

namespace hgl {
    namespace graph {
        class RenderCmdBuffer;
        struct CameraInfo;
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
        class CameraInfoStorage;
        class MaterialData;
        class GeometryData;
        class ShadowProxy;
        class MaterialAuthoringSideTable;
        class MaterialRuntimePipelineTable;

        class ECSContext;

        /**
         * ECSContext manages all entities and systems
         * Acts as the main container for the ECS simulation
         */
        class ECSContext : public ECSCoreContext, public graph::RenderTargetObserver, public IWorldSpatialObserver, private IECSEntityObserver, private IWorldGPUResourcesSource, private IWorldRenderDataSource
        {
        private:
            void ConfigureSystem(System *system) override;

        public:

            ECSContext(const char* name) : ECSContext(AnsiString(name)) {}
            ECSContext(const AnsiString& name = "World");
            ECSContext(const std::string& name) : ECSContext(AnsiString(name.c_str())) {}
            ~ECSContext() override;

            void OnTransformRegistered(TransformID id, bool isMovable) override;
            void OnTransformMembershipChanged(TransformID id) override;
            void OnTransformChanged(TransformID id, uint32_t change_mask) override;
            void OnEntityVisibilityChanged(EntityID id, bool visible) override;

        private:
            OBJECT_LOGGER
            friend struct RenderPassSetupTestAccess;
            const AnsiString &RenderDataWorldName() const override { return GetName(); }
            void RenderDataPipelineInvalidated() override { MarkTopologyChanged(TopologyChange::MaterialBinding); }
            void RenderDataModelTemplateChanged() override { MarkTopologyChanged(TopologyChange::ModelTemplate); }
            graph::GraphicsContext *RenderDataGraphicsContext() override;
            graph::IRenderTarget *RenderDataRenderTarget() override;
            graph::TextureManager *RenderDataTextureManager() override;
            uint64_t RenderDataSubmissionSerial() const override { return GetRenderSubmissionSerial(); }
            bool RenderDataGPUQuiescent() const override;

            graph::GraphicsContext *GPUResourcesGraphicsContext() override;
            graph::VulkanDevice *GPUResourcesDevice() override;
            const AnsiString &GPUResourcesWorldName() const override;
            EntityGPUTable &GPUResourcesEntityTable() override;
            EntityRenderIDTable &GPUResourcesRenderIDTable() override;
            LightRowTable &GPUResourcesLightTable() override;
            const graph::ShadowInfo &GPUResourcesShadowInfo() const override;
            CameraInfoStorage *GPUResourcesCameraStorage() override;
            uint64_t GPUResourcesTransformAddress() override;
            uint32_t GPUResourcesEnvironmentProfile() const override;
            uint64_t GPUResourcesAmbientAddress(uint32_t profile) const override;
            uint64_t GPUResourcesModelExpansionAddress() override;

            // ── T3b：实体行表（`EntityGPU` 的 GPU 派生视图；世界私有）──
            // Shutdown 在派生成员析构前清理实体并注销观察者。
            EntityGPUTable entity_gpu_table;
            uint64_t render_diagnostic_world_id = 0;

            /// T3d-3a：实体 4-ID 表（世界私有；销毁通知同时清理两表）
            EntityRenderIDTable entity_render_id_table;

            // Component-driven system-group activity tracking（A2：枚举 + O(1) 数组，热路径无字符串）
            SystemGroupActivity system_group_activity;

            // 空间域聚合状态（变换槽位与行存储、包围盒行存储、可见性集合）
            WorldSpatialState spatial_state;

            // CPU 渲染数据底座（光源行表、阴影参数、材质表组、世界相机表与常驻 Fallback）
            WorldRenderData render_data;

            // GPU 侧资源与设备镜像表（世界地址表、实体行表、4-ID 表、光源表、影子表）
            WorldGPUResources gpu_resources;

            // 渲染执行器与调度器（渲染帧生命周期、Pass 调度、阶段驱动与帧缓存）
            WorldRenderRunner render_runner;

            /// Unified render pipeline registry: name → RenderPipelineBase
            /// Managed by SystemGroup installers (e.g., InstallPrimitiveGroup, InstallLineGroup)
            /// Supports dynamic enable/disable per SystemGroup
            hgl::UnorderedMap<AnsiString, RenderPipelineBase*> render_pipelines;

            // ========== GPU 设备和资源管理（Phase 1 新增） ==========

            /// GPU 设备（从旧集中式入口迁移来）
            hgl::graph::VulkanDevice* gpu_device = nullptr;

            /// 用于渲染的目标
            hgl::graph::IRenderTarget* render_target = nullptr;

            /// 本世界的环境 profile（**Env 随世界**，C2）：显式覆盖（`SetEnvProfileID` /
            /// `CreateEnvProfile`）优先；未显式设置时**按需解析本世界 RT 的 env_profile**
            /// （作者侧在 `SetEnvironmentProfile`，可能发生在世界创建之后 ⇒ 不能只在 Initialize 取一次）。
            graph::EnvProfileID env_profile = graph::kEnvProfileDefault;
            bool env_profile_explicit = false;   ///< true ⇒ 用 `env_profile`；false ⇒ 跟随本世界 RT
            bool env_profile_owned = false;      ///< true ⇒ Shutdown 时 `Release()` 归还（本世界创建）

            // ---- A1 车道等待列表（per-frame，见 RenderOptions.h 的车道说明）----

            /// 帧提交等待列表：纯状态机，无世界/设备依赖
            FrameLaneWaitSet lane_waits;

            /// 静态场景 revision：任何 Static transform 被检出变更时递增（A3）。
            /// 消费方（如 EnvironmentSystem 的静态级联阴影缓存）记录上次消费值，
            /// 与当前值不等即知"静态场景已变"，做对应的缓存失效。只增不减。
            uint64_t static_scene_revision = 0;

            /// Graphics context adapter (Phase 2) - now raw pointer
            hgl::graph::GraphicsContext* graphics_context = nullptr;
            hgl::graph::RenderContext* render_context = nullptr;

            /// Resource naming prefix for hierarchical tracking (e.g., "RenderToTexture:OffscreenRT")
            /// Used by systems when creating GPU resources for better leak tracking
            AnsiString resource_name_prefix;

        private:

            void RunRenderPhaseUpdates(ExecutionPhase phase, float deltaTime);
            void RunRenderPhaseUpdates(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime);
            void RunRenderSystemsInRange(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime);
            void RunSystemUpdate(System *system, float deltaTime) override;

            /// 组件行增/删 ⇒ 系统组计数（A2/P-4 的挂卸驱动路径；组归属取自类型表）
            void OnComponentRowAdded(ComponentType type) override;
            void OnComponentRowRemoved(ComponentType type) override;
            void OnEntityCreated(EntityID id) override
            {
                entity_gpu_table.Activate(id);
                MarkTopologyChanged(TopologyChange::EntityStructure);
            }
            void OnEntityDestroying(EntityID id) override;
            void OnEntityCleanup(EntityID id) override;

            bool EnsureRenderCoreInitialized();
            void ExecuteScenePrePassWorkflow(float deltaTime);

            bool BeginManagedRenderFrame(float deltaTime, bool need_swapchain_acquire = true, const graph::RenderPassOptions *options = nullptr);
            bool EndManagedRenderFrame(float deltaTime);
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
            void Shutdown() override;

            /// Tick all non-render systems and entities
            void Tick(float deltaTime) override;

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
            void MarkSceneStructureDirty() const { render_runner.MarkSceneStructureDirty(); }

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
            uint32_t GetFrameIndex() const { return render_runner.GetFrameIndex(); }
            uint64_t GetRenderDiagnosticWorldID() const { return render_diagnostic_world_id; }

            int GetActiveMobilityFilter() const { return render_runner.GetPassScopeState().GetMobilityFilter(); }

            /// Static transform 变更检出时递增（由 TransformSystem 调用；A3 静态缓存失效链）
            void BumpStaticSceneRevision() { ++static_scene_revision; }
            uint64_t GetStaticSceneRevision() const { return static_scene_revision; }

            /// 拓扑失效台帐（只读访问；写入统一走 `MarkTopologyChanged/MarkDynamicFactChanged`）
            const PrimitiveTopologyLedger &GetPrimitiveTopologyLedger() const { return render_runner.GetMainFrameTopology().Ledger(); }

            /// 可写访问：**只**给统计累加（`RecordEligibility`）与设备轮询用；
            /// 拓扑/事实的标记仍应走下面的两个具名入口，以免绕过 `MarkSceneStructureDirty`。
            PrimitiveTopologyLedger &GetPrimitiveTopologyLedger() { return render_runner.GetMainFrameTopology().Ledger(); }

            /// 主帧拓扑快照（P2）。可写访问：构建方（批阶段尾部）与统计读取方共用。
            MainFrameTopologySnapshot &GetMainFrameTopologySnapshot() { return render_runner.GetMainFrameTopology().Snapshot(); }
            const MainFrameTopologySnapshot &GetMainFrameTopologySnapshot() const { return render_runner.GetMainFrameTopology().Snapshot(); }

            /// 开/关主帧拓扑快照。**关闭时必须完全不构建**（P2 的零开销保证）。
            void SetMainFrameTopologySnapshotEnabled(bool enabled) { render_runner.GetMainFrameTopology().SetSnapshotEnabled(enabled); }
            bool IsMainFrameTopologySnapshotEnabled() const { return render_runner.GetMainFrameTopology().IsSnapshotEnabled(); }

            /// 开/关主帧**收集快路径**（P3）。开启后主帧在满足全部资格条件时直接从
            /// 上一份主帧拓扑快照重建收集结果；条件不满足（任一帧）就退回完整收集，
            /// 不做任何"部分复用"。关闭时整条快路径不存在（零开销）。
            void SetMainFrameCollectCacheEnabled(bool enabled) { render_runner.GetMainFrameTopology().SetCollectCacheEnabled(enabled); }
            bool IsMainFrameCollectCacheEnabled() const { return render_runner.GetMainFrameTopology().IsCollectCacheEnabled(); }

            /// 本 pass 是否有资格充当「主帧」（与 P1 资格判定的谓词完全一致）：
            /// 非阴影 pass 且无移动性过滤。阴影/过滤 pass 与主帧共用同一份
            /// `RenderFrameCache`，成员集合天然不同 ⇒ 不得互相覆盖快照。
            bool IsMainFrameTopologyPass() const
            {
                return !IsCurrentPassShadow() && render_runner.GetPassScopeState().GetMobilityFilter() < 0;
            }

            /// 声明拓扑变更（写入方调用；见 `TopologyChange` 各类别的注释）
            void MarkTopologyChanged(TopologyChange kind, uint32_t count = 1) const
            {
                render_runner.GetMainFrameTopology().MarkTopology(kind, count);
                MarkSceneStructureDirty();
            }

            /// 声明动态事实变更（不改拓扑，只需重新发布到 GPU 行）
            void MarkDynamicFactChanged(DynamicFactChange kind, uint32_t count = 1) const
            {
                render_runner.GetMainFrameTopology().MarkDynamicFact(kind, count);
            }

            bool IsCurrentPassShadow() const { return render_runner.GetPassScopeState().IsShadowPass(); }
            bool HasShadowOrigin() const { return render_runner.GetPassScopeState().HasShadowOrigin(); }
            const glm::vec3 &GetShadowOrigin() const { return render_runner.GetPassScopeState().ShadowOrigin(); }

            RenderPassScopeState &GetPassScopeState() { return render_runner.GetPassScopeState(); }
            const RenderPassScopeState &GetPassScopeState() const { return render_runner.GetPassScopeState(); }

            MainFrameTopologyState &GetMainFrameTopologyState() { return render_runner.GetMainFrameTopology(); }
            const MainFrameTopologyState &GetMainFrameTopologyState() const { return render_runner.GetMainFrameTopology(); }

            void SetScenePipelineMode(ScenePipelineMode mode)
            {
                render_runner.SetScenePipelineMode(mode);
            }
            ScenePipelineMode GetScenePipelineMode() const { return render_runner.GetScenePipelineMode(); }

            void SetRenderSubmissionSerial(const uint64_t serial)
            {
                render_runner.SetRenderSubmissionSerial(serial);
            }
            uint64_t GetRenderSubmissionSerial() const
            {
                return render_runner.GetRenderSubmissionSerial();
            }

            WorldRenderRunner &GetRenderRunner() { return render_runner; }
            const WorldRenderRunner &GetRenderRunner() const { return render_runner; }

            // ========== GPU 设备和资源接口（Phase 1 新增） ==========

            /// 获取 GPU 设备
            hgl::graph::VulkanDevice* GetGPUDevice() { return gpu_device; }

            /// 设置渲染目标（用于窗口 resize 等场景重建 render target）
            void SetRenderTarget(hgl::graph::IRenderTarget* target) { render_target = target; }
            void OnSwapchainTargetChanged(hgl::graph::IRenderTarget *target) override;
            void OnRenderTargetResized(hgl::graph::IRenderTarget *target, uint32 width, uint32 height) override;

            /// [唯一权威 getter] 获取本世界绑定的渲染目标。
            ///
            /// 应用代码取 RT 一律走此入口。其余入口
            /// （RenderContext::GetCurrentRenderTarget / AppFramework::GetSwapchainRenderTarget /
            ///  SwapchainModule::GetRenderTarget / RenderTargetSystem::GetRenderTarget）
            /// 均为框架内部接线或系统内缓存，不应在应用层直接依赖。
            hgl::graph::IRenderTarget* GetRenderTarget() { return render_target; }

            /// 获取当前渲染命令缓冲区（仅在 Render() 执行期间有效）
            hgl::graph::RenderCmdBuffer* GetCurrentRenderCmd() { return render_runner.GetCurrentRenderCmd(); }
            void SetCurrentRenderCmd(hgl::graph::RenderCmdBuffer* cmd) { render_runner.SetCurrentRenderCmd(cmd); }

            /// 本 pass 活跃相机的**槽号**（世界内；PushConstants 的 `camera_row` 由它算出行号）。
            /// 只由本世界的 pass 设置路径（`RenderTo`）写 —— 不做公开 setter，避免出现第二个写者。
            // 2026-10-10（审计 §1.1）：零调用者的 `GetActiveCameraID()` 已删除（活跃相机槽仍存在，
            // 但只经 `RenderPassScopeState` 读写，不再对外暴露裸成员）。

            /// 当前相机的 CameraInfo 行号 = 槽号 * HGL_FRAME_SLOT_TOTAL + 本帧数据槽。
            /// 着色器端 `pc_root.camera_row` 收到的就是这个行号（scene_ubo.glsl 的 cameras[] 下标），
            /// 因此离屏 pass 与主帧各自的相机数据落在不相交的行上。
            uint32_t GetActiveCameraRow() const;

            /// **本 pass 生效相机**的 CameraInfo（组件自己的那份，没有世界共享载体）。
            /// 离屏 pass 期间 = `RenderTo(req.camera)` 的相机（如 CSM 光相机），主帧 = 本世界主相机。
            /// **认领版**（会认领 0 号槽 / 必要时生成常驻 fallback）：
            /// 剔除 / Line 视锥 / shadow origin 等**渲染侧**统一读这里，不要各自缓存指针。
            const graph::CameraInfo* GetActiveCameraInfo();

            /// **纯读版**（§3-⑥ 2026-10-11）：不认领 0 号槽、不生成 fallback、不改世界状态；
            /// 世界当前解析不到相机 ⇒ nullptr。
            /// **只读意图（gizmo 朝向 / 拾取射线 / 诊断）一律走这里** —— 走认领版会让"读一次"变成
            /// "改世界"（空世界凭空多出常驻 fallback 相机并占 0 号槽；有相机时会把某个相机升格）。
            const graph::CameraInfo* ResolveActiveCameraInfo() const;

            /// 本世界**生效**的 env profile：显式设置优先，否则取本世界 RT 的 env_profile，再退回
            /// 内置 default。世界表里的 `addr_ambient_env` / `addr_shadow` 按它解析。
            graph::EnvProfileID GetEnvProfileID() const;

            /// 选本世界的 env profile；`take_ownership=true` 表示"本世界创建的"（Shutdown 时归还）。
            void SetEnvProfileID(graph::EnvProfileID id, bool take_ownership = false);

            /// 建一个**本世界拥有**的 env profile 并立刻选用（Shutdown 时自动 `Release`）。
            /// 返回 `kEnvProfileInvalid` 表示管理不可用 / 创建失败。
            graph::EnvProfileID CreateEnvProfile(const AnsiString &name,
                                                 const graph::EnvironmentInfo &init = {});

            /// 本世界生效 profile 的 **ambient provider 模块名**（`ambient_light_provider` 槽的
            /// GLSL 模块；**空 = 用场景渲染模板默认** `indirect_sky_ambient`）。
            ///
            /// 这是环境光的**算法轴**，与 `GetEnvProfileID()` 指向的**数据轴**
            ///（`ambient_env` 段 = IBL 句柄）配对：世界/环境决定两者，材质不再声明。
            /// 写入口在 `EnvironmentSystem::SetAmbientProvider()`（转发到
            /// `EnvironmentManager::SetAmbientProvider`）；渲染侧（`RenderCollectSystem`
            /// 解析材质程序）只**读**这里。
            ///
            /// 返回**引用**：有效期到该 profile 被改/归还为止（一帧内稳定，逐帧现取即可）。
            const AnsiString& GetAmbientProviderName() const;

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
                return render_data.IsFallbackCamera(camera);
            }

            CameraComponent* GetFallbackCamera() const { return render_data.GetFallbackCamera(); }

            // ---- A1 车道等待列表 ----

            /// 设定下一次提交的等待列表（RenderTo / 主帧收尾用）
            void SetSubmitWaits(const graph::SemaphoreSubmit *waits,uint32_t count) { lane_waits.SetSubmitWaits(waits,count); }
            /// 累积本帧离屏 RT 的车道值（主帧提交 await 它们）
            void AddFrameLaneWait(graph::Semaphore *lane,uint64_t value) { lane_waits.AddFrameLaneWait(lane,value); }
            /// 把当前累积复制为下一次提交的等待列表
            void SetSubmitWaitsFromFrameLanes() { lane_waits.SetSubmitWaitsFromFrameLanes(); }
            /// 提交系统取用等待列表；is_main_frame=true 时同时收尾清空本帧累积
            const graph::SemaphoreSubmit *TakeSubmitWaits(uint32_t &count,bool is_main_frame) { return lane_waits.TakeSubmitWaits(count,is_main_frame); }

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

            /// Clear and release all registered render pipelines
            void ClearPipelines();

            /// Resource naming prefix for hierarchical GPU resource tracking
            /// Example: "RenderToTexture:OffscreenRT:IndirectDrawBuffer"
            void SetResourceNamePrefix(const AnsiString& prefix) { resource_name_prefix = prefix; }
            void SetResourceNamePrefix(const char* prefix) { resource_name_prefix = prefix; }
            void SetResourceNamePrefix(const std::string& prefix) { resource_name_prefix = prefix.c_str(); }
            const AnsiString& GetResourceNamePrefix() const { return resource_name_prefix; }

            // ── IECSEntityObserver 接口实现 ──
            void OnEntityDestroyed(EntityID id) override;
            void OnEntityRowsChanged(const Entity *entity, ComponentType type) override;
            void OnSceneStructureDirty() override;

            // ── T2-c：组件挂载 / 卸载 / 收集（世界侧唯一入口；实体只持行号）──
            //   由基类 ECSCoreContext 统一提供。

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

            /// 实体 → 材质授权记录 `MaterialData`（世界级旁表，非组件；无则 nullptr）。
            /// **stage B 会换成值类型句柄**（与 TransformAccessor / BoundingBoxAccessor 同构），
            /// 届时不再返回裸指针。
            MaterialData* GetMaterialData(EntityID owner);
            const MaterialData* GetMaterialData(EntityID owner) const;
            MaterialData* GetMaterialData(Entity* owner);
            const MaterialData* GetMaterialData(const Entity* owner) const;

            /// 实体 → 材质授权记录（无则在世界级旁表 `MaterialAuthoringSideTable` 建条目）。
            /// 2026-11 退役：不再挂组件、不占行池；渲染/作者侧需要"一定有授权数据"时用。
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

            WorldSpatialState&       GetSpatialState()       { return spatial_state; }
            const WorldSpatialState& GetSpatialState() const { return spatial_state; }

            /// Get world-level BoundingBoxDataStorage
            BoundingBoxDataStorage* GetBoundingBoxStorage() { return spatial_state.GetBoundingBoxStorage(); }
            const BoundingBoxDataStorage* GetBoundingBoxStorage() const { return spatial_state.GetBoundingBoxStorage(); }

            /// 世界级可见性存储（渲染热路径做 O(1) 查询；不可见集合 + 祖先链语义）
            VisibilityDataStorage* GetVisibilityStorage() { return spatial_state.GetVisibilityStorage(); }
            const VisibilityDataStorage* GetVisibilityStorage() const { return spatial_state.GetVisibilityStorage(); }

            // ── T3b：实体行表（`EntityGPU`）访问器与两条写入钩子 ──
            EntityGPUTable&       GetEntityGPUTable()       { return entity_gpu_table; }
            const EntityGPUTable& GetEntityGPUTable() const { return entity_gpu_table; }

            /// 实体销毁通知：清 `Active`。
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
            LightRowTable&       GetLightRowTable()       { return render_data.GetLightRowTable(); }
            const LightRowTable& GetLightRowTable() const { return render_data.GetLightRowTable(); }

            // ── T6b/P4：影子数据（`ShadowInfo`，世界私有）──────────────────────────
            /// 编辑本世界的影子数据（CPU 权威；GPU 侧由 `CommitShadowInfo` 在提交点写本帧槽）
            graph::ShadowInfo       *EditShadowInfo()       { return render_data.EditShadowInfo(); }
            // 2026-10-10（审计 §1.1）：只读 `GetShadowInfo()` 已删除 —— 它唯一的调用者是同样零外部调用者的
            // `MainLightShadowSystem::GetShadowInfo()`（一并删除）；读影子数据请经 `EditShadowInfo()`。

            const graph::ShadowInfo &GetShadowInfo() const  { return render_data.GetShadowInfo(); }
            void SetShadowInfo(const graph::ShadowInfo &info) { render_data.SetShadowInfo(info); }

            /// 提交点（交换链帧 acquire 之后）把 CPU 权威的影子数据写进**本帧槽**的设备副本。
            void CommitShadowInfo(const uint32_t frame_slot);

            /// 组件卸载 / 全卸后重刷 `type[]/row[]`（由 `Entity` 的卸载路径调用）
            void RefreshEntityRow(const Entity *entity)
            {
                entity_gpu_table.Refresh(entity);
                entity_gpu_table.VerifyAfterWrite(entity, "ECSContext::RefreshEntityRow");   // §2.2 问题 2(a)：写入侧对拍（debug 面）
            }

            /// 设置实体可见性 —— **唯一真值落在这里**（不再有组件内的第二份 bool）。
            /// **边界换算的唯一位置**：① 缺席集（负向）与 ② 实体行位（正向、**扁平**）都按
            /// **传入值**落，**不重算**。两条错法显式禁止：① 用 `IsDirectlyInvisible` 冒充
            /// `IsInvisible`（漏继承 ⇒ 后代被误判可见）；② 在这里「顺手传播」到后代
            ///（只写自己 ⇒ 行位与含继承的权威失步）。权威链见 doc/visibility-authority-chain.md。
            void SetEntityVisible(EntityID id, bool visible)
            {
                if (!GetEntity(id))
                    return;

                spatial_state.SetEntityVisible(id, visible);

                // T3b：镜像到实体行（唯一真值仍是 VisibilityDataStorage；本表是派生视图）
                entity_gpu_table.SetVisible(id, visible);
                entity_gpu_table.VerifyAfterWrite(GetEntity(id), "ECSContext::SetEntityVisible");   // §2.2 问题 2(a)
            }

            /// 实体是否可见：O(1) 直查不可见集合 + 祖先链上溯（祖先不可见 ⇒ 后代不可见）
            bool IsEntityVisible(EntityID id) const
            {
                return spatial_state.IsEntityVisible(id);
            }

            // ── 变换名单的**结构性遍历**（唯一正确方式）─────────────────────────
            //   名单里存在**空洞**：销毁 / Mobility 换边会留下 `INVALID_TRANSFORM_ID`
            //   （空洞记 INVALID_TRANSFORM_ID，注册优先复用空洞）。
            //   裸遍历 `GetMovableTransforms()/GetStaticTransforms()` 会把空洞当成有效 id 传下去，
            //   以 (uint32)-1 索引行存储的平行数组 ⇒ 越界崩溃（2026-10-02 实测）。因此：
            //   **新增消费者一律用这两个入口**（内部自动跳洞，调用方不可能漏）。

            template<typename Fn>
            void ForEachMovableTransform(Fn &&fn) const
            {
                spatial_state.ForEachMovableTransform(std::forward<Fn>(fn));
            }

            template<typename Fn>
            void ForEachStaticTransform(Fn &&fn) const
            {
                spatial_state.ForEachStaticTransform(std::forward<Fn>(fn));
            }

            /// 实体 → 变换行（无变换时返回 INVALID_TRANSFORM_ID）
            TransformID GetTransformID(EntityID owner) const;

            /// 变换访问器（薄句柄；读写直落本世界的存储）
            TransformAccessor GetTransform(TransformID id);

            const ValueArray<TransformID>& GetStaticTransforms() const { return spatial_state.GetStaticTransforms(); }
            const ValueArray<TransformID>& GetMovableTransforms() const { return spatial_state.GetMovableTransforms(); }

            /// 变换的**稳定槽位**（B1b）：命中返回 true 并写 `out_slot`。
            /// 槽位是 L2W 行号的唯一来源（静态 `1+slot`、动态 `1+预算static + frame×预算dynamic + slot`），
            /// 一旦分配就不再随名单顺序变动（空洞复用，不整体平移）。
            bool TryGetTransformSlot(TransformID id, bool movable, uint32_t &out_slot) const;

            /// 该段**活着的**变换数（≠ 槽位表长度：表里有空洞）
            uint32_t GetLiveTransformCount(bool movable) const;

            /// Mobility 换边计数（每次换段 = 旧行号作废 ⇒ 计数增；可观测/契约用）
            uint32_t GetTransformRowMoveCount() const { return spatial_state.GetTransformRowMoveCount(); }

            /// 实体 → 变换访问器（无变换时返回无效 accessor）
            TransformAccessor GetTransformByEntity(EntityID owner)
            {
                return GetTransform(GetTransformID(owner));
            }

            /// Get world-level TransformDataStorage
            TransformDataStorage* GetTransformStorage() { return spatial_state.GetTransformStorage(); }
            const TransformDataStorage* GetTransformStorage() const { return spatial_state.GetTransformStorage(); }

            WorldRenderData&       GetRenderData()       { return render_data; }
            const WorldRenderData& GetRenderData() const { return render_data; }

            WorldGPUResources&       GetGPUResources()       { return gpu_resources; }
            const WorldGPUResources& GetGPUResources() const { return gpu_resources; }

            /// A3 材质变体表（材质运行期 program/recipe 按静态键去重；见 support/MaterialVariantTable.h）
            MaterialVariantTable* GetMaterialVariantTable() { return render_data.GetMaterialVariantTable(); }
            const MaterialVariantTable* GetMaterialVariantTable() const { return render_data.GetMaterialVariantTable(); }

            /// A4 材质运行期表（共享行 interned + CoW + refcount + 世代；见 support/MaterialRuntimeTable.h）
            /// B5-b 运行期管线旁表（键 = 实体；按 pass 比对）
            MaterialRuntimePipelineTable& GetMaterialRuntimePipelineTable() { return render_data.GetMaterialRuntimePipelineTable(); }
            const MaterialRuntimePipelineTable& GetMaterialRuntimePipelineTable() const { return render_data.GetMaterialRuntimePipelineTable(); }


            MaterialAuthoringSideTable* GetMaterialAuthoringTable() { return render_data.GetMaterialAuthoringTable(); }
            const MaterialAuthoringSideTable* GetMaterialAuthoringTable() const { return render_data.GetMaterialAuthoringTable(); }

            MaterialRuntimeTable* GetMaterialRuntimeTable() { return render_data.GetMaterialRuntimeTable(); }
            const MaterialRuntimeTable* GetMaterialRuntimeTable() const { return render_data.GetMaterialRuntimeTable(); }

            ModelTemplateMaterialTable* GetModelTemplateMaterialTable() { return render_data.GetModelTemplateMaterialTable(); }
            const ModelTemplateMaterialTable* GetModelTemplateMaterialTable() const { return render_data.GetModelTemplateMaterialTable(); }

            /// 实体 → 材质运行期 slot（无则创建）。命名向 `GetOrCreateMaterialData` 看齐。
            /// ⚠ stage B 会换成**值类型句柄**（与 TransformAccessor / BoundingBoxAccessor 同构），
            /// 届时不再返回引用。
            MaterialRuntimeSlot& GetOrCreateMaterialRuntimeSlot(EntityID owner)
            {
                return render_data.GetOrCreateMaterialRuntimeSlot(owner);
            }

            /// 指针版（作者/引擎手里已有实体）
            MaterialRuntimeSlot& GetOrCreateMaterialRuntimeSlot(Entity *owner)
            {
                return render_data.GetOrCreateMaterialRuntimeSlot(owner ? owner->GetEntityID() : EntityID{});
            }

            /// 实体 → 材质运行期 slot（无则 nullptr）
            MaterialRuntimeSlot* GetMaterialRuntimeSlot(EntityID owner)
            {
                return render_data.GetMaterialRuntimeSlot(owner);
            }

            MaterialRuntimeSlot* GetMaterialRuntimeSlot(Entity *owner)
            {
                return owner ? render_data.GetMaterialRuntimeSlot(owner->GetEntityID()) : nullptr;
            }

            /// Get 世界私有相机行存储（相机 = 世界级观察者数据；0 号槽 = 本世界默认相机）
            CameraInfoStorage* GetCameraInfoStorage() { return render_data.GetCameraInfoStorage(); }
            const CameraInfoStorage* GetCameraInfoStorage() const { return render_data.GetCameraInfoStorage(); }

            /// 槽拥有者（CameraSlotGuard / 归还挂钩）用：弱引用 ⇒ 世界先销毁时 lock 失败 = 安全 no-op
            std::weak_ptr<CameraInfoStorage> GetCameraInfoStorageWeak() const { return render_data.GetCameraInfoStorageWeak(); }

            /// 世界地址表基址（**本帧槽**）：写入 pc_root.addr_world_addresses（0 = 尚未创建）
            uint64_t GetWorldAddressesAddress(uint32_t frame_slot) const;

            /// 把本世界的 world 私有地址（相机行表 / 实体 4-ID 表等）同步进**当前帧槽**的世界表。
            /// 槽 = 当前 RT 的帧槽（主帧 [0,4) / 离屏 [4,8)），由 SetFrameIndex 触发。
            bool SyncWorldAddresses();

            /// 本世界的 EnvironmentManager（经 RenderContext/GraphicsContext 解析；不可用时 nullptr）
            hgl::graph::EnvironmentManager *ResolveEnvManager() const;

        public:

            /// Ensure CameraSystem exists in this context.
            /// If created after context activation, it is initialized immediately.
            std::shared_ptr<CameraSystem> EnsureCameraSystem();

            /// Get systems by render element type name (e.g., "Primitive", "Text", "SkySphere")
            void GetSystemsByElementType(const std::string& element_type, std::vector<std::shared_ptr<System>>& out_systems) const;

            /// Get all registered render element type names
            void GetAllRenderElementTypes(std::vector<std::string>& out_element_types) const;

            /// 启用/禁用某个系统组的全部系统（A2：按枚举 O(1)，不查字符串）
            void SetSystemGroupEnabled(SystemGroupID group, bool enabled) override;

            /// 该组的安装器是否已在本世界跑过（A2：枚举）
            bool IsSystemGroupInstalled(SystemGroupID group) const;

            /// 标记该组已安装（幂等）；组快照不在此建立，由 SetSystemGroupEnabled 首次启用时惰性重建
            void MarkSystemGroupInstalled(SystemGroupID group);

            /// 重置组件驱动的组状态（计数 / 已安装 / 快照）—— 世界 Shutdown 用
            void ResetComponentDrivenGroups();

        public:
            /**
             * 本世界是否还有图元卡在必需资源上（异步读盘在途 / 离屏 RT 尚未产出）。
             *
             * 生产方（如 `OffscreenWorld`）据此判断「刚画的那一帧内容是否完整」。
             */
            bool HasPendingRenderableResources() const;

            RenderFrameCache& GetRenderFrameCache() { return render_runner.GetRenderFrameCache(); }
            const RenderFrameCache& GetRenderFrameCache() const { return render_runner.GetRenderFrameCache(); }
            RenderSystemCore* GetRenderCore() const { return render_runner.GetRenderCore(); }
        };
    }//namespace ecs
}//namespace hgl
