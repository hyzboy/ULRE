#include<hgl/ecs/components/Sprite2DComponent.h>

namespace hgl::ecs
{
    void Sprite2DComponent::SetPixelSize(uint32_t width, uint32_t height)
    {
        pixel_size = hgl::math::Vector2u(width, height);
    }

    void Sprite2DComponent::SetPixelSize(const hgl::math::Vector2u& size)
    {
        pixel_size = size;
    }

    void Sprite2DComponent::SetWorldSize(float width, float height)
    {
        world_size = glm::vec2(width, height);
    }

    void Sprite2DComponent::SetWorldSize(const glm::vec2& size)
    {
        world_size = size;
    }

    void Sprite2DComponent::OnAttach()
    {
        PrimitiveComponent::OnAttach();
    }

    void Sprite2DComponent::OnUpdate(float deltaTime)
    {
        PrimitiveComponent::OnUpdate(deltaTime);
    }

    void Sprite2DComponent::OnDetach()
    {
        PrimitiveComponent::OnDetach();
    }

    const char* Sprite2DComponent::GetSerializationType()
    {
        return "Sprite2D";
    }

    bool Sprite2DComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                              const hgl::UnorderedMap<EntityID, int32_t>& entity_index,
                                              ComponentRecord& out_record)
    {
        // TODO: Implement serialization
        return true;
    }

    void Sprite2DComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                  Entity* entity,
                                                  std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents)
    {
        // TODO: Implement deserialization
        // Note (Step 7): Add legacy "Quad" -> "Sprite2D" conversion here before removing QuadComponent.
    }
}//namespace hgl::ecs
