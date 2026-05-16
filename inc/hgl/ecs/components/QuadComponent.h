#pragma once

#include<hgl/ecs/components/PrimitiveComponent.h>
#include<glm/glm.hpp>
#include<hgl/math/VectorTypes.h>
#include<vulkan/vulkan.h>
#include<string>

namespace hgl::ecs
{
    /**
     * QuadComponent - Rectangular quad rendering component
     *
     * A simple component for rendering a flat rectangular quad.
     * This is the decoupled rendering counterpart to the shape rendering pipeline.
     *
     * Features:
     * - Extends PrimitiveComponent for rendering
     * - Manages quad size (fixed pixel size or world space)
     * - Tracks front face orientation
     *
     * Use Cases:
     * - Static sprites
     * - Particle quad elements
     * - UI elements
     * - Billboards (when combined with FacingTransformComponent)
     * - Decals
     *
     * Usage Example:
     *     auto quad = entity->AddComponent<QuadComponent>();
     *     quad->SetFixedPixelSize(true);
     *     quad->SetPixelSize(256, 256);
     */
    class QuadComponent : public PrimitiveComponent
    {
    private:

        bool                fixed_size;         ///< If true, use pixel_size; otherwise world space
        hgl::math::Vector2u pixel_size;         ///< Size in pixels (when fixed_size is true)
        glm::vec2           world_size;         ///< Size in world units (when fixed_size is false)
        VkFrontFace         front_face;         ///< Face direction (CCW or CW)

    public:

        explicit QuadComponent(const std::string& name = "Quad")
            : PrimitiveComponent(name)
            , fixed_size(true)
            , pixel_size(256, 256)
            , world_size(1.0f, 1.0f)
            , front_face(VK_FRONT_FACE_CLOCKWISE)
        {
        }

        virtual ~QuadComponent() = default;

    public:

        // Size management
        void SetFixedPixelSize(bool fixed) { fixed_size = fixed; }
        bool IsFixedPixelSize() const { return fixed_size; }

        void SetPixelSize(uint32_t width, uint32_t height);
        void SetPixelSize(const hgl::math::Vector2u& size);
        const hgl::math::Vector2u& GetPixelSize() const { return pixel_size; }

        void SetWorldSize(float width, float height);
        void SetWorldSize(const glm::vec2& size);
        glm::vec2 GetWorldSize() const { return world_size; }

        // Front face orientation
        void SetFrontFace(VkFrontFace face) { front_face = face; }
        VkFrontFace GetFrontFace() const { return front_face; }


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
