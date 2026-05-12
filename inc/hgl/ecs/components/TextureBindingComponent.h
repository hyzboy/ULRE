#pragma once

#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/common/TextureSamplerTypeDef.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/type/String.h>
#include<string>

namespace hgl
{
    namespace graph
    {
        class Texture2D;
        class Sampler;
    }
}

namespace hgl::ecs
{
    /**
     * TextureBindingComponent
     *
     * Stores texture binding parameters for a single texture slot.
     * Decoupled from geometry type (Quad, Billboard, etc.).
     *
     * Dirty semantics:
     *   - texture_dirty is set when texture_path, domain_tag, slot, source_mode,
     *     or channel_hint changes.
     *   - SetAppliedTexturePath() marks the binding as applied and clears dirty.
     */
    class TextureBindingComponent : public Component
    {
    private:

        graph::mtl::SamplerSlot         slot         = graph::mtl::SamplerSlot::BaseColor;
        graph::mtl::TextureSourceMode   source_mode  = graph::mtl::TextureSourceMode::Simple;
        graph::TextureChannelHint       channel_hint = graph::TextureChannelHint::RGBA;

        hgl::OSString                   texture_path;
        hgl::OSString                   applied_texture_path;
        std::string                     domain_tag;

        bool                            texture_dirty = false;
        bool                            enabled       = true;

        // Cached runtime objects (not serialized)
        graph::Texture2D*               texture       = nullptr;
        graph::Sampler*                 sampler       = nullptr;

    public:

        // ── Sampler slot ──────────────────────────────────────────────────────
        graph::mtl::SamplerSlot GetSamplerSlot() const       { return slot; }
        void SetSamplerSlot(graph::mtl::SamplerSlot s)
        {
            if (slot != s) { slot = s; texture_dirty = true; }
        }

        // ── Source mode ───────────────────────────────────────────────────────
        graph::mtl::TextureSourceMode GetTextureSourceMode() const     { return source_mode; }
        void SetTextureSourceMode(graph::mtl::TextureSourceMode m)
        {
            if (source_mode != m) { source_mode = m; texture_dirty = true; }
        }

        // ── Channel hint ──────────────────────────────────────────────────────
        graph::TextureChannelHint GetChannelHint() const               { return channel_hint; }
        void SetChannelHint(graph::TextureChannelHint h)
        {
            if (channel_hint != h) { channel_hint = h; texture_dirty = true; }
        }

        // ── Texture path ──────────────────────────────────────────────────────
        const hgl::OSString& GetTexturePath() const                    { return texture_path; }
        void SetTexturePath(const hgl::OSString& path)
        {
            if (texture_path != path) { texture_path = path; texture_dirty = true; }
        }

        // ── Applied path (set by system after binding) ────────────────────────
        const hgl::OSString& GetAppliedTexturePath() const             { return applied_texture_path; }
        void SetAppliedTexturePath(const hgl::OSString& path)
        {
            applied_texture_path = path;
            texture_dirty        = false;
        }

        // ── Domain tag ────────────────────────────────────────────────────────
        const std::string& GetDomainTag() const                        { return domain_tag; }
        bool HasDomainTag() const                                      { return !domain_tag.empty(); }
        void SetDomainTag(const std::string& tag)
        {
            if (domain_tag != tag) { domain_tag = tag; texture_dirty = true; }
        }

        // ── Dirty flag ────────────────────────────────────────────────────────
        bool IsTextureDirty() const  { return texture_dirty; }
        void SetTextureDirty()       { texture_dirty = true; }
        void ClearTextureDirty()     { texture_dirty = false; }

        // ── Enabled ───────────────────────────────────────────────────────────
        bool IsEnabled() const       { return enabled; }
        void SetEnabled(bool e)      { enabled = e; }

        // ── Cached runtime objects ────────────────────────────────────────────
        graph::Texture2D* GetTexture() const  { return texture; }
        graph::Sampler*   GetSampler() const  { return sampler; }
        void SetTextureObjects(graph::Texture2D* t, graph::Sampler* s)
        {
            texture = t;
            sampler = s;
        }

    public:

        void OnAttach() override;
        void OnUpdate(float deltaTime) override;
        void OnDetach() override;

        // ── Serialization ─────────────────────────────────────────────────────
        static const char* GetSerializationType();

        static bool SerializeToRecord(const std::shared_ptr<Component>& component,
                                      const hgl::UnorderedMap<EntityID, int32_t>& entity_index_map,
                                      ComponentRecord& out_record);

        static void DeserializeFromRecord(const ComponentRecord& record,
                                          Entity* entity,
                                          std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& deferred_parents);
    };

}//namespace hgl::ecs
