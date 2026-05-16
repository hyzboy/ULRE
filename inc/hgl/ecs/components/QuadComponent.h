#pragma once

#include<hgl/ecs/components/PrimitiveComponent.h>
#include<glm/glm.hpp>
#include<hgl/math/VectorTypes.h>
#include<hgl/type/String.h>
#include<hgl/mtl/SamplerSlot.h>
#include<vulkan/vulkan.h>
#include<string>

namespace hgl::graph
{
    class Texture2D;
    class Sampler;
}

namespace hgl::ecs
{
    /**
     * QuadComponent - Rectangular quad rendering component
     *
     * A simple component for rendering a flat rectangular quad with optional texture.
     * This is the decoupled rendering counterpart to the shape rendering pipeline.
     *
     * Features:
     * - Extends PrimitiveComponent for rendering
     * - Manages quad size (fixed pixel size or world space)
     * - Tracks front face orientation
     * - Supports optional texture binding
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
     *     quad->SetTexturePath(OS_TEXT("res/sprite.Tex2D"));
     */
    class QuadComponent : public PrimitiveComponent
    {
    private:

        bool                fixed_size;         ///< If true, use pixel_size; otherwise world space
        hgl::math::Vector2u pixel_size;         ///< Size in pixels (when fixed_size is true)
        glm::vec2           world_size;         ///< Size in world units (when fixed_size is false)
        VkFrontFace         front_face;         ///< Face direction (CCW or CW)
        hgl::OSString       texture_path;       ///< Optional texture path (system loads/binds)
        hgl::OSString       applied_texture;    ///< Last applied texture path
        bool                texture_dirty;      ///< Texture path changed
        std::string                   domain_tag;           ///< Domain tag for texture array batching (empty = legacy single-texture path)
        graph::mtl::TextureSourceMode texture_source_mode; ///< Texture source mode; Simple = single texture, Array = texture array domain
        class hgl::graph::Texture2D* texture;   ///< Cached texture (optional)
        class hgl::graph::Sampler* sampler;     ///< Cached sampler (optional)

    public:

        explicit QuadComponent(const std::string& name = "Quad")
            : PrimitiveComponent(name)
            , fixed_size(true)
            , pixel_size(256, 256)
            , world_size(1.0f, 1.0f)
            , front_face(VK_FRONT_FACE_CLOCKWISE)
            , texture_dirty(false)
            , texture_source_mode(graph::mtl::TextureSourceMode::Simple)
            , texture(nullptr)
            , sampler(nullptr)
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

        // Texture control (optional)
        void SetTexturePath(const hgl::OSString& path) { texture_path = path; texture_dirty = true; }
        const hgl::OSString& GetTexturePath() const { return texture_path; }
        void SetAppliedTexturePath(const hgl::OSString& path) { applied_texture = path; texture_dirty = false; }
        const hgl::OSString& GetAppliedTexturePath() const { return applied_texture; }
        bool IsTextureDirty() const { return texture_dirty; }
        void ClearTextureDirty() { texture_dirty = false; }

        // Domain tag for texture array batching
        void SetDomainTag(const std::string& tag) { domain_tag = tag; }
        const std::string& GetDomainTag() const { return domain_tag; }
        bool HasDomainTag() const { return !domain_tag.empty(); }

        // Texture source mode (Simple = single texture, Array = texture array domain)
        void SetTextureSourceMode(graph::mtl::TextureSourceMode mode) { texture_source_mode = mode; }
        graph::mtl::TextureSourceMode GetTextureSourceMode() const { return texture_source_mode; }
        bool IsTextureArrayMode() const { return texture_source_mode == graph::mtl::TextureSourceMode::Array; }

        void SetTextureObjects(hgl::graph::Texture2D* tex, hgl::graph::Sampler* samp)
        {
            texture = tex;
            sampler = samp;
        }
        hgl::graph::Texture2D* GetTexture() const { return texture; }
        hgl::graph::Sampler* GetSampler() const { return sampler; }

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
