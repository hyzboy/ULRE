#include<hgl/ecs/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/Context.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/PrimitiveRenderItem.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/TransformSystem.h>
#include<hgl/ecs/CameraSystem.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/log/Log.h>
#include<glm/glm.hpp>

namespace hgl::ecs
{
    RenderPrimitiveCollectSystem::RenderPrimitiveCollectSystem(const std::string& name)
        : System(name)
    {
        // Set system type and properties
        SetSystemType(SystemType::RenderCollect);
        SetExecutionOrder(100);  // First render stage
        
        // Declare dependencies
        AddDependency<TransformSystem>(); // Needs world transforms
        AddDependency<CameraSystem>();    // Needs camera info
    }

    void RenderPrimitiveCollectSystem::Update(float /*deltaTime*/)
    {
        if (!world || !cameraInfo)
            return;

        auto& cache = world->GetRenderFrameCache();
        cache.cameraInfo = cameraInfo;
        cache.BeginFrame();

        std::vector<std::shared_ptr<PrimitiveComponent>> primitives;
        world->GetComponents<PrimitiveComponent>(primitives);

        size_t skipped_invisible = 0;
        size_t skipped_no_owner = 0;
        size_t skipped_no_transform = 0;
        size_t added = 0;

        const glm::vec3 camera_pos = glm::vec3(cameraInfo->pos);

        for (const auto& primitiveComp : primitives)
        {
            if (!primitiveComp || !primitiveComp->IsVisible() || !primitiveComp->CanRender())
            {
                if (primitiveComp && !primitiveComp->IsVisible())
                    ++skipped_invisible;
                continue;
            }

            EntityID entity_id = primitiveComp->GetOwnerID();
            Entity* entity = primitiveComp->GetOwner();
            if (!entity)
            {
                ++skipped_no_owner;
                continue;
            }

            auto transform = entity->GetComponent<TransformComponent>();
            if (!transform)
            {
                ++skipped_no_transform;
                continue;
            }

            auto item = std::make_unique<PrimitiveRenderItem>(entity_id, transform, primitiveComp, world);

            glm::vec3 worldPos = transform->GetWorldPosition();
            item->worldPosition = worldPos;
            glm::vec3 toCamera = worldPos - camera_pos;
            item->distanceToCamera = glm::length(toCamera);

            item->UpdateWorldMatrix();

            cache.renderItems.push_back(std::move(item));
            cache.renderableCount++;
            ++added;
        }

        //if (cache.renderableCount == 0)
        //{
        //    GLogInfo("[RenderPrimitiveCollectSystem] No renderables: total=%zu visible=%zu no_owner=%zu no_transform=%zu",
        //             primitives.size(),
        //             added,
        //             skipped_no_owner,
        //             skipped_no_transform);
        //}
    }
}//namespace hgl::ecs
