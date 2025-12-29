#pragma once

#include<hgl/ecs/Component.h>
#include<glm/glm.hpp>

namespace hgl::ecs
{
    /**
    * Base renderable component interface
    * Derived classes should implement specific rendering needs
    */
    class RenderableComponent : public Component
    {
    protected:

        bool visible;
        float boundingRadius; // Simple bounding sphere for frustum culling

    public:

        explicit RenderableComponent(const std::string& name = "Renderable")
            : Component(name)
            , visible(true)
            , boundingRadius(1.0f)
        {
        }

        virtual ~RenderableComponent() = default;

        bool IsVisible() const { return visible; }
        void SetVisible(bool v) { visible = v; }

        float GetBoundingRadius() const { return boundingRadius; }
        void SetBoundingRadius(float radius) { boundingRadius = radius; }

        // Override in derived classes for specific rendering
        virtual void Render(const glm::mat4& worldMatrix) {}
    };        
}//namespace hgl::ecs
