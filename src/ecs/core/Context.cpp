#include<hgl/ecs/core/Context.h>

#include<utility>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/ecs/core/EntityManager.h>
#include<hgl/ecs/core/DefaultSystems.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/ecs/components/ShadowProxy.h>
#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/support/PrimitiveState.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/support/RenderPipelineBase.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/ecs/support/RenderItemDataStorage.h>
#include<hgl/ecs/support/DrawItemIDStorage.h>
#include<hgl/ecs/support/CameraSlotGuard.h>
#include<hgl/ecs/support/CameraInfoStorage.h>
#include<hgl/ecs/support/MaterialAuthoringSideTable.h>
#include<hgl/ecs/support/MaterialRuntimePipelineTable.h>
#include<hgl/graph/ubo/WorldAddresses.h>
#include<hgl/vk/buffer/StructView.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/ecs/systems/render/RenderSystemCore.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/MainLightShadowSystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/ecs/systems/render/SwapchainNextImageSystem.h>
#include<hgl/ecs/systems/render/SwapchainSubmitSystem.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GraphModuleManager.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/log/Log.h>
#include<hgl/object/ObjectTracker.h>
#include<hgl/time/Time.h>
#include<algorithm>
#include<cstdlib>
#include<cstdio>
#include<vector>

namespace hgl
{
    namespace ecs
    {
        ECSContext::ECSContext(const AnsiString& name)
            : Object(name)
            , entity_manager(std::make_unique<EntityManager>(1000))
            , transform_storage(std::make_unique<TransformDataStorage>())
            , bounding_box_storage(std::make_unique<BoundingBoxDataStorage>())
            , visibility_storage(std::make_unique<VisibilityDataStorage>())
            , render_item_storage(std::make_unique<RenderItemDataStorage>())
            , draw_item_id_storage(std::make_unique<DrawItemIDStorage>())
            , material_variant_table(std::make_unique<MaterialVariantTable>())
            , material_runtime_table(std::make_unique<MaterialRuntimeTable>())
            , material_authoring_side_table(std::make_unique<MaterialAuthoringSideTable>())
            , material_runtime_pipeline_table(std::make_unique<MaterialRuntimePipelineTable>())
            , camera_info_storage(std::make_shared<CameraInfoStorage>())
            , active(false)
        {
            // 可见性存储需要拿到本世界指针：祖先链上溯要回查实体与变换行
            visibility_storage->SetContext(this);

            // 变换行存储需要拿到本世界指针：行句柄（TransformAccessor）不再自带 context，
            // 一律经 storage->GetContext() 反查世界（Mobility 换边通知 / owner 实体解析走这条）。
            transform_storage->SetContext(this);

            // 材质运行期表需要拿到本世界指针：共享行回收时退休其纹理配置池行
            // （GPU 侧绑定随行存亡）；单测里没有世界，钩子自动 no-op。
            material_runtime_table->SetContext(this);
        }

        ECSContext::~ECSContext()
        {
            Shutdown();
        }

        bool ECSContext::Initialize(hgl::graph::VulkanDevice* device, hgl::graph::IRenderTarget* target)
        {
            // W3 合并：原 InitializeGraphics 的 GPU 绑定 + 原 Initialize 的系统初始化
            if (!device || !target)
                return false;

            gpu_device = device;
            render_target = target;

            // Env 随世界（C2）：profile 未显式设置时**按需解析本世界 RT 的 env_profile**
            // （见 GetEnvProfileID）——作者侧可能在世界创建之后才 SetEnvironmentProfile，
            // 所以这里不缓存快照。sky / shadow 地址按它写本世界地址表。
            env_profile_explicit = false;
            env_profile_owned = false;

            // 世界私有相机行存储（相机 = 世界级观察者数据）：16 槽 × HGL_FRAME_SLOT_TOTAL 帧槽，
            // 0 号槽恒为本世界默认相机。定稿见 doc/world-addresses-and-camera-model-plan.md §2。
            if (camera_info_storage && !camera_info_storage->IsReady())
            {
                if (!camera_info_storage->Create(device, GetName().c_str()))
                {
                    LogError("[ECSContext::Initialize] 世界相机行存储（CameraInfoStorage）创建失败");
                    return false;
                }
            }

            // 世界地址表（WorldAddresses，SSBO + 持久 BDA）：表内是本世界的 world 私有地址，
            // 基址经 pc_root.addr_world_addresses 下发 —— 多世界渲染只换这一个指针。
            if (!InitializeWorldAddressesTable())
            {
                LogError("[ECSContext::Initialize] 世界地址表（WorldAddresses）创建失败");
                return false;
            }

            // Propagate device to RenderBufferUploadSystem if it was registered first
            if (auto upload_system = GetSystem<RenderBufferUploadSystem>())
                upload_system->SetDevice(gpu_device);

            // A5b：渲染组件删除后，图元枚举改按 GeometryData（精确类型）——
            // 不再需要基类查询注册（原图元组件的基类查询注册已随该类删除）。

            // Ensure TransformSystem is registered and bound to this world
            {
                auto transform_system = GetSystem<TransformSystem>();
                if (!transform_system)
                {
                    transform_system = RegisterSystem<TransformSystem>();
                }

                if (transform_system)
                {
                    transform_system->SetWorld(this);
                }
            }

            // Ensure RenderBufferUploadSystem is always present — it is infrastructure,
            // not a feature system. Every app with staged GPU buffers needs it.
            {
                auto upload_system = GetSystem<RenderBufferUploadSystem>();
                if (!upload_system)
                    upload_system = RegisterSystem<RenderBufferUploadSystem>();

                if (upload_system)
                {
                    upload_system->SetWorld(this);
                    upload_system->SetDevice(gpu_device);
                }
            }

                SortTickSystems();
                SortRenderSystems();

            // Initialize all systems
                for (auto& entry : tick_system_order)
                {
                    if (entry.system)
                    {
                        entry.system->Initialize();
                    }
                }

                for (auto& entry : render_system_order)
                {
                    if (entry.system)
                    {
                        entry.system->Initialize();
                    }
                }

            active = true;
            return true;
        }

        std::shared_ptr<CameraSystem> ECSContext::EnsureCameraSystem()
        {
            auto camera_system = GetSystem<CameraSystem>();
            if (!camera_system)
            {
                camera_system = RegisterSystem<CameraSystem>(this);
            }

            if (!camera_system)
                return nullptr;

            if (active)
            {
                camera_system->Initialize();
            }

            return camera_system;
        }

        RenderPipelineBase* ECSContext::GetRenderPipeline(const char* name)
        {
            if (!name) return nullptr;
            return GetRenderPipeline(std::string(name));
        }

        RenderPipelineBase* ECSContext::GetRenderPipeline(const std::string& name)
        {
            auto it = render_pipelines.find(name);
            if (it != render_pipelines.end())
                return it->second.get();
            return nullptr;
        }

        RenderPipelineBase* ECSContext::GetRenderPipeline(const AnsiString& name)
        {
            return GetRenderPipeline(std::string(name.c_str()));
        }

        void ECSContext::RegisterRenderPipeline(const char* name, std::unique_ptr<RenderPipelineBase> pipeline)
        {
            if (!name || !pipeline) return;
            RegisterRenderPipeline(std::string(name), std::move(pipeline));
        }

        void ECSContext::RegisterRenderPipeline(const std::string& name, std::unique_ptr<RenderPipelineBase> pipeline)
        {
            if (!pipeline)
                return;

//            LogDebug("[ECS] Registering render pipeline: %s", name.c_str());
            render_pipelines[name] = std::move(pipeline);
        }

        void ECSContext::RegisterRenderPipeline(const AnsiString& name, std::unique_ptr<RenderPipelineBase> pipeline)
        {
            RegisterRenderPipeline(std::string(name.c_str()), std::move(pipeline));
        }

        void ECSContext::Shutdown()
        {
            if (shutdown_in_progress)
                return;

            shutdown_in_progress = true;

            if (auto *device = GetGPUDevice())
                device->WaitIdle();

            // Release support pipelines early while graphics managers are still valid.
            // AppFramework destroys GraphicsContext before deleting ECSContext, so
            // deferring this to ECSContext destructor can access dangling pointers.
            // Text/Primitive specific support-pipeline members removed; all pipelines live in render_pipelines.

            // Release all registered render pipelines
            render_pipelines.clear();

            // Release render-frame items first  (only clears renderItems, keeps materialBatches for reuse)
            render_frame_cache.renderItems.clear();
            render_frame_cache.cameraInfo = nullptr;
            render_frame_cache.renderableCount = 0;

            if (!active)
            {
                // Even when inactive, clear entities/components now so component OnDetach
                // does not run later during member destruction after maps are already destroyed.
                if (entity_manager)
                {
                    LogDebug("[ECSContext] Shutdown(inactive) - clearing entities before member teardown");
                    entity_manager->Clear();
                }

                tick_systems.Clear();
                render_systems.Clear();
                tick_system_order.clear();
                render_system_order.clear();
                ClearComponentPools();
                systems_by_element_type.clear();
                tick_dependencies.Clear();
                render_dependencies.Clear();
                ResetComponentDrivenGroups();
                static_transforms.clear();
                movable_transforms.clear();

                LogDebug("[ECSContext] Shutdown(inactive) - releasing %zu material batches",
                         render_frame_cache.materialBatches.GetCount());
                render_frame_cache.materialBatches.Clear();
                shutdown_in_progress = false;
                return;
            }

            // 先标记 inactive，避免系统/组件在 Shutdown 过程中触发重入时再走完整清理路径
            active = false;

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

            ClearComponentPools();
            ResetComponentDrivenGroups();
            static_transforms.clear();
            movable_transforms.clear();

            // Finally, clear materialBatches after all systems/entities are destroyed
            LogDebug("[ECSContext] Shutdown - releasing %zu material batches",
                     render_frame_cache.materialBatches.GetCount());
            render_frame_cache.materialBatches.Clear();
            LogDebug("[ECSContext] Shutdown - material batches cleared");

            // 相机槽拥有者的安全网由 **弱引用** 提供：`CameraSlotGuard` 只持
            // `weak_ptr<CameraInfoStorage>`，世界先销毁时 lock() 失败 ⇒ 析构自动 no-op，不需要在这里
            // 逐个 Detach（世界可能从未 Initialize，Shutdown 会走早退分支 —— 那正是悬垂指针的来源）。
            // 槽本身也不需要逐个归还：`camera_info_storage->Reset()` 会把槽账目整表清空。

            // Env 随世界（C2）：**本世界创建/拥有的** profile 在这里归还（内置 default 永不归还）。
            // 释放顺序在世界地址表之前——表里还留着它的地址，但表随后即销毁。
            if (env_profile_owned)
            {
                if (auto *env_manager = ResolveEnvManager())
                    env_manager->Release(env_profile);
            }
            env_profile = graph::kEnvProfileDefault;
            env_profile_explicit = false;
            env_profile_owned = false;

            // 世界地址表释放（StructView 非拥有 ⇒ buffer 走 BufferManager 归还）
            if (world_addresses_table)
            {
                delete world_addresses_table;
                world_addresses_table = nullptr;
            }

            if (world_addresses_buffer)
            {
                if (auto *gc = GetGraphicsContext())
                {
                    if (auto *bm = gc->GetBufferManager())
                    {
                        bm->Release(world_addresses_buffer);

                        // T3c：实体行表随世界释放（`StructView` 是裸指针、生命周期随 buffer，与地址表同办）
                        if (entity_gpu_buffer)
                            bm->Release(entity_gpu_buffer);

                        // T3d-3a：实体 4-ID 表随世界释放（同一纪律）
                        if (entity_render_id_buffer)
                            bm->Release(entity_render_id_buffer);

                        // T6b：光源行表随世界释放（同一纪律）
                        if (light_row_buffer)
                            bm->Release(light_row_buffer);

                        // T6b/P4：影子数据表随世界释放（同一纪律）
                        if (shadow_info_buffer)
                            bm->Release(shadow_info_buffer);
                    }
                }

                world_addresses_buffer = nullptr;

                entity_gpu_buffer = nullptr;
                entity_gpu_view = nullptr;
                entity_gpu_table_addr = 0;

                entity_render_id_buffer = nullptr;
                entity_render_id_view = nullptr;
                entity_render_id_table_addr = 0;

                light_row_buffer = nullptr;
                light_row_view = nullptr;
                light_row_table_addr = 0;

                shadow_info_buffer = nullptr;
                shadow_info_view = nullptr;
                shadow_info_addr = 0;
            }

            world_addresses_addr = 0;

            // 常驻 fallback 相机随世界销毁（不是 Entity 组件 ⇒ 不走 entity_manager->Clear()）；
            // 它若正占着 0 号槽，槽账目已随相机表一起销毁 —— 不再有第二份 default_camera 要清。
            fallback_camera = nullptr;

            shutdown_in_progress = false;
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

            // 原每帧全实体 × 全组件的 OnUpdate 虚分发已删除——其唯一有效实现
            // （fixed_pixel 等像素缩放）由 gizmo 的
            // TransformGizmoSystem 在 TickPostCamera 相位经
            // SetFixedPixelSizingContext 即时重算（本就是主通道）。
            // 逻辑更新归 tick 系统，不归组件。

            if (auto input_system = GetSystem<InputSystem>())
            {
                input_system->EndFrame();
            }
        }

        hgl::graph::GraphicsContext* ECSContext::GetGraphicsContext()
        {
            if (!graphics_context && render_context)
                graphics_context = render_context->GetGraphicsContext();
            return graphics_context;
        }

        const hgl::graph::GraphicsContext* ECSContext::GetGraphicsContext() const
        {
            if (graphics_context)
                return graphics_context;
            return render_context ? render_context->GetGraphicsContext() : nullptr;
        }

        void ECSContext::SetFrameIndex(const uint32_t index)
        {
            frame_index = index;

            // 世界私有直推：只推进本世界 TransformSystem 的 L2W ring 帧索引
            // （旧静态广播会触达所有世界的 buffer，且静态表析构不摘除留悬空）
            auto ts = GetSystem<TransformSystem>();
            if (ts)
                if (auto *tb = ts->GetTransformBuffer())
                    tb->SetFrameIndex(index);

            // 帧槽变了 ⇒ 本世界的地址表换到本槽（相机行表 / 渲染项表 / DrawItemID 表随世界，
            // 每个 pass 各写自己的槽，不与在途帧/其它世界互踩）。
            SyncWorldAddresses();
        }

        uint32_t ECSContext::GetActiveCameraRow() const
        {
            // 槽号非法（本帧尚未解析出相机认领的槽 / 槽耗尽）⇒ 退回 **0 号槽**（默认相机 / 常驻 fallback）：
            // 着色器拿到的永远是一个合法行，而不是按 INVALID_SLOT 算出的越界行（会读到别的世界的存储）。
            const uint32_t slot = CameraInfoStorage::IsValidSlot(active_camera_id)
                                    ? active_camera_id
                                    : CameraInfoStorage::kDefaultCameraSlot;

            // 行号 = 相机槽 × 帧槽总数 + 帧槽（世界内算术，见 CameraInfoStorage::CameraRow）
            return CameraInfoStorage::CameraRow(slot, frame_index);
        }

        const graph::CameraInfo* ECSContext::GetActiveCameraInfo()
        {
            auto camera_system = GetSystem<CameraSystem>();
            return camera_system ? camera_system->GetActiveCameraInfo() : nullptr;
        }

        // ==== 相机（世界级资源：16 槽 × 帧槽，0 号槽恒为本世界默认相机）====
        // 「相机属于哪个世界 / 哪个槽」的唯一真源 = camera_info_storage 的槽账目（槽 → 宿主弱引用）。

        CameraComponent* ECSContext::GetCamera(const uint32_t slot) const
        {
            return camera_info_storage ? camera_info_storage->GetSlotOwner(slot) : nullptr;
        }

        CameraComponent* ECSContext::GetDefaultCamera() const
        {
            // 默认相机 = **0 号槽的宿主**（没有第二份 weak_ptr 记账：相机销毁 ⇒ 弱引用失效 ⇒
            // 这里自然返回 nullptr，下一次三级解析重新指名）。
            return GetCamera(CameraInfoStorage::kDefaultCameraSlot);
        }

        bool ECSContext::SetDefaultCamera(const std::shared_ptr<CameraComponent>& camera)
        {
            if (!camera_info_storage)
                return false;

            // 0 号槽唯一：改绑即顶替原宿主（常驻 fallback 被实体相机顶替时**不需要**额外动作）。
            return camera_info_storage->BindCameraSlot(CameraInfoStorage::kDefaultCameraSlot, camera);
        }

        uint32_t ECSContext::GetCameraSlot(const CameraComponent* camera) const
        {
            if (!camera || !camera_info_storage)
                return CameraComponent::kInvalidSlot;

            return camera_info_storage->FindSlot(camera);
        }

        namespace
        {
            // 世界级「实体 → 组件」访问器的**核心**：调用方手里已有实体（`Entity*`）时走这里，
            // 不再做 `GetEntity(EntityID)` 那次多余往返 —— `EntityID` 版只负责解析一次后转调。
            // 两种句柄是**不同的句柄，不是兼容层**：ID 版给只持有 ID 的引擎内部代码，
            // 指针版给持有实体的作者/系统代码。
            template<typename T>
            inline T *ComponentOfEntity(Entity *entity)
            {
                return entity ? entity->GetComponent<T>() : nullptr;
            }

            template<typename T>
            inline const T *ComponentOfEntity(const Entity *entity)
            {
                return entity ? entity->GetComponent<T>() : nullptr;
            }

            template<typename T, typename... Args>
            inline T *GetOrAddComponentOfEntity(Entity *entity, Args&&... args)
            {
                if (!entity)
                    return nullptr;

                T *component = entity->GetComponent<T>();

                if (!component)
                    component = entity->AddComponent<T>(std::forward<Args>(args)...);

                return component;
            }
        }

        // 实体 → 相机（世界级读取口；与 GetMaterialData / GetGeometryData 同族）。
        // 相机是**世界级资源**（槽账目在 camera_info_storage），实体只是宿主：
        // 示例/系统取相机一律走这里，不走 Entity::GetComponent<CameraComponent>
        // （那会形成第二个读取入口 ⇒ 组件级旧读法 vs 世界级访问器成双真值）。
        const CameraComponent* ECSContext::GetCameraByEntity(const EntityID owner) const
        {
            return ComponentOfEntity<CameraComponent>(GetEntity(owner));
        }

        CameraComponent* ECSContext::GetCameraByEntity(const EntityID owner)
        {
            return ComponentOfEntity<CameraComponent>(GetEntity(owner));
        }

        const CameraComponent* ECSContext::GetCameraByEntity(const Entity *owner) const
        {
            return ComponentOfEntity<CameraComponent>(owner);
        }

        CameraComponent* ECSContext::GetCameraByEntity(Entity *owner)
        {
            return ComponentOfEntity<CameraComponent>(owner);
        }

        CameraComponent* ECSContext::GetOrCreateCamera(const EntityID owner, const std::string& name)
        {
            return GetOrAddComponentOfEntity<CameraComponent>(GetEntity(owner), name);
        }

        CameraComponent* ECSContext::GetOrCreateCamera(Entity *owner, const std::string& name)
        {
            return GetOrAddComponentOfEntity<CameraComponent>(owner, name);
        }

        bool ECSContext::ReleaseCameraSlot(const CameraComponent* camera)
        {
            if (!camera_info_storage)
                return false;

            return camera_info_storage->ReleaseCameraSlot(camera);
        }

        bool ECSContext::EnsureCameraSlot(CameraComponent* camera, const bool is_default)
        {
            if (!camera || !camera_info_storage)
                return false;

            // 槽的宿主必须是 shared_ptr 持有者（槽账目 = 宿主弱引用）：实体组件（注册表）/ 作者 /
            // 系统（CameraSlotGuard）都满足；栈上或裸 new 的相机拿不到槽 ⇒ fail-fast 留痕
            // （宁可它不出图，也不能按越界行号写到别的世界去）。
            auto owner = std::static_pointer_cast<CameraComponent>(camera->weak_from_this().lock());
            if (!owner)
            {
                GLogError("[ECSContext] 相机 \"%s\" 没有 shared_ptr 宿主，无法认领相机槽"
                          "（槽账目按宿主弱引用计占用期）", camera->GetName().c_str());
                return false;
            }

            if (is_default)
            {
                // 0 号槽 = 本世界默认相机专属（三级解析的落点，含常驻 fallback）。
                // 「升格为默认相机」时若它原持**普通槽**，先把旧槽交回（否则那个槽会一直挂在这个
                // 相机名下、迟早把 16 个槽顶满）；0 号槽唯一 ⇒ 改绑即让原宿主（含常驻 fallback）让位。
                const uint32_t old_slot = camera_info_storage->FindSlot(camera);
                if (old_slot != CameraInfoStorage::INVALID_SLOT
                 && old_slot != CameraInfoStorage::kDefaultCameraSlot)
                    camera_info_storage->ReleaseCameraSlot(old_slot);

                if (!SetDefaultCamera(owner))
                    return false;

                camera->world_owner = this;
                return true;
            }

            if (camera_info_storage->FindSlot(camera) != CameraInfoStorage::INVALID_SLOT)
                return true;    // 已认领（幂等）

            if (!camera_info_storage->IsReady())
                return false;   // 行池未就绪：保持未认领 ⇒ 发布时跳过并一次性告警（fail-fast）

            const uint32_t slot = camera_info_storage->AcquireCameraSlot(owner);

            if (slot == CameraInfoStorage::INVALID_SLOT)
            {
                GLogError("[ECSContext] 世界相机槽耗尽（上限 %u）：相机 \"%s\" 未拿到槽，"
                          "本帧不会发布它的相机行——检查是否有相机未归还",
                          CameraInfoStorage::kSlotCapacity, camera->GetName().c_str());
                return false;
            }

            camera->world_owner = this;
            return true;
        }

        CameraComponent* ECSContext::EnsureFallbackCamera()
        {
            // 已存在 ⇒ 重新回到"默认相机"位置（可能被实体相机顶替过一轮，现在世界又没相机了）：
            // 重新成为 0 号槽的宿主。
            if (fallback_camera)
            {
                SetDefaultCamera(fallback_camera);
                return fallback_camera.get();
            }

            // 三级解析第 ④ 级：世界内没有任何相机组件，但渲染**必须**有一个相机 ⇒ 在 (0,0,0) 强制生成。
            // 常驻（创建后一直活着）、占 0 号槽、**不是实体组件**（CollectCameras() 看不到它，
            // 因此不会参与"最小 EntityID.index"的选主，也不会被当作实体相机的重复项）。
            auto camera = std::make_shared<CameraComponent>("WorldFallbackCamera");
            camera->position       = math::Vector3f(0.0f, 0.0f, 0.0f);
            camera->target         = math::Vector3f(0.0f, 0.0f, 1.0f);
            camera->is_main_camera = false;
            camera->matrix_dirty   = true;
            camera->world_owner    = this;

            fallback_camera = camera;
            SetDefaultCamera(camera);

            GLogInfo("[ECS] %s: 世界内没有相机 ⇒ 在 (0,0,0) 生成常驻 fallback 相机（占 0 号槽）",
                     GetName().c_str());

            return fallback_camera.get();
        }

        // ── T2-c：组件行 → 系统组计数（组归属是**类型属性**，取自类型表）──
        void ECSContext::OnComponentRowAdded(const ComponentType type)
        {
            const SystemGroupID group = GetComponentGroup(type);

            if (group == SystemGroupID::None)
                return;

            ++system_group_component_counts[static_cast<uint32_t>(group)];

            EnsureSystemGroupSystems(this, group, GetRenderTarget());
            SetSystemGroupEnabled(group, true);
        }

        void ECSContext::OnComponentRowRemoved(const ComponentType type)
        {
            const SystemGroupID group = GetComponentGroup(type);

            if (group == SystemGroupID::None)
                return;

            uint32_t &count = system_group_component_counts[static_cast<uint32_t>(group)];

            if (count > 0)
                --count;

            if (count == 0)
                SetSystemGroupEnabled(group, false);
        }

        void ECSContext::ClearComponentPools()
        {
            // 显式析构全部行（卸载钩子 + ~T），再把池整个丢掉（块随池析构释放）。
            // 实体销毁路径通常已经把行销干净了，这里是世界级收尾与"从未关联实体"的兜底。
            for (auto &pool : component_pools)
                if (pool)
                    pool->Clear();

            component_pools.clear();

            for (auto &slot : component_pool_slots)
                slot = nullptr;
        }

        void ECSContext::DestroyComponent(const ComponentType type, const uint32_t row)
        {
            if (row == 0 || !IsValidComponentType(type))
                return;

            ComponentPoolBase *pool = component_pool_slots[static_cast<uint32_t>(type)];

            if (!pool)
                return;     // 该类型从未建过池 ⇒ 不可能有活行

            pool->DestroyRow(row);
            OnComponentRowRemoved(type);
        }

        uint32_t ECSContext::GetTransformSegmentBudget(const bool movable) const
        {
            const auto *storage = GetTransformStorage();

            if (!storage)
                return 0;

            return movable ? storage->GetDynamicRowBudget() : storage->GetStaticRowBudget();
        }

        // ── 稳定槽位（B1b）──
        // 槽位表的下标**就是槽位**：L2W 行号 = 静态 `1+slot` / 动态 `1+预算static + frame×预算dynamic + slot`。
        // 注销只把该格置空洞并入空闲表 ⇒ **别的物体行号不受影响**（改前是 erase 整体平移）。
        uint32_t ECSContext::AcquireTransformSlot(const bool movable)
        {
            auto &free_slots = movable ? movable_free_slots : static_free_slots;
            auto &list       = movable ? movable_transforms  : static_transforms;

            if (!free_slots.empty())
            {
                const uint32_t slot = free_slots.back();
                free_slots.pop_back();
                return slot;                            // 复用空洞：新物体拿回被释放的行号
            }

            const uint32_t budget = GetTransformSegmentBudget(movable);

            if (budget != 0 && static_cast<uint32_t>(list.size()) >= budget)
                return INVALID_TRANSFORM_SLOT;          // 段预算用尽：拒绝，不扩容

            list.push_back(INVALID_TRANSFORM_ID);
            return static_cast<uint32_t>(list.size() - 1);
        }

        void ECSContext::ReleaseTransformSlot(const bool movable,const uint32_t slot)
        {
            auto &list       = movable ? movable_transforms : static_transforms;
            auto &free_slots = movable ? movable_free_slots  : static_free_slots;

            if (slot >= static_cast<uint32_t>(list.size()))
                return;

            if (!IsValidTransformID(list[slot]))        // 已是空洞 ⇒ 不重复入表
                return;

            list[slot] = INVALID_TRANSFORM_ID;
            free_slots.push_back(slot);
        }

        bool ECSContext::TryGetTransformSlot(TransformID id,bool movable,uint32_t &out_slot) const
        {
            if (!IsValidTransformID(id))
                return false;

            const auto &list = movable ? movable_transforms : static_transforms;

            for (uint32_t slot = 0; slot < static_cast<uint32_t>(list.size()); ++slot)
            {
                if (list[slot] == id)
                {
                    out_slot = slot;
                    return true;
                }
            }

            return false;
        }

        uint32_t ECSContext::GetLiveTransformCount(const bool movable) const
        {
            const auto &list = movable ? movable_transforms : static_transforms;
            uint32_t live = 0;

            for (const TransformID id : list)
            {
                if (IsValidTransformID(id))
                    ++live;
            }

            return live;
        }

        void ECSContext::RegisterTransform(TransformID id,bool isMovable)
        {
            if (!IsValidTransformID(id))
                return;

            // 有变换出现即自动装上 TransformSystem（避免样例手动注册）
            {
                auto transform_system = GetSystem<TransformSystem>();
                if (!transform_system)
                    transform_system = RegisterSystem<TransformSystem>();

                if (transform_system)
                {
                    transform_system->SetWorld(this);
                    transform_system->SetEnabled(true);
                }
            }

            uint32_t existing_slot = 0;

            if (TryGetTransformSlot(id,isMovable,existing_slot))
                return;                                     // 幂等：重复登记不改行号

            // 换段（移动性变了）时先归还旧段槽位，避免同一变换占两段
            uint32_t other_slot = 0;

            if (TryGetTransformSlot(id,!isMovable,other_slot))
                ReleaseTransformSlot(!isMovable,other_slot);

            const uint32_t slot = AcquireTransformSlot(isMovable);

            if (slot == INVALID_TRANSFORM_SLOT)
            {
                if (!transform_slot_overflow_warned)
                {
                    transform_slot_overflow_warned = true;
                    GLogError("[ECSContext] %s变换槽预算用尽（budget=%u）—— 该变换**没有 L2W 行**"
                              "（拒绝，不扩容；请调大预算后重载）。",
                              isMovable ? "动态" : "静态",
                              GetTransformSegmentBudget(isMovable));
                }

                return;
            }

            (isMovable ? movable_transforms : static_transforms)[slot] = id;
        }

        void ECSContext::MigrateTransform(TransformID id,bool toMovable)
        {
            if (!IsValidTransformID(id))
                return;

            uint32_t old_slot = 0;

            if (!TryGetTransformSlot(id,!toMovable,old_slot))
                return;                                     // 不在旧段 ⇒ 无需迁移

            ReleaseTransformSlot(!toMovable,old_slot);

            const uint32_t slot = AcquireTransformSlot(toMovable);

            if (slot == INVALID_TRANSFORM_SLOT)
            {
                if (!transform_slot_overflow_warned)
                {
                    transform_slot_overflow_warned = true;
                    GLogError("[ECSContext] %s变换槽预算用尽（budget=%u）—— Mobility 换边被拒绝，"
                              "该变换留在原段（不扩容）。",
                              toMovable ? "动态" : "静态",
                              GetTransformSegmentBudget(toMovable));
                }

                (toMovable ? movable_transforms : static_transforms)[old_slot] = id;   // 换边失败 ⇒ 复位
                return;
            }

            (toMovable ? movable_transforms : static_transforms)[slot] = id;

            ++transform_row_move_count;

            // 行号随段移动 ⇒ 旧 L2W 行作废（这正是 §9.2 说的"行号已移动"情形；改前是全体静默平移，更糟）
            if (!transform_row_move_warned)
            {
                transform_row_move_warned = true;
                GLogWarning("[ECSContext] Mobility 换边 ⇒ L2W 行号移动（transform=%u 旧槽=%u → 新段槽=%u）；"
                            "固化 ID / 预烘焙数据需重新取号。",
                            id, old_slot, slot);
            }

            if (auto *storage = GetTransformStorage())
                storage->SetMobility(id,toMovable ? 1 : 0);
        }

        void ECSContext::UnregisterTransform(TransformID id)
        {
            if (!IsValidTransformID(id))
                return;

            uint32_t slot = 0;

            if (TryGetTransformSlot(id,false,slot))
                ReleaseTransformSlot(false,slot);

            if (TryGetTransformSlot(id,true,slot))
                ReleaseTransformSlot(true,slot);
        }

        TransformID ECSContext::CreateTransform(EntityID owner,Mobility mobility)
        {
            auto *storage = GetTransformStorage();

            if (!storage)
                return INVALID_TRANSFORM_ID;

            const TransformID id = storage->Allocate();

            // 预算超限：存储侧已做一次性明确告警并拒绝分配（不扩容）⇒ 这里不再造半个变换。
            if (!IsValidTransformID(id))
                return INVALID_TRANSFORM_ID;

            storage->SetOwner(id,owner);
            storage->SetMobility(id,(mobility == Mobility::Movable) ? 1 : 0);

            RegisterTransform(id,mobility == Mobility::Movable);

            return id;
        }

        void ECSContext::DestroyTransform(TransformID id)
        {
            if (!IsValidTransformID(id))
                return;

            UnregisterTransform(id);

            if (auto *storage = GetTransformStorage())
            {
                // 先从父的子表里摘掉自己（父链与子表真源都在存储），再释放本行
                const TransformID parent = storage->GetParent(id);

                if (IsValidTransformID(parent))
                    storage->RemoveChild(parent,id);

                storage->Deallocate(id);
            }
        }

        TransformID ECSContext::GetTransformID(EntityID owner) const
        {
            const auto *storage = GetTransformStorage();

            return storage ? storage->FindByOwner(owner) : INVALID_TRANSFORM_ID;
        }

        TransformAccessor ECSContext::GetTransform(TransformID id)
        {
            return TransformAccessor(GetTransformStorage(),id);
        }

        BoundingBoxAccessor ECSContext::GetBoundingBoxByEntity(EntityID owner) const
        {
            auto *storage = bounding_box_storage.get();

            if (!storage)
                return BoundingBoxAccessor();

            // const 成员里仍要交出可变 storage 句柄（行数据由世界拥有；句柄只做定位）
            return BoundingBoxAccessor(storage,storage->FindByOwner(owner));
        }

        BoundingBoxAccessor ECSContext::GetOrCreateBoundingBox(EntityID owner)
        {
            auto *storage = bounding_box_storage.get();

            if (!storage)
                return BoundingBoxAccessor();

            auto id = storage->FindByOwner(owner);

            if (id == BoundingBoxDataStorage::INVALID_HANDLE)
            {
                id = storage->Allocate();
                storage->SetOwner(id,owner);
            }

            return BoundingBoxAccessor(storage,id);
        }

        MaterialData* ECSContext::GetMaterialData(EntityID owner)
        {
            return ComponentOfEntity<MaterialData>(GetEntity(owner));
        }

        const MaterialData* ECSContext::GetMaterialData(EntityID owner) const
        {
            return ComponentOfEntity<MaterialData>(GetEntity(owner));
        }

        MaterialData* ECSContext::GetMaterialData(Entity *owner)
        {
            return ComponentOfEntity<MaterialData>(owner);
        }

        const MaterialData* ECSContext::GetMaterialData(const Entity *owner) const
        {
            return ComponentOfEntity<MaterialData>(owner);
        }

        MaterialData* ECSContext::GetOrCreateMaterialData(EntityID owner)
        {
            return GetOrAddComponentOfEntity<MaterialData>(GetEntity(owner));
        }

        MaterialData* ECSContext::GetOrCreateMaterialData(Entity *owner)
        {
            return GetOrAddComponentOfEntity<MaterialData>(owner);
        }

        GeometryData* ECSContext::GetGeometryData(EntityID owner)
        {
            return ComponentOfEntity<GeometryData>(GetEntity(owner));
        }

        const GeometryData* ECSContext::GetGeometryData(EntityID owner) const
        {
            return ComponentOfEntity<GeometryData>(GetEntity(owner));
        }

        GeometryData* ECSContext::GetGeometryData(Entity *owner)
        {
            return ComponentOfEntity<GeometryData>(owner);
        }

        const GeometryData* ECSContext::GetGeometryData(const Entity *owner) const
        {
            return ComponentOfEntity<GeometryData>(owner);
        }

        GeometryData* ECSContext::GetOrCreateGeometryData(EntityID owner)
        {
            return GetOrAddComponentOfEntity<GeometryData>(GetEntity(owner));
        }

        GeometryData* ECSContext::GetOrCreateGeometryData(Entity *owner)
        {
            return GetOrAddComponentOfEntity<GeometryData>(owner);
        }

        void ECSContext::InvalidateEntityRuntimePipeline(EntityID owner)
        {
            // B5-b：管线缓存已迁到世界级旁表（slot 里不放指针）
            material_runtime_pipeline_table->Clear(owner);
        }

        void ECSContext::DestroyBoundingBox(BoundingBoxDataStorage::HandleID id)
        {
            if (id == BoundingBoxDataStorage::INVALID_HANDLE)
                return;

            if (auto *storage = bounding_box_storage.get())
                storage->Deallocate(id);
        }
    }//namespace ecs
}//namespace hgl