#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/ECSComponentRecords.h>

namespace hgl::ecs
{
    namespace
    {
        struct RenderableRecord
        {
            bool visible = true;
            float boundingRadius = 1.0f;
        };
    }

    const char* RenderableComponent::GetSerializationType()
    {
        return "Renderable";
    }

    bool RenderableComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                                const hgl::UnorderedMap<EntityID, int32_t>&,
                                                ComponentRecord& out_record)
    {
        auto renderable = std::dynamic_pointer_cast<RenderableComponent>(component);
        if (!renderable)
            return false;

        RenderableRecord data{};
        data.visible = renderable->IsVisible();
        data.boundingRadius = renderable->GetBoundingRadius();

        out_record.type = GetSerializationType();
        out_record.payload = data;
        return true;
    }

    void RenderableComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                    Entity* entity,
                                                    std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&)
    {
        const auto& data = std::any_cast<const RenderableRecord&>(record.payload);
        auto renderable = std::make_shared<RenderableComponent>();
        renderable->SetVisible(data.visible);
        renderable->SetBoundingRadius(data.boundingRadius);
        entity->AddComponentInstance(renderable);
    }
}


