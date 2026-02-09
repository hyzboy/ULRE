#include<hgl/ecs/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/Context.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/PrimitiveRenderItem.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/graph/CameraInfo.h>
#include<glm/glm.hpp>

namespace hgl::ecs
{
    RenderPrimitiveCollectSystem::RenderPrimitiveCollectSystem(const std::string& name)
        : System(name)
    {
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

        const glm::vec3 camera_pos = glm::vec3(cameraInfo->pos);

        for (const auto& primitiveComp : primitives)
        {
            if (!primitiveComp || !primitiveComp->IsVisible() || !primitiveComp->CanRender())
                continue;

            auto entity = primitiveComp->GetOwner();
            if (!entity)
                continue;

            auto transform = entity->GetComponent<TransformComponent>();
            if (!transform)
                continue;

            auto item = std::make_unique<PrimitiveRenderItem>(entity, transform, primitiveComp);

            glm::vec3 worldPos = transform->GetWorldPosition();
            item->worldPosition = worldPos;
            glm::vec3 toCamera = worldPos - camera_pos;
            item->distanceToCamera = glm::length(toCamera);

            item->UpdateWorldMatrix();

            cache.renderItems.push_back(std::move(item));
            cache.renderableCount++;
        }
    }
}//namespace hgl::ecs
