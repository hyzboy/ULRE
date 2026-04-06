#include <hgl/ecs/systems/render/AssetInstanceCollectSystem.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/AssetPrimitiveRenderItem.h>
#include <hgl/ecs/components/AssetInstanceComponent.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/ecs/systems/tick/TransformSystem.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/graph/asset/AssetWorldRegistry.h>
#include <hgl/graph/CameraInfo.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    AssetInstanceCollectSystem::AssetInstanceCollectSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderCollect);
        SetExecutionOrder(ExecutionPhase::RenderCollect);
        SetRenderElementType("AssetInstance");

        AddDependency<TransformSystem>();
        AddDependency<CameraSystem>();
    }

    void AssetInstanceCollectSystem::Update(float /*deltaTime*/)
    {
        if (!world || !cameraInfo)
            return;

        const hgl::graph::AssetWorldRegistry* registry = world->GetAssetWorldRegistry();
        if (!registry)
            return;

        auto& cache = world->GetRenderFrameCache();
        // Note: RenderPrimitiveCollectSystem already called cache.BeginFrame() this frame.
        // We only add items; we do NOT call BeginFrame() again here.

        std::vector<std::shared_ptr<AssetInstanceComponent>> instances;
        world->GetComponents<AssetInstanceComponent>(instances);

        const glm::vec3 camera_pos = glm::vec3(cameraInfo->pos);

        for (const auto& inst : instances)
        {
            if (!inst || !inst->IsVisible() || !inst->IsValid())
                continue;

            const EntityID entity_id = inst->GetOwnerID();
            if (!entity_id.IsValid())
                continue;

            const hgl::graph::AssetWorldDef* def = registry->Get(inst->GetAssetID());
            if (!def || !def->mesh)
                continue;

            Entity* entity = inst->GetOwner();
            if (!entity)
                continue;

            auto transform = entity->GetComponent<TransformComponent>();
            if (!transform)
                continue;

            // Emit one render item per Primitive in the StaticMesh
            const auto& prim_list = def->mesh->GetPrimitiveList();
            const int prim_count  = prim_list.GetCount();

            for (int i = 0; i < prim_count; ++i)
            {
                hgl::graph::Primitive* prim = prim_list[i];
                if (!prim)
                    continue;

                auto item = std::make_unique<AssetPrimitiveRenderItem>(
                    entity_id, transform, prim, inst->GetAssetID(), world);

                // Compute distance to camera for sorting
                const glm::vec3 world_pos = glm::vec3(item->GetWorldMatrix()[3]);
                item->worldPosition   = world_pos;
                item->distanceToCamera = glm::length(world_pos - camera_pos);

                cache.renderItems.push_back(std::unique_ptr<RenderItem>(std::move(item)));
            }
        }
    }

}//namespace hgl::ecs
