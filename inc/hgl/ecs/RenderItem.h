#pragma once

#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>

namespace hgl::ecs
{
    // Forward declarations
    class World;
    class RenderableComponent;

    /**
        * Render item - collected entity with rendering data
        */
    struct RenderItem
    {
        std::shared_ptr<Entity> entity;
        std::shared_ptr<TransformComponent> transform;
        std::shared_ptr<RenderableComponent> renderable;
        glm::mat4 worldMatrix;
        float distanceToCamera;
        bool isVisible;

        RenderItem()
            : worldMatrix(1.0f)
            , distanceToCamera(0.0f)
            , isVisible(true)
        {
        }
    };
}//namespace hgl::ecs
