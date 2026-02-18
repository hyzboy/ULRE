#include<hgl/ecs/components/BillboardComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/ECSComponentRecords.h>

namespace hgl::ecs
{
    namespace
    {
        struct BillboardRecord
        {
            // From PrimitiveComponent
            bool visible = true;
            float boundingRadius = 1.0f;
            bool hasPrimitive = false;
            bool hasOverrideMaterial = false;

            // Billboard-specific
            bool fixed_size = true;
            uint32_t pixel_width = 256;
            uint32_t pixel_height = 256;
            float world_width = 1.0f;
            float world_height = 1.0f;
            uint32_t front_face = VK_FRONT_FACE_CLOCKWISE;
        };
    }

    const char* BillboardComponent::GetSerializationType()
    {
        return "Billboard";
    }

    bool BillboardComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                               const hgl::UnorderedMap<EntityID, int32_t>&,
                                               ComponentRecord& out_record)
    {
        auto billboard = std::dynamic_pointer_cast<BillboardComponent>(component);
        if (!billboard)
            return false;

        BillboardRecord data{};
        data.visible = billboard->IsVisible();
        data.boundingRadius = billboard->GetBoundingRadius();
        data.hasPrimitive = billboard->GetPrimitive() != nullptr;
        data.hasOverrideMaterial = billboard->GetOverrideMaterial() != nullptr;

        data.fixed_size = billboard->IsFixedPixelSize();
        data.pixel_width = billboard->GetPixelSize().x;
        data.pixel_height = billboard->GetPixelSize().y;
        data.world_width = billboard->GetWorldSize().x;
        data.world_height = billboard->GetWorldSize().y;
        data.front_face = billboard->GetFrontFace();

        out_record.type = GetSerializationType();
        out_record.payload = data;
        return true;
    }

    void BillboardComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                   Entity* entity,
                                                   std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&)
    {
        const auto& data = std::any_cast<const BillboardRecord&>(record.payload);
        auto billboard = std::make_shared<BillboardComponent>();
        billboard->SetVisible(data.visible);
        billboard->SetBoundingRadius(data.boundingRadius);

        billboard->SetFixedPixelSize(data.fixed_size);
        billboard->SetPixelSize(data.pixel_width, data.pixel_height);
        billboard->SetWorldSize(data.world_width, data.world_height);
        billboard->SetFrontFace(static_cast<VkFrontFace>(data.front_face));

        entity->AddComponentInstance(billboard);
    }

    void BillboardComponent::SetPixelSize(uint32_t width, uint32_t height)
    {
        pixel_size.x = width;
        pixel_size.y = height;
    }

    void BillboardComponent::SetPixelSize(const hgl::math::Vector2u& size)
    {
        pixel_size = size;
    }

    void BillboardComponent::SetWorldSize(float width, float height)
    {
        world_size.x = width;
        world_size.y = height;
    }

    void BillboardComponent::SetWorldSize(const glm::vec2& size)
    {
        world_size = size;
    }

    void BillboardComponent::OnAttach()
    {
        // Call parent class implementation
        PrimitiveComponent::OnAttach();
    }

    void BillboardComponent::OnUpdate(float deltaTime)
    {
        // Call parent class implementation
        PrimitiveComponent::OnUpdate(deltaTime);
    }

    void BillboardComponent::OnDetach()
    {
        // Call parent class implementation
        PrimitiveComponent::OnDetach();
    }
}//namespace hgl::ecs
