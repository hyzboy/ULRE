#pragma once

#include<hgl/ecs/Component.h>
#include<glm/glm.hpp>
#include<memory>
#include <hgl/type/UnorderedMap.h>
#include<utility>
#include<vector>

namespace hgl::ecs
{
    struct ComponentRecord;
    class TransformComponent;
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

        static const char* GetSerializationType();
        static bool SerializeToRecord(const std::shared_ptr<Component>& component,
                                      const hgl::UnorderedMap<EntityID, int32_t>& entity_index,
                                      ComponentRecord& out_record);
        static void DeserializeFromRecord(const ComponentRecord& record,
                                          Entity* entity,
                                          std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents);
    };
}//namespace hgl::ecs

