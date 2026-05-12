#include<hgl/ecs/components/QuadMeshComponent.h>
#include<hgl/ecs/core/Entity.h>

namespace hgl::ecs
{
    namespace
    {
        struct QuadMeshRecord
        {
            glm::vec2 size { 1.0f, 1.0f };
            glm::vec2 pivot { 0.5f, 0.5f };
            glm::vec4 uv_rect { 0.0f, 0.0f, 1.0f, 1.0f };
            uint32_t front_face = static_cast<uint32_t>(VK_FRONT_FACE_CLOCKWISE);
        };
    }

    void QuadMeshComponent::OnAttach()
    {
        geometry_dirty = true;
    }

    void QuadMeshComponent::OnUpdate(float)
    {
    }

    void QuadMeshComponent::OnDetach()
    {
    }

    const char* QuadMeshComponent::GetSerializationType()
    {
        return "QuadMesh";
    }

    bool QuadMeshComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                              const hgl::UnorderedMap<EntityID, int32_t>&,
                                              ComponentRecord& out_record)
    {
        auto quad_mesh = std::dynamic_pointer_cast<QuadMeshComponent>(component);
        if (!quad_mesh)
            return false;

        QuadMeshRecord data{};
        data.size = quad_mesh->GetSize();
        data.pivot = quad_mesh->GetPivot();
        data.uv_rect = quad_mesh->GetUVRect();
        data.front_face = static_cast<uint32_t>(quad_mesh->GetFrontFace());

        out_record.type = GetSerializationType();
        out_record.payload = data;
        return true;
    }

    void QuadMeshComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                  Entity* entity,
                                                  std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&)
    {
        const auto& data = std::any_cast<const QuadMeshRecord&>(record.payload);
        auto quad_mesh = std::make_shared<QuadMeshComponent>();
        quad_mesh->SetSize(data.size.x, data.size.y);
        quad_mesh->SetPivot(data.pivot.x, data.pivot.y);
        quad_mesh->SetUVRect(data.uv_rect.x, data.uv_rect.y, data.uv_rect.z, data.uv_rect.w);
        quad_mesh->SetFrontFace(static_cast<VkFrontFace>(data.front_face));
        entity->AddComponentInstance(quad_mesh);
    }
}//namespace hgl::ecs
