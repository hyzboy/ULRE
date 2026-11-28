#pragma once

#include<hgl/vk/VK.h>
#include<hgl/ecs/core/ECSCoreContext.h>
#include<hgl/ecs/core/entity/Object.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/core/system/System.h>
#include<hgl/ecs/core/system/SystemRegistry.h>
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
#include<hgl/ecs/support/render/WorldEntityRenderState.h>
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
#include<hgl/ecs/support/render/WorldRenderPipelineRegistry.h>
#include<hgl/ecs/support/render/WorldRenderEnvironment.h>
#include<hgl/ecs/support/material/MaterialVariantTable.h>
#include<hgl/ecs/support/material/MaterialRuntimeTable.h>
#include<hgl/ecs/support/material/ModelTemplateMaterialTable.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/support/material/MaterialAuthoringSideTable.h>
#include<hgl/ecs/core/entity/EntityManager.h>
#include<hgl/graph/ubo/EnvironmentInfo.h>
#include<hgl/graph/render/RenderTargetObserver.h>
#include<hgl/log/Log.h>
#include<functional>
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
        class ECSContext : public ECSCoreContext, public graph::RenderTargetObserver, public IWorldSpatialObserver
        {
        private:
            friend class WorldRenderRunner;
            void ConfigureSystem(System *system) override;

        public:

            ECSContext(const char* name) : ECSContext(AnsiString(name)) {}
            ECSContext(const AnsiString& name = "World");
            ~ECSContext() override;

            void OnTransformRegistered(TransformID id, bool isMovable) override;
            void OnTransformMembershipChanged(TransformID id) override;
            void OnTransformChanged(TransformID id, uint32_t change_mask) override;
            void OnEntityVisibilityChanged(EntityID id, bool visible) override;

        private:
            OBJECT_LOGGER
            friend struct RenderPassSetupTestAccess;
            // 实体索引的 CPU 渲染派生表；Shutdown 先清理实体，再销毁此状态域。
            WorldEntityRenderState entity_render_state;

            // 空间域聚合状态（变换槽位与行存储、包围盒行存储、可见性集合）
            WorldSpatialState spatial_state;

            // CPU 渲染数据底座（光源行表、阴影参数、材质表组、世界相机表与常驻 Fallback）
            WorldRenderData render_data;

            // GPU 侧资源与设备镜像表（世界地址表、实体行表、4-ID 表、光源表、影子表）
            WorldGPUResources gpu_resources;

            // 渲染执行器与调度器（渲染帧生命周期、Pass 调度、阶段驱动与帧缓存）
            WorldRenderRunner render_runner;

            WorldRenderPipelineRegistry render_pipeline_registry;

            // ========== GPU 设备和资源管理（Phase 1 新增） ==========

            /// GPU 设备（从旧集中式入口迁移来）
            WorldRenderEnvironment render_environment;

        private:

            void RunSystemUpdate(System *system, float deltaTime) override;
            uint64_t ResolveTransformDeviceAddress() const;
            uint64_t ResolveModelExpansionAddress() const;
            bool SyncGPUResourceAddresses();

            /// 组件行增/删 ⇒ 系统组计数（A2/P-4 的挂卸驱动路径；组归属取自类型表）
            void OnComponentRowAdded(ComponentType type) override;
            void OnComponentRowRemoved(ComponentType type) override;
            void OnEntityCreated(EntityID id) override
            {
                entity_render_state.ActivateEntityRow(id);
                MarkTopologyChanged(TopologyChange::EntityStructure);
            }
            void OnEntityDestroying(EntityID id) override;
            void OnEntityCleanup(EntityID id) override;

            void ExecuteRenderGraphPasses(const RenderGraph& graph,
                                          float deltaTime,
                                          const std::function<void(float)> &pre_render);

            // ========== 帧驱动内部编排（应用层勿直接调用） ==========
            // 公共入口只有 Render(dt, pre_render) 与 RenderTo(rt, clear, dt)；
            // 以下相位方法仅由上述两者与托管帧流程按 ExecutionPhase 顺序编排。

            void Render(float deltaTime, const RenderGraph& graph, const std::function<void(float)> &pre_render);

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

            /// Handle render target resize
            void OnResize(const VkExtent2D &extent);

            void SetFrameIndex(const uint32_t index);

            /// 声明拓扑变更（写入方调用；见 `TopologyChange` 各类别的注释）
            void MarkTopologyChanged(TopologyChange kind, uint32_t count = 1) const
            {
                render_runner.GetMainFrameTopology().MarkTopology(kind, count);
                render_runner.MarkSceneStructureDirty();
            }

            /// 声明动态事实变更（不改拓扑，只需重新发布到 GPU 行）
            void MarkDynamicFactChanged(DynamicFactChange kind, uint32_t count = 1) const
            {
                render_runner.GetMainFrameTopology().MarkDynamicFact(kind, count);
            }

            WorldRenderRunner &Runner() { return render_runner; }
            const WorldRenderRunner &Runner() const { return render_runner; }

            // ========== GPU 设备和资源接口（Phase 1 新增） ==========

            /// 获取 GPU 设备
            WorldRenderEnvironment &Environment() { return render_environment; }
            const WorldRenderEnvironment &Environment() const { return render_environment; }

            void OnSwapchainTargetChanged(hgl::graph::IRenderTarget *target) override;
            void OnRenderTargetResized(hgl::graph::IRenderTarget *target, uint32 width, uint32 height) override;

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
            /// 选本世界的 env profile；`take_ownership=true` 表示"本世界创建的"（Shutdown 时归还）。
            void SetEnvProfileID(graph::EnvProfileID id, bool take_ownership = false);

            /// 建一个**本世界拥有**的 env profile 并立刻选用（Shutdown 时自动 `Release`）。
            /// 返回 `kEnvProfileInvalid` 表示管理不可用 / 创建失败。
            graph::EnvProfileID CreateEnvProfile(const AnsiString &name,
                                                 const graph::EnvironmentInfo &init = {});

            /// 本世界生效 profile 的 **ambient provider 模块名**（`ambient_light_provider` 槽的
            /// GLSL 模块；**空 = 用场景渲染模板默认** `indirect_sky_ambient`）。
            ///
            /// 这是环境光的**算法轴**，与 `Environment().GetEnvProfileID()` 指向的**数据轴**
            ///（`ambient_env` 段 = IBL 句柄）配对：世界/环境决定两者，材质不再声明。
            /// 写入口在 `EnvironmentSystem::SetAmbientProvider()`（转发到
            /// `EnvironmentManager::SetAmbientProvider`）；渲染侧（`RenderCollectSystem`
            /// 解析材质程序）只**读**这里。
            ///
            /// 返回**引用**：有效期到该 profile 被改/归还为止（一帧内稳定，逐帧现取即可）。
            // ===== 相机（**世界级资源**：16 槽 × 帧槽，0 号槽恒为本世界默认相机）=====
            //
            // 「相机属于哪个世界 / 哪个槽」的**唯一真源 = 本世界的相机表**（`camera_info_storage`：
            // 行空间 + 槽账目「槽 → 宿主相机」）。组件侧**不再**持有槽号 / 归还挂钩；需要槽号一律走
            // 下面这组访问器。三级解析（0 号槽的默认相机 → 显式 `is_main_camera` → 最小 `EntityID`
            // → `(0,0,0)` 常驻 fallback）见 `CameraSystem::SelectMainCamera`，落点即"谁是 0 号槽的宿主"。

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
            CameraComponent* GetOrCreateCamera(EntityID owner, const AnsiString& name = "Camera");
            CameraComponent* GetOrCreateCamera(Entity* owner, const AnsiString& name = "Camera");

            /// Asset world registry (Phase 3)

            WorldRenderPipelineRegistry& PipelineRegistry() { return render_pipeline_registry; }
            const WorldRenderPipelineRegistry& PipelineRegistry() const { return render_pipeline_registry; }

            // ── ECSCoreContext 实体变更钩子实现 ──
            void OnEntityDestroyed(EntityID id) override;
            void OnEntityRowsChanged(const Entity *entity, ComponentType type) override;
            void OnSceneStructureDirty() override;

            // ── T2-c：组件挂载 / 卸载 / 收集（世界侧唯一入口；实体只持行号）──
            //   由基类 ECSCoreContext 统一提供。

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

            WorldSpatialState&       Spatial()       { return spatial_state; }
            const WorldSpatialState& Spatial() const { return spatial_state; }

            WorldEntityRenderState&       EntityRenderState()       { return entity_render_state; }
            const WorldEntityRenderState& EntityRenderState() const { return entity_render_state; }

            WorldRenderData&       RenderData()       { return render_data; }
            const WorldRenderData& RenderData() const { return render_data; }

            WorldGPUResources&       GPUResources()       { return gpu_resources; }
            const WorldGPUResources& GPUResources() const { return gpu_resources; }

        public:

            /// Ensure CameraSystem exists in this context.
            /// If created after context activation, it is initialized immediately.
            std::shared_ptr<CameraSystem> EnsureCameraSystem();

            /// 启用/禁用某个系统组的全部系统（A2：按枚举 O(1)，不查字符串）
            void SetSystemGroupEnabled(SystemGroupID group, bool enabled) override;


        public:
            /**
             * 本世界是否还有图元卡在必需资源上（异步读盘在途 / 离屏 RT 尚未产出）。
             *
             * 生产方（如 `OffscreenWorld`）据此判断「刚画的那一帧内容是否完整」。
             */
        };
    }//namespace ecs
}//namespace hgl
