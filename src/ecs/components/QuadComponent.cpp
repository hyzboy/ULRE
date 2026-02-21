#include<hgl/ecs/components/QuadComponent.h>

namespace hgl::ecs
{
    void QuadComponent::SetPixelSize(uint32_t width, uint32_t height)
    {
        pixel_size = hgl::math::Vector2u(width, height);
    }

    void QuadComponent::SetPixelSize(const hgl::math::Vector2u& size)
    {
        pixel_size = size;
    }

    void QuadComponent::SetWorldSize(float width, float height)
    {
        world_size = glm::vec2(width, height);
    }

    void QuadComponent::SetWorldSize(const glm::vec2& size)
    {
        world_size = size;
    }

    void QuadComponent::OnAttach()
    {
        // Called when component is attached to entity
        PrimitiveComponent::OnAttach();
    }

    void QuadComponent::OnUpdate(float deltaTime)
    {
        // Quad component doesn't need per-frame updates
        // Material/texture loading is handled by QuadResourcePrepareSystem and QuadMaterialBindingSystem
        PrimitiveComponent::OnUpdate(deltaTime);
    }

    void QuadComponent::OnDetach()
    {
        // Called when component is detached from entity
        PrimitiveComponent::OnDetach();
    }

    const char* QuadComponent::GetSerializationType()
    {
        return "Quad";
    }

    bool QuadComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                         const hgl::UnorderedMap<EntityID, int32_t>& entity_index,
                                         ComponentRecord& out_record)
    {
        // TODO: Implement serialization
        return true;
    }

    void QuadComponent::DeserializeFromRecord(const ComponentRecord& record,
                                             Entity* entity,
                                             std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents)
    {
        // TODO: Implement deserialization
    }
}//namespace hgl::ecs
