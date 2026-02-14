#include<hgl/ecs/components/SubWorldComponent.h>
#include<hgl/ecs/core/Context.h>
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
    SubWorldComponent::SubWorldComponent(const std::string& name)
        : Component(name)
    {
    }

    SubWorldComponent::~SubWorldComponent()
    {
        if (sub_world)
        {
            sub_world->Shutdown();
        }
    }

    bool SubWorldComponent::Initialize(ECSContext* parent_context)
    {
        if (!parent_context || !sub_world)
            return false;

        // Attach this context to parent (share TransformDataStorage)
        sub_world->AttachToParent(parent_context);

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
        auto camera_system = sub_world->RegisterTickSystem<CameraSystem>(sub_world.get());
        auto bbox_system = sub_world->RegisterTickSystem<BoundingBoxUpdateSystem>();
        auto render_collect_system = sub_world->RegisterTickSystem<RenderPrimitiveCollectSystem>();
        auto render_batch_system = sub_world->RegisterTickSystem<RenderPrimitiveBatchSystem>();
        auto render_commit_system = sub_world->RegisterRenderSystem<RenderBufferCommitSystem>();
        auto render_submit_system = sub_world->RegisterRenderSystem<RenderPrimitiveSubmitSystem>();

        if (bbox_system)
            bbox_system->SetWorld(sub_world.get());

        if (render_collect_system)
        {
            render_collect_system->SetWorld(sub_world.get());
            render_collect_system->SetCameraInfo(parent_camera_info);
        }

        if (render_batch_system)
        {
            render_batch_system->SetWorld(sub_world.get());
            render_batch_system->SetCameraInfo(parent_camera_info);
            render_batch_system->SetDevice(parent_device);
        }

        if (render_commit_system)
        {
            render_commit_system->SetWorld(sub_world.get());
            render_commit_system->SetDevice(parent_device);
        }

        if (render_submit_system)
        {
            render_submit_system->SetWorld(sub_world.get());
        }

        // Initialize sub-world systems
        sub_world->Initialize();

        // Setup visibility inheritance linkage
        SetupVisibilityInheritance();

        return true;
    }

    void SubWorldComponent::UpdateSubWorld(float delta_time)
    {
        if (!sub_world || !sub_world->IsActive())
            return;

        static bool update_logged = false;
        if (!update_logged)
        {
            update_logged = true;
            GLogInfo(OS_TEXT("SubWorldComponent UpdateSubWorld active"));
        }

        sub_world->Tick(delta_time);
    }

    void SubWorldComponent::RenderSubWorld(graph::RenderCmdBuffer* cmd, float delta_time)
    {
        if (!sub_world || !sub_world->IsActive())
            return;

        static bool render_logged = false;
        if (!render_logged)
        {
            render_logged = true;
            GLogInfo(OS_TEXT("SubWorldComponent RenderSubWorld active"));
        }

        sub_world->Render(cmd, delta_time);
    }

    void SubWorldComponent::ClearSubWorld()
    {
        if (sub_world)
        {
            sub_world->ClearEntities();
        }
    }

    void SubWorldComponent::OnAttach()
    {
        if (!owner_entity)
            owner_entity = GetOwner();

        // Create sub-world if not already created
        if (!sub_world)
        {
            std::string sub_world_name = owner_entity ? owner_entity->GetName() + "_SubWorld" : "SubWorld";
            sub_world = std::make_shared<ECSContext>(sub_world_name);

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
        if (sub_world)
        {
            sub_world->Shutdown();
            sub_world.reset();
        }
    }

    void SubWorldComponent::SetupVisibilityInheritance()
    {
        if (!sub_world || !owner_entity)
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

