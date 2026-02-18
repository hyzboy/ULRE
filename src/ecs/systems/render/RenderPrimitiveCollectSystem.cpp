#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/BillboardComponent.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/VisibilitySystem.h>
#include<hgl/ecs/support/VisibilityDataStorage.h>
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
        SetExecutionOrder(ExecutionPhase::RenderCollect, ExecutionPriority::First);

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

        // Get visibility storage for fast O(1) lookup
        VisibilityDataStorage* visibility_storage = nullptr;
        auto vis_system = world->GetSystem<VisibilitySystem>();
        if (vis_system)
        {
            visibility_storage = vis_system->GetStorage();
        }

        std::vector<std::shared_ptr<PrimitiveComponent>> primitives;
        world->GetComponents<PrimitiveComponent>(primitives);

        size_t skipped_invisible = 0;
        size_t skipped_no_owner = 0;
        size_t skipped_no_transform = 0;
        size_t added = 0;

        size_t billboard_total = 0;
        size_t billboard_visible = 0;
        size_t billboard_invisible = 0;
        size_t billboard_can_render = 0;
        size_t billboard_added = 0;
        size_t billboard_missing_owner = 0;
        size_t billboard_missing_transform = 0;

        const glm::vec3 camera_pos = glm::vec3(cameraInfo->pos);

        for (const auto& primitiveComp : primitives)
        {
            if (!primitiveComp)
                continue;

            BillboardComponent* billboard = dynamic_cast<BillboardComponent*>(primitiveComp.get());
            const bool is_billboard = (billboard != nullptr);
            if (is_billboard)
            {
                ++billboard_total;
                std::cout << "[RenderPrimitiveCollect] Billboard candidate: entity="
                          << (primitiveComp->GetOwner() ? primitiveComp->GetOwner()->GetName() : "(null)")
                          << " visible=" << (primitiveComp->IsVisible() ? "YES" : "NO")
                          << " canRender=" << (primitiveComp->CanRender() ? "YES" : "NO")
                          << " primitive=" << (void*)primitiveComp->GetPrimitive()
                          << " material=" << (void*)primitiveComp->GetMaterialInstance()
                          << " pipeline=" << (void*)primitiveComp->GetPipeline()
                          << std::endl;
            }

            if (!primitiveComp->IsVisible() || !primitiveComp->CanRender())
            {
                if (!primitiveComp->IsVisible())
                {
                    ++skipped_invisible;
                    if (is_billboard)
                        ++billboard_invisible;
                }
                continue;
            }

            if (is_billboard)
            {
                ++billboard_visible;
                ++billboard_can_render;
            }

            EntityID entity_id = primitiveComp->GetOwnerID();

            // Fast O(1) lookup from VisibilityDataStorage
            if (visibility_storage && visibility_storage->IsInvisible(entity_id))
            {
                ++skipped_invisible;
                continue;
            }

            Entity* entity = primitiveComp->GetOwner();
            if (!entity)
            {
                ++skipped_no_owner;
                if (is_billboard)
                    ++billboard_missing_owner;
                continue;
            }

            auto transform = entity->GetComponent<TransformComponent>();
            if (!transform)
            {
                ++skipped_no_transform;
                if (is_billboard)
                    ++billboard_missing_transform;
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

            if (is_billboard)
            {
                ++billboard_added;
                std::cout << "[RenderPrimitiveCollect] Billboard added: entity=" << entity->GetName()
                          << " worldPos=(" << worldPos.x << ", " << worldPos.y << ", " << worldPos.z << ")"
                          << " distance=" << glm::length(toCamera)
                          << std::endl;
            }
        }

        if (billboard_total > 0)
        {
            std::cout << "[RenderPrimitiveCollect] Billboard summary: total=" << billboard_total
                      << " visible=" << billboard_visible
                      << " invisible=" << billboard_invisible
                      << " canRender=" << billboard_can_render
                      << " added=" << billboard_added
                      << " noOwner=" << billboard_missing_owner
                      << " noTransform=" << billboard_missing_transform
                      << std::endl;
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

