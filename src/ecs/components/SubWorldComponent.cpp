#include<hgl/ecs/components/SubWorldComponent.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/World.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/VisibilityComponent.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/BoundingBoxUpdateSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderBufferCommitSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveBatchSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveSubmitSystem.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    namespace
    {
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
            graph::VulkanDevice* parent_device_from_system = nullptr;

            if (auto parent_collect = parent_context->GetSystem<RenderPrimitiveCollectSystem>())
            {
                parent_camera_info = parent_collect->GetCameraInfo();
            }

            if (auto parent_batch = parent_context->GetSystem<RenderPrimitiveBatchSystem>())
            {
                if (!parent_camera_info)
                    parent_camera_info = parent_batch->GetCameraInfo();

                parent_device_from_system = parent_batch->GetDevice();
            }

            if (!parent_device_from_system)
            {
                if (auto parent_commit = parent_context->GetSystem<RenderBufferCommitSystem>())
                    parent_device_from_system = parent_commit->GetDevice();
            }

            if (auto child_collect = child_context->GetSystem<RenderPrimitiveCollectSystem>())
            {
                child_collect->SetWorld(child_context);
                child_collect->SetCameraInfo(parent_camera_info);
            }

            if (auto child_batch = child_context->GetSystem<RenderPrimitiveBatchSystem>())
            {
                child_batch->SetWorld(child_context);
                child_batch->SetCameraInfo(parent_camera_info);
                child_batch->SetDevice(parent_device_from_system);
            }

            if (auto child_commit = child_context->GetSystem<RenderBufferCommitSystem>())
            {
                child_commit->SetWorld(child_context);
                child_commit->SetDevice(parent_device_from_system);
            }

            if (auto child_submit = child_context->GetSystem<RenderPrimitiveSubmitSystem>())
            {
                child_submit->SetWorld(child_context);
            }
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
        : Component(name)
    {
    }

    ECSContext* SubWorldComponent::GetSubContext() const
    {
        return sub_world ? sub_world->GetContext() : nullptr;
    }

    SubWorldComponent::~SubWorldComponent()
    {
        if (auto* ctx = GetSubContext())
        {
            ctx->Shutdown();
        }
    }

    bool SubWorldComponent::Initialize(ECSContext* parent_context)
    {
        ECSContext* child_context = GetSubContext();
        if (!parent_context || !child_context)
            return false;

        SyncSubWorldRuntimeResources(parent_context, child_context);

        child_context->SetSubWorldAutoUpdate(false);

        const graph::CameraInfo* parent_camera_info = nullptr;
        graph::VulkanDevice* parent_device = nullptr;

        if (auto parent_collect = parent_context->GetSystem<RenderPrimitiveCollectSystem>())
        {
            parent_camera_info = parent_collect->GetCameraInfo();
        }

        if (auto parent_batch = parent_context->GetSystem<RenderPrimitiveBatchSystem>())
        {
            if (!parent_camera_info)
                parent_camera_info = parent_batch->GetCameraInfo();
            parent_device = parent_batch->GetDevice();
        }

        if (!parent_device)
        {
            if (auto parent_commit = parent_context->GetSystem<RenderBufferCommitSystem>())
                parent_device = parent_commit->GetDevice();
        }

        // Register required systems for sub-world rendering
        auto camera_system = child_context->RegisterTickSystem<CameraSystem>(child_context);
        auto bbox_system = child_context->RegisterTickSystem<BoundingBoxUpdateSystem>();
        auto render_collect_system = child_context->RegisterRenderSystem<RenderPrimitiveCollectSystem>();
        auto render_batch_system = child_context->RegisterRenderSystem<RenderPrimitiveBatchSystem>();
        auto render_commit_system = child_context->RegisterRenderSystem<RenderBufferCommitSystem>();
        auto render_submit_system = child_context->RegisterRenderSystem<RenderPrimitiveSubmitSystem>();

        if (bbox_system)
            bbox_system->SetWorld(child_context);

        if (render_collect_system)
        {
            render_collect_system->SetWorld(child_context);
            render_collect_system->SetCameraInfo(parent_camera_info);
        }

        if (render_batch_system)
        {
            render_batch_system->SetWorld(child_context);
            render_batch_system->SetCameraInfo(parent_camera_info);
            render_batch_system->SetDevice(parent_device);
        }

        if (render_commit_system)
        {
            render_commit_system->SetWorld(child_context);
            render_commit_system->SetDevice(parent_device);
        }

        if (render_submit_system)
        {
            render_submit_system->SetWorld(child_context);
        }

        // Initialize sub-world systems
        child_context->Initialize();

        // Setup visibility inheritance linkage
        SetupVisibilityInheritance();

        return true;
    }

    void SubWorldComponent::UpdateSubWorld(float delta_time)
    {
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

    void SubWorldComponent::ClearSubWorld()
    {
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
        instanced_entity_ids = std::move(created_ids);
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
    }

    void SubWorldComponent::OnAttach()
    {
        if (!owner_entity)
            owner_entity = GetOwner();

        if (!asset_path.empty())
        {
            InstantiateAssetToParent();
            return;
        }

        // Create sub-world if not already created
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
    }

    void SubWorldComponent::OnDetach()
    {
        ClearInstancedAssetEntities();

        if (auto* ctx = GetSubContext())
        {
            ctx->Shutdown();
        }

        if (sub_world)
        {
            sub_world.reset();
        }
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

