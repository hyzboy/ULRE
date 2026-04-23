#pragma once

#include<hgl/ecs/components/PrimitiveComponent.h>
#include<glm/glm.hpp>
#include<hgl/math/VectorTypes.h>
#include<hgl/type/String.h>
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
     * Sprite2DComponent - Sprite2D rendering component
     *
     * A component for rendering a 2D sprite with a unit-square geometry.
     * Supports pixel-fixed or world-space sizing, pivot, rotation, and tint.
     *
     * Features:
     * - Extends PrimitiveComponent for rendering
     * - Manages sprite size (fixed pixel size or world space)
     * - Supports pivot offset, rotation, and RGBA tint
     * - Tracks front face orientation
     * - Supports optional texture binding
     *
     * Step 2's shared unit-square geometry (vec2 Position + vec2 TexCoord)
     * is assigned by Sprite2DMaterialBindingSystem.
     */
    class Sprite2DComponent : public PrimitiveComponent
    {
    private:

        bool                fixed_size;         ///< If true, use pixel_size; otherwise world space
        hgl::math::Vector2u pixel_size;         ///< Size in pixels (when fixed_size is true)
        glm::vec2           world_size;         ///< Size in world units (when fixed_size is false)
        glm::vec2           pivot;              ///< Normalized pivot offset [0,1], default 0.5,0.5
        float               rotation;           ///< Rotation in radians
        glm::u8vec4         tint;               ///< RGBA tint, default {255,255,255,255}
        VkFrontFace         front_face;         ///< Face direction (CCW or CW)
        hgl::OSString       texture_path;       ///< Optional texture path (system loads/binds)
        hgl::OSString       applied_texture;    ///< Last applied texture path
        bool                texture_dirty;      ///< Texture path changed
        std::string         domain_tag;         ///< Domain tag for texture array batching (empty = legacy single-texture path)
        class hgl::graph::Texture2D* texture;  ///< Cached texture (optional)
        class hgl::graph::Sampler*   sampler;  ///< Cached sampler (optional)

    public:

        explicit Sprite2DComponent(const std::string& name = "Sprite2D")
            : PrimitiveComponent(name)
            , fixed_size(true)
            , pixel_size(256, 256)
            , world_size(1.0f, 1.0f)
            , pivot(0.5f, 0.5f)
            , rotation(0.0f)
            , tint(255, 255, 255, 255)
            , front_face(VK_FRONT_FACE_CLOCKWISE)
            , texture_dirty(false)
            , texture(nullptr)
            , sampler(nullptr)
        {
        }

        virtual ~Sprite2DComponent() = default;

    public:

        // ─── Size management ───────────────────────────────────────────────

        void SetFixedSize(bool v)   { fixed_size = v; }
        bool IsFixedSize() const    { return fixed_size; }

        void SetPixelSize(uint32_t width, uint32_t height);
        void SetPixelSize(const hgl::math::Vector2u& size);
        const hgl::math::Vector2u& GetPixelSize() const { return pixel_size; }

        void SetWorldSize(float width, float height);
        void SetWorldSize(const glm::vec2& size);
        const glm::vec2& GetWorldSize() const { return world_size; }

        // ─── Pivot / Rotation / Tint ───────────────────────────────────────

        void SetPivot(const glm::vec2& p)       { pivot = p; }
        const glm::vec2& GetPivot() const        { return pivot; }

        void SetRotation(float radians)          { rotation = radians; }
        float GetRotation() const                { return rotation; }

        void SetTint(const glm::u8vec4& c)       { tint = c; }
        const glm::u8vec4& GetTint() const       { return tint; }

        // ─── Front face ────────────────────────────────────────────────────

        void SetFrontFace(VkFrontFace face)      { front_face = face; }
        VkFrontFace GetFrontFace() const         { return front_face; }

        // ─── Texture ───────────────────────────────────────────────────────

        void SetTexturePath(const hgl::OSString& path) { texture_path = path; texture_dirty = true; }
        const hgl::OSString& GetTexturePath() const    { return texture_path; }
        void SetAppliedTexturePath(const hgl::OSString& path) { applied_texture = path; texture_dirty = false; }
        const hgl::OSString& GetAppliedTexturePath() const    { return applied_texture; }
        bool IsTextureDirty() const  { return texture_dirty; }
        void ClearTextureDirty()     { texture_dirty = false; }

        // ─── Domain tag ────────────────────────────────────────────────────

        void SetDomainTag(const std::string& tag) { domain_tag = tag; }
        const std::string& GetDomainTag() const   { return domain_tag; }
        bool HasDomainTag() const                  { return !domain_tag.empty(); }

        void SetTextureObjects(hgl::graph::Texture2D* tex, hgl::graph::Sampler* samp)
        {
            texture = tex;
            sampler = samp;
        }
        hgl::graph::Texture2D* GetTexture() const { return texture; }
        hgl::graph::Sampler*   GetSampler() const { return sampler; }

    public:

        // Component lifecycle
        void OnAttach()  override;
        void OnUpdate(float deltaTime) override;
        void OnDetach()  override;

        static const char* GetSerializationType();
        static bool SerializeToRecord(const std::shared_ptr<Component>& component,
                                      const hgl::UnorderedMap<EntityID, int32_t>& entity_index,
                                      ComponentRecord& out_record);
        static void DeserializeFromRecord(const ComponentRecord& record,
                                          Entity* entity,
                                          std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents);

        friend class Sprite2DMaterialBindingSystem;
    };
}//namespace hgl::ecs
