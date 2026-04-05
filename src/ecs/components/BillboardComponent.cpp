#include<hgl/ecs/components/BillboardComponent.h>
#include<hgl/ecs/components/QuadComponent.h>
#include<hgl/ecs/components/FacingTransformComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/ComponentRecords.h>

namespace hgl::ecs
{
    // Helper to get or create a component
    template<typename T>
    T* GetOrAddComponentHelper(Entity* owner)
    {
        if (!owner) return nullptr;
        auto comp = owner->GetComponent<T>();
        if (!comp)
            comp = owner->AddComponent<T>();
        return comp.get();
    }

    // Convenience API - delegates to QuadComponent
    void BillboardComponent::SetSize(uint32_t width, uint32_t height)
    {
        auto* owner = GetOwner();
        if (!quad)
            quad = GetOrAddComponentHelper<QuadComponent>(owner);

        if (quad)
            quad->SetPixelSize(width, height);
    }

    void BillboardComponent::SetPixelSize(uint32_t width, uint32_t height)
    {
        auto* owner = GetOwner();
        if (!quad)
            quad = GetOrAddComponentHelper<QuadComponent>(owner);

        if (quad)
            quad->SetPixelSize(width, height);
    }

    void BillboardComponent::SetWorldSize(float width, float height)
    {
        auto* owner = GetOwner();
        if (!quad)
            quad = GetOrAddComponentHelper<QuadComponent>(owner);

        if (quad)
            quad->SetWorldSize(width, height);
    }

    bool BillboardComponent::IsFixedPixelSize() const
    {
        return quad ? quad->IsFixedPixelSize() : true;
    }

    void BillboardComponent::SetFixedPixelSize(bool fixed)
    {
        auto* owner = const_cast<BillboardComponent*>(this)->GetOwner();
        if (!quad)
            quad = GetOrAddComponentHelper<QuadComponent>(owner);

        if (quad)
            quad->SetFixedPixelSize(fixed);
    }

    void BillboardComponent::SetTexture(const hgl::OSString& path)
    {
        auto* owner = GetOwner();
        if (!quad)
            quad = GetOrAddComponentHelper<QuadComponent>(owner);

        if (quad)
            quad->SetTexturePath(path);
    }

    const hgl::OSString& BillboardComponent::GetTexturePath() const
    {
        static const hgl::OSString empty;
        return quad ? quad->GetTexturePath() : empty;
    }

    void BillboardComponent::SetDomainTag(const std::string& tag)
    {
        auto* owner = GetOwner();
        if (!quad)
            quad = GetOrAddComponentHelper<QuadComponent>(owner);

        if (quad)
            quad->SetDomainTag(tag);
    }

    const std::string& BillboardComponent::GetDomainTag() const
    {
        static const std::string empty;
        return quad ? quad->GetDomainTag() : empty;
    }

    void BillboardComponent::SetFrontFace(VkFrontFace face)
    {
        auto* owner = GetOwner();
        if (!quad)
            quad = GetOrAddComponentHelper<QuadComponent>(owner);

        if (quad)
            quad->SetFrontFace(face);
    }

    VkFrontFace BillboardComponent::GetFrontFace() const
    {
        return quad ? quad->GetFrontFace() : VK_FRONT_FACE_CLOCKWISE;
    }

    // Convenience API - delegates to FacingTransformComponent
    void BillboardComponent::SetFacingMode(FacingMode mode)
    {
        auto* owner = GetOwner();
        if (!facing)
            facing = GetOrAddComponentHelper<FacingTransformComponent>(owner);

        if (facing)
            facing->SetFacingMode(mode);
    }

    FacingMode BillboardComponent::GetFacingMode() const
    {
        return facing ? facing->GetFacingMode() : FacingMode::LookAtCamera;
    }

    void BillboardComponent::SetTargetPosition(const glm::vec3& pos)
    {
        auto* owner = GetOwner();
        if (!facing)
            facing = GetOrAddComponentHelper<FacingTransformComponent>(owner);

        if (facing)
            facing->SetTargetPosition(pos);
    }

    const glm::vec3& BillboardComponent::GetTargetPosition() const
    {
        static const glm::vec3 origin(0.0f);
        return facing ? facing->GetTargetPosition() : origin;
    }

    void BillboardComponent::SetVisible(bool visible)
    {
        auto* owner = GetOwner();
        if (!quad)
            quad = GetOrAddComponentHelper<QuadComponent>(owner);

        if (quad)
            quad->SetVisible(visible);
    }

    bool BillboardComponent::IsVisible() const
    {
        return quad ? quad->IsVisible() : false;
    }

    void BillboardComponent::OnAttach()
    {
        // When attached to entity, automatically create sub-components
        auto* owner = GetOwner();
        if (owner)
        {
            quad = GetOrAddComponentHelper<QuadComponent>(owner);
            facing = GetOrAddComponentHelper<FacingTransformComponent>(owner);
        }

        Component::OnAttach();
    }

    void BillboardComponent::OnUpdate(float deltaTime)
    {
        // Nothing to do here - sub-components are handled by their own systems
        Component::OnUpdate(deltaTime);
    }

    void BillboardComponent::OnDetach()
    {
        // Don't remove sub-components - they might be used by other systems
        Component::OnDetach();
    }

    const char* BillboardComponent::GetSerializationType()
    {
        return "Billboard";
    }

    bool BillboardComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                               const hgl::UnorderedMap<EntityID, int32_t>& entity_index,
                                               ComponentRecord& out_record)
    {
        // Delegate to QuadComponent and FacingTransformComponent serialization
        // For now, just return true as placeholders
        return true;
    }

    void BillboardComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                   Entity* entity,
                                                   std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents)
    {
        // TODO: Implement proper deserialization
    }
}//namespace hgl::ecs
