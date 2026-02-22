#pragma once

#include<hgl/ecs/core/Component.h>
#include<glm/glm.hpp>
#include<hgl/type/String.h>
#include<hgl/type/UnorderedMap.h>
#include<vulkan/vulkan.h>

namespace hgl::ecs
{
    class QuadComponent;
    class FacingTransformComponent;
    class TransformComponent;
    class Entity;
    enum class FacingMode : uint8_t;
    struct ComponentRecord;

    /**
     * BillboardComponent - Convenience wrapper for quad + facing transform
     *
     * This component is a DECORATOR pattern that internally manages:
     * - QuadComponent: handles quad geometry and texturing
     * - FacingTransformComponent: handles camera-facing rotation
     *
     * When attached to an entity, it automatically creates both sub-components.
     * This provides a convenient API for the common "billboard" use case while
     * allowing users to also use QuadComponent and FacingTransformComponent
     * separately for more specialized scenarios.
     *
     * Internally Composed Of:
     * - QuadComponent (for rendering rectangular quads)
     * - FacingTransformComponent (for camera-facing rotation)
     *
     * Usage Example (Simple):
     *     auto billboard = entity->AddComponent<BillboardComponent>();
     *     billboard->SetSize(256, 256);
     *     billboard->SetTexture(OS_TEXT("res/sprite.Tex2D"));
     *     // Automatically faces camera each frame
     *
     * Usage Example (Advanced):
     *     // Or create components separately for more control
     *     auto quad = entity->AddComponent<QuadComponent>();
     *     auto facing = entity->AddComponent<FacingTransformComponent>();
     *     facing->SetFacingMode(FacingMode::LookAtTarget);
     */
    class BillboardComponent : public Component
    {
    private:

        QuadComponent* quad = nullptr;
        FacingTransformComponent* facing = nullptr;

    public:

        explicit BillboardComponent(const std::string& name = "Billboard")
            : Component(name)
            , quad(nullptr)
            , facing(nullptr)
        {
        }

        virtual ~BillboardComponent() = default;

        const char* GetRenderSystemGroupName() const override { return "Billboard"; }

    public:

        // Access underlying components
        QuadComponent* GetQuadComponent() const { return quad; }
        FacingTransformComponent* GetFacingComponent() const { return facing; }

    public:

        // Convenience API (delegates to QuadComponent)
        void SetSize(uint32_t width, uint32_t height);
        void SetPixelSize(uint32_t width, uint32_t height);
        void SetWorldSize(float width, float height);
        bool IsFixedPixelSize() const;
        void SetFixedPixelSize(bool fixed);

        void SetTexture(const hgl::OSString& path);
        const hgl::OSString& GetTexturePath() const;

        void SetFrontFace(VkFrontFace face);
        VkFrontFace GetFrontFace() const;

        // Convenience API (delegates to FacingTransformComponent)
        void SetFacingMode(FacingMode mode);
        FacingMode GetFacingMode() const;
        void SetTargetPosition(const glm::vec3& pos);
        const glm::vec3& GetTargetPosition() const;

        // Visibility control (delegates to QuadComponent)
        void SetVisible(bool visible);
        bool IsVisible() const;

    public:

        // Component lifecycle
        void OnAttach() override;
        void OnUpdate(float deltaTime) override;
        void OnDetach() override;

        static const char* GetSerializationType();
        static bool SerializeToRecord(const std::shared_ptr<Component>& component,
                                      const hgl::UnorderedMap<EntityID, int32_t>& entity_index,
                                      ComponentRecord& out_record);
        static void DeserializeFromRecord(const ComponentRecord& record,
                                          Entity* entity,
                                          std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents);
    };
}//namespace hgl::ecs
