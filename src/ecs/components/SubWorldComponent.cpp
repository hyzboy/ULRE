#include<hgl/ecs/components/SubWorldComponent.h>
#include<hgl/ecs/components/SubSceneMembershipComponent.h>
#include<hgl/ecs/core/ComponentRecords.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/World.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/VisibilityComponent.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/BoundingBoxUpdateSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
// Old Cull/Sort/Build/Finalize/Submit systems replaced by PrimitiveRenderPipelineGroup
#include<hgl/ecs/support/primitive/PrimitiveRenderPipelineGroup.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/StructuredBufferAccessor.h>
#include<hgl/log/Log.h>
#include<atomic>

namespace hgl::ecs
{
    namespace
    {
        struct EntityIDRecord
        {
            uint32_t index = UINT32_MAX;
            uint16_t generation = 0;
        };

        struct SubWorldRecord
        {
            uint8_t mode = static_cast<uint8_t>(SubWorldMode::SharedContext);
            bool render_shared = true;
            bool logic_isolated = false;
            uint64_t subscene_id = 0;
            EntityIDRecord root_entity_id{};
            bool paused = false;
            bool tick_enabled = true;
            bool render_enabled = true;
            std::string asset_path;
            bool asset_binary = false;
        };

        EntityIDRecord ToEntityIDRecord(const EntityID& id)
        {
            EntityIDRecord rec;
            rec.index = id.index;
            rec.generation = id.generation;
            return rec;
        }

        EntityID FromEntityIDRecord(const EntityIDRecord& rec)
        {
            if (rec.index == UINT32_MAX)
                return EntityID::Invalid();

            return EntityID(rec.index, rec.generation);
        }

        uint64_t NextSubsceneID()
        {
            static std::atomic<uint64_t> next_id{1};
            return next_id.fetch_add(1, std::memory_order_relaxed);
        }

        void SyncSubWorldRuntimeResources(ECSContext* parent_context, ECSContext* child_context)
        {
            if (!parent_context || !child_context)
                return;

            child_context->SetGraphicsContext(parent_context->GetGraphicsContext());
            child_context->SetRenderContext(parent_context->GetRenderContext());
            child_context->SetRenderTarget(parent_context->GetRenderTarget());

            if (auto* parent_device = parent_context->GetGPUDevice())
            {
                child_context->InitializeGraphics(parent_device, parent_context->GetRenderTarget());
            }

            const graph::CameraInfo* parent_camera_info = nullptr;
            graph::VulkanDevice* parent_device_from_system = parent_context->GetGPUDevice();

            std::shared_ptr<CameraSystem> parent_camera_system;
            std::shared_ptr<CameraSystem> child_camera_system;

            parent_camera_system = parent_context->GetSystem<CameraSystem>();
            child_camera_system = child_context->GetSystem<CameraSystem>();

            if (auto parent_collect = parent_context->GetSystem<RenderPrimitiveCollectSystem>())
            {
                parent_camera_info = parent_collect->GetCameraInfo();
            }

            if (!parent_camera_info && parent_camera_system)
                parent_camera_info = parent_camera_system->GetCameraInfo();

            if (child_camera_system)
            {
                child_camera_system->SetRenderContext(parent_context->GetRenderContext());

                if (parent_camera_system && parent_camera_system->GetViewportInfo())
                    child_camera_system->SetViewportInfo(parent_camera_system->GetViewportInfo());
                else if (auto *rt = parent_context->GetRenderTarget())
                    child_camera_system->SetViewportInfo(rt->GetViewportInfo());

                if (parent_camera_info)
                {
                    if (auto *child_camera_ubo = child_camera_system->GetCameraUBO())
                    {
                        child_camera_ubo->Update(*parent_camera_info);
                        child_camera_ubo->MarkDirty();
                    }
                }
            }

            if (auto child_collect = child_context->GetSystem<RenderPrimitiveCollectSystem>())
            {
                child_collect->SetWorld(child_context);
                child_collect->SetCameraInfo(parent_camera_info);
            }

            // PrimitiveRenderSystem uses context set by RegisterRenderSystem, no manual SetWorld needed
        }

        void ParentImportedRoots(Entity* owner_entity,
                                 ECSContext* parent_context,
                                 const std::vector<EntityID>& created_ids)
        {
            if (!owner_entity || !parent_context)
                return;

            const EntityID owner_id = owner_entity->GetID();

            for (const auto& id : created_ids)
            {
                if (!id.IsValid())
                    continue;

                Entity* entity = parent_context->GetEntity(id);
                if (!entity)
                    continue;

                auto transform = entity->GetComponent<TransformComponent>();
                if (!transform)
                    continue;

                if (!transform->GetParentID().IsValid())
                    transform->SetParent(owner_id);
            }
        }

        void TagEntitiesWithSubscene(ECSContext* parent_context,
                                     const std::vector<EntityID>& created_ids,
                                     uint64_t subscene_id)
        {
            if (!parent_context || subscene_id == 0)
                return;

            for (const auto& id : created_ids)
            {
                if (!id.IsValid())
                    continue;

                Entity* entity = parent_context->GetEntity(id);
                if (!entity)
                    continue;

                auto membership = entity->GetComponent<SubSceneMembershipComponent>();
                if (!membership)
                    membership = entity->AddComponent<SubSceneMembershipComponent>(subscene_id);
                else
                    membership->SetSubsceneID(subscene_id);
            }
        }

        bool SyncSubWorldFrameIndex(Entity* owner_entity, ECSContext* child_context)
        {
            if (!owner_entity || !child_context)
                return false;

            ECSContext* parent_context = owner_entity->GetContext();
            if (!parent_context)
                return false;

            const uint32_t parent_frame = parent_context->GetFrameIndex();
            const uint32_t child_frame = child_context->GetFrameIndex();

            if (parent_frame != child_frame)
            {
                child_context->SetFrameIndex(parent_frame);
                return true;
            }

            return false;
        }

        void TickNestedSubWorlds(ECSContext* context, float delta_time)
        {
            if (!context)
                return;

            std::vector<std::shared_ptr<SubWorldComponent>> sub_worlds;
            context->GetComponents(sub_worlds);
            for (const auto& sub_world : sub_worlds)
            {
                if (sub_world)
                    sub_world->UpdateSubWorld(delta_time);
            }
        }

        void RenderNestedSubWorlds(ECSContext* context, graph::RenderCmdBuffer* cmd, float delta_time)
        {
            if (!context)
                return;

            std::vector<std::shared_ptr<SubWorldComponent>> sub_worlds;
            context->GetComponents(sub_worlds);
            for (const auto& sub_world : sub_worlds)
            {
                if (sub_world)
                    sub_world->RenderSubWorld(cmd, delta_time);
            }
        }
    }

    SubWorldComponent::SubWorldComponent(const std::string& name)
        : SubWorldComponent(SubWorldMode::SharedContext, name)
    {
    }

    SubWorldComponent::SubWorldComponent(SubWorldMode init_mode, const std::string& name)
        : Component(name)
    {
        mode = init_mode;
        SyncPolicyFromMode();
        subscene_id = NextSubsceneID();
    }

    void SubWorldComponent::SyncPolicyFromMode()
    {
        if (mode == SubWorldMode::SharedContext)
        {
            render_shared = true;
            logic_isolated = false;
            return;
        }

        render_shared = false;
        logic_isolated = true;
    }

    void SubWorldComponent::SyncModeFromPolicy()
    {
        // Keep policy matrix valid; pure "none" mode is not supported.
        if (!render_shared && !logic_isolated)
            render_shared = true;

        // Mode remains as a compatibility surface for existing callers.
        mode = logic_isolated ? SubWorldMode::IsolatedContext : SubWorldMode::SharedContext;
    }

    void SubWorldComponent::SetMode(SubWorldMode m)
    {
        mode = m;
        SyncPolicyFromMode();
        SyncSubsceneStateToParentContext();
    }

    void SubWorldComponent::SetSubsceneID(uint64_t id)
    {
        if (id == 0 || subscene_id == id)
            return;

        ECSContext* parent_context = owner_entity ? owner_entity->GetContext() : nullptr;
        if (parent_context)
            parent_context->RemoveSubsceneState(subscene_id);

        subscene_id = id;
        SyncSubsceneStateToParentContext();
    }

    void SubWorldComponent::SetRenderShared(bool value)
    {
        if (!value && !logic_isolated)
        {
            LogWarning("[SubWorldComponent] Reject invalid policy: render_shared=false requires logic_isolated=true");
            return;
        }

        render_shared = value;
        SyncModeFromPolicy();
        SyncSubsceneStateToParentContext();
    }

    void SubWorldComponent::SetLogicIsolated(bool value)
    {
        if (!render_shared && !value)
        {
            LogWarning("[SubWorldComponent] Reject invalid policy: render_shared=false with logic_isolated=false is not supported");
            return;
        }

        logic_isolated = value;
        SyncModeFromPolicy();
        SyncSubsceneStateToParentContext();
    }

    void SubWorldComponent::SetPaused(bool value)
    {
        if (paused == value)
            return;

        paused = value;
        SyncSubsceneStateToParentContext();
    }

    void SubWorldComponent::SetTickEnabled(bool value)
    {
        if (tick_enabled == value)
            return;

        tick_enabled = value;
        SyncSubsceneStateToParentContext();
    }

    void SubWorldComponent::SetRenderEnabled(bool value)
    {
        if (render_enabled == value)
            return;

        render_enabled = value;
        SyncSubsceneStateToParentContext();
    }

    void SubWorldComponent::SyncSubsceneStateToParentContext()
    {
        if (!owner_entity)
            owner_entity = GetOwner();

        ECSContext* parent_context = owner_entity ? owner_entity->GetContext() : nullptr;
        if (!parent_context)
            return;

        parent_context->SetSubsceneState(subscene_id, paused, tick_enabled, render_enabled);
    }

    ECSContext* SubWorldComponent::GetSubContext() const
    {
        return sub_world ? sub_world->GetContext() : nullptr;
    }

    SubWorldComponent::~SubWorldComponent()
    {
        if (RequiresLocalContext())
        {
            if (auto* ctx = GetSubContext())
            {
                ctx->Shutdown();
            }
        }
    }

    bool SubWorldComponent::Initialize(ECSContext* parent_context)
    {
        if (!RequiresLocalContext())
            return parent_context != nullptr;

        ECSContext* child_context = GetSubContext();
        if (!parent_context || !child_context)
            return false;

        SyncSubWorldRuntimeResources(parent_context, child_context);

        child_context->SetSubWorldAutoUpdate(false);
        child_context->SetContextRole(ECSContext::ContextRole::LocalSubWorld);
        child_context->SetRenderSystemRegistrationAllowed(!render_shared);

        const graph::CameraInfo* parent_camera_info = nullptr;

        if (auto parent_collect = parent_context->GetSystem<RenderPrimitiveCollectSystem>())
        {
            parent_camera_info = parent_collect->GetCameraInfo();
        }

        // Register required local gameplay systems.
        auto camera_system = child_context->RegisterTickSystemScoped<CameraSystem>(
            ECSContext::SystemOwnershipScope::LocalIsolated,
            child_context);
        auto bbox_system = child_context->RegisterTickSystemScoped<BoundingBoxUpdateSystem>(
            ECSContext::SystemOwnershipScope::LocalIsolated);
        std::shared_ptr<RenderPrimitiveCollectSystem> render_collect_system;
        if (!render_shared)
            render_collect_system = child_context->RegisterRenderSystemScoped<RenderPrimitiveCollectSystem>(
                ECSContext::SystemOwnershipScope::LocalIsolated);

        if (bbox_system)
            bbox_system->SetWorld(child_context);

        if (render_collect_system)
        {
            render_collect_system->SetWorld(child_context);
            render_collect_system->SetCameraInfo(parent_camera_info);
        }

        if (!render_shared)
        {
            // New unified pipeline group replaces Cull/Sort/Build/Finalize/Submit systems
            hgl::ecs::PrimitiveRenderPipelineGroup group;
            group.Initialize(child_context);
        }

        // Initialize sub-world systems
        child_context->Initialize();

        // Setup visibility inheritance linkage
        SetupVisibilityInheritance();

        return true;
    }

    bool SubWorldComponent::IsInitialized() const
    {
        if (!RequiresLocalContext())
            return owner_entity && owner_entity->GetContext();

        return GetSubContext() != nullptr;
    }

    void SubWorldComponent::UpdateSubWorld(float delta_time)
    {
        if (!logic_isolated)
            return;

        ECSContext* child_context = GetSubContext();
        if (!child_context || !child_context->IsActive())
            return;

        if (paused || !tick_enabled)
            return;

        ECSContext* parent_context = owner_entity ? owner_entity->GetContext() : nullptr;
        SyncSubWorldRuntimeResources(parent_context, child_context);

        if (SyncSubWorldFrameIndex(owner_entity, child_context))
        {
            static bool frame_sync_warned_tick = false;
            if (!frame_sync_warned_tick)
            {
                frame_sync_warned_tick = true;
                LogWarning("[SubWorldComponent] Tick frame index drift detected and synchronized");
            }
        }

        static bool update_logged = false;
        if (!update_logged)
        {
            update_logged = true;
            LogInfo(OS_TEXT("SubWorldComponent UpdateSubWorld active"));
        }

        child_context->Tick(delta_time);
        TickNestedSubWorlds(child_context, delta_time);
    }

    void SubWorldComponent::RenderSubWorld(graph::RenderCmdBuffer* cmd, float delta_time)
    {
        if (render_shared)
            return;

        ECSContext* child_context = GetSubContext();
        if (!child_context || !child_context->IsActive())
            return;

        if (paused || !render_enabled)
            return;

        ECSContext* parent_context = owner_entity ? owner_entity->GetContext() : nullptr;
        SyncSubWorldRuntimeResources(parent_context, child_context);

        if (SyncSubWorldFrameIndex(owner_entity, child_context))
        {
            static bool frame_sync_warned_render = false;
            if (!frame_sync_warned_render)
            {
                frame_sync_warned_render = true;
                LogWarning("[SubWorldComponent] Render frame index drift detected and synchronized");
            }
        }

        static bool render_logged = false;
        if (!render_logged)
        {
            render_logged = true;
            LogInfo(OS_TEXT("SubWorldComponent RenderSubWorld active"));
        }

        child_context->Render(cmd, delta_time);
        RenderNestedSubWorlds(child_context, cmd, delta_time);
    }

    void SubWorldComponent::SyncSharedRenderBridge(float delta_time)
    {
        (void)delta_time;

        // Bridge only applies to hybrid mode: local gameplay with global render.
        if (!logic_isolated || !render_shared)
            return;

        ECSContext* child_context = GetSubContext();
        if (!child_context || !child_context->IsActive())
            return;

        if (paused || !render_enabled)
            return;

        ECSContext* parent_context = owner_entity ? owner_entity->GetContext() : nullptr;
        SyncSubWorldRuntimeResources(parent_context, child_context);

        if (SyncSubWorldFrameIndex(owner_entity, child_context))
        {
            static bool frame_sync_warned_bridge = false;
            if (!frame_sync_warned_bridge)
            {
                frame_sync_warned_bridge = true;
                LogWarning("[SubWorldComponent] Bridge frame index drift detected and synchronized");
            }
        }

        // Ensure latest transform payload is visible before root render collect phase.
        if (auto transform_system = child_context->GetSystem<TransformSystem>())
            transform_system->SubmitTransformUpdates();
    }

    void SubWorldComponent::PrepareSubWorld(float delta_time)
    {
        if (render_shared)
            return;

        ECSContext* child_context = GetSubContext();
        if (!child_context || !child_context->IsActive())
            return;

        if (paused || !render_enabled)
            return;

        ECSContext* parent_context = owner_entity ? owner_entity->GetContext() : nullptr;
        SyncSubWorldRuntimeResources(parent_context, child_context);
        SyncSubWorldFrameIndex(owner_entity, child_context);

        // Run Collect → Batch → BufferCommit → BufferUpload → FrameSync.
        // This MUST happen before the parent's BeginRenderPass so that any
        // StagedBuffers written during Batch (e.g. transform_vab) are
        // copied to GPU memory before the render pass opens.
        const uint32_t frame_index = child_context->GetFrameIndex();
        child_context->PrepareRenderPassSetup(frame_index, delta_time);

        // Propagate to nested sub-worlds (depth-first)
        std::vector<std::shared_ptr<SubWorldComponent>> nested;
        child_context->GetComponents(nested);
        for (const auto& sw : nested)
            if (sw) sw->PrepareSubWorld(delta_time);
    }

    void SubWorldComponent::DrawSubWorld(graph::RenderCmdBuffer* cmd, float delta_time)
    {
        if (render_shared)
            return;

        ECSContext* child_context = GetSubContext();
        if (!child_context || !child_context->IsActive())
            return;

        if (paused || !render_enabled)
            return;

        // PrepareSubWorld() must have been called this frame before BeginRenderPass.
        // Here we only issue GPU draw commands into the already-open render pass.
        child_context->RenderDrawOnly(cmd, delta_time);
    }

    void SubWorldComponent::ClearSubWorld()
    {
        if (!RequiresLocalContext())
            return;

        if (auto* ctx = GetSubContext())
        {
            ctx->ClearEntities();
        }
    }

    bool SubWorldComponent::InstantiateAssetToParent()
    {
        if (!owner_entity)
            owner_entity = GetOwner();

        ECSContext* parent_context = owner_entity ? owner_entity->GetContext() : nullptr;
        if (!parent_context)
            return false;

        if (asset_path.empty())
            return false;

        ClearInstancedAssetEntities();

        std::vector<EntityID> created_ids;
        bool ok = false;

        if (asset_binary)
            ok = parent_context->ImportFromBinary(asset_path, &created_ids);
        else
            ok = parent_context->ImportFromJson(asset_path, &created_ids);

        if (!ok)
        {
            LogWarning("[SubWorldComponent] Asset import failed: %s", asset_path.c_str());
            return false;
        }

        ParentImportedRoots(owner_entity, parent_context, created_ids);
        TagEntitiesWithSubscene(parent_context, created_ids, subscene_id);
        instanced_entity_ids = std::move(created_ids);

        root_entity_id = owner_entity ? owner_entity->GetID() : EntityID();
        for (const auto& id : instanced_entity_ids)
        {
            if (id.IsValid())
            {
                root_entity_id = id;
                break;
            }
        }

        return true;
    }

    void SubWorldComponent::ClearInstancedAssetEntities()
    {
        if (!owner_entity)
            owner_entity = GetOwner();

        ECSContext* parent_context = owner_entity ? owner_entity->GetContext() : nullptr;
        if (!parent_context)
        {
            instanced_entity_ids.clear();
            return;
        }

        for (const auto& id : instanced_entity_ids)
        {
            if (id.IsValid())
                parent_context->DestroyEntity(id);
        }

        instanced_entity_ids.clear();
        root_entity_id = EntityID();
    }

    void SubWorldComponent::OnAttach()
    {
        if (!owner_entity)
            owner_entity = GetOwner();

        if (!RequiresLocalContext())
        {
            SyncSubsceneStateToParentContext();

            if (!asset_path.empty())
                InstantiateAssetToParent();

            return;
        }

        // Create local sub-world context first when local logic/render isolation is required.
        if (!sub_world)
        {
            std::string sub_world_name = owner_entity ? owner_entity->GetName() + "_SubWorld" : "SubWorld";
            sub_world = std::make_shared<World>(sub_world_name);

            // Get parent context through entity
            ECSContext* parent_context = nullptr;
            if (owner_entity)
            {
                parent_context = owner_entity->GetContext();
            }

            Initialize(parent_context);
        }

        // Asset instancing remains parent-side, but must not bypass local-context setup.
        if (!asset_path.empty())
            InstantiateAssetToParent();

        SyncSubsceneStateToParentContext();
    }

    void SubWorldComponent::OnDetach()
    {
        ClearInstancedAssetEntities();

        if (RequiresLocalContext())
        {
            if (auto* ctx = GetSubContext())
            {
                ctx->Shutdown();
            }
        }

        if (sub_world)
        {
            sub_world.reset();
        }

        if (!owner_entity)
            owner_entity = GetOwner();

        ECSContext* parent_context = owner_entity ? owner_entity->GetContext() : nullptr;
        if (parent_context)
            parent_context->RemoveSubsceneState(subscene_id);
    }

    const char* SubWorldComponent::GetSerializationType()
    {
        return "SubWorld";
    }

    bool SubWorldComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                              const hgl::UnorderedMap<EntityID, int32_t>&,
                                              ComponentRecord& out_record)
    {
        auto sub_world = std::dynamic_pointer_cast<SubWorldComponent>(component);
        if (!sub_world)
            return false;

        SubWorldRecord data{};
        data.mode = static_cast<uint8_t>(sub_world->GetMode());
        data.render_shared = sub_world->IsRenderShared();
        data.logic_isolated = sub_world->IsLogicIsolated();
        data.subscene_id = sub_world->GetSubsceneID();
        data.root_entity_id = ToEntityIDRecord(sub_world->GetRootEntityID());
        data.paused = sub_world->IsPaused();
        data.tick_enabled = sub_world->IsTickEnabled();
        data.render_enabled = sub_world->IsRenderEnabled();
        data.asset_path = sub_world->GetAssetPath();
        data.asset_binary = sub_world->IsAssetBinary();

        out_record.type = GetSerializationType();
        out_record.payload = data;
        return true;
    }

    void SubWorldComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                  Entity* entity,
                                                  std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&)
    {
        if (!entity)
            return;

        const auto& data = std::any_cast<const SubWorldRecord&>(record.payload);

        auto sub_world = std::make_shared<SubWorldComponent>(
            static_cast<SubWorldMode>(data.mode));

        sub_world->SetRenderShared(data.render_shared);
        sub_world->SetLogicIsolated(data.logic_isolated);

        if (data.subscene_id != 0)
            sub_world->SetSubsceneID(data.subscene_id);

        sub_world->SetRootEntityID(FromEntityIDRecord(data.root_entity_id));
        sub_world->SetAssetPath(data.asset_path, data.asset_binary);
        sub_world->SetPaused(data.paused);
        sub_world->SetTickEnabled(data.tick_enabled);
        sub_world->SetRenderEnabled(data.render_enabled);

        entity->AddComponentInstance(sub_world);
    }

    void SubWorldComponent::SetupVisibilityInheritance()
    {
        if (!GetSubContext() || !owner_entity)
            return;

        // The visibility inheritance is automatically handled through:
        // 1. VisibilityDataStorage::IsInvisible() checks parent chain
        // 2. Parent-child relationships via TransformComponent
        // 3. Shared transform storage across contexts

        // Ensure owner entity has a VisibilityComponent if not already present
        auto owner_vis = owner_entity->GetComponent<VisibilityComponent>();
        if (!owner_vis)
        {
            owner_entity->AddComponent<VisibilityComponent>();
        }
    }
}//namespace hgl::ecs

