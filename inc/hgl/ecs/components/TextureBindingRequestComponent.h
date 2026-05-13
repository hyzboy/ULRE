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
     * TextureBindingRequestComponent
     *
     * One-shot request for binding a texture resource to a primitive slot.
     * The request is consumed by TextureMaterialBindingSystem and then removed.
     */
    class TextureBindingRequestComponent : public Component
    {
    private:

        graph::mtl::SamplerSlot         slot         = graph::mtl::SamplerSlot::BaseColor;
        graph::mtl::TextureSourceMode   source_mode  = graph::mtl::TextureSourceMode::Simple;
        graph::TextureChannelHint       channel_hint = graph::TextureChannelHint::RGBA;

        hgl::OSString                   texture_path;
        std::string                     domain_tag;

        bool                            enabled       = true;

    public:

        explicit TextureBindingRequestComponent(const std::string &name = "TextureBindingRequest")
            : Component(name)
        {
        }

        graph::mtl::SamplerSlot GetSamplerSlot() const       { return slot; }
        void SetSamplerSlot(graph::mtl::SamplerSlot s)
        {
            slot = s;
        }

        graph::mtl::TextureSourceMode GetTextureSourceMode() const     { return source_mode; }
        void SetTextureSourceMode(graph::mtl::TextureSourceMode m)
        {
            source_mode = m;
        }

        graph::TextureChannelHint GetChannelHint() const               { return channel_hint; }
        void SetChannelHint(graph::TextureChannelHint h)
        {
            channel_hint = h;
        }

        const hgl::OSString& GetTexturePath() const                    { return texture_path; }
        void SetTexturePath(const hgl::OSString& path)
        {
            texture_path = path;
        }

        const std::string& GetDomainTag() const                        { return domain_tag; }
        bool HasDomainTag() const                                      { return !domain_tag.empty(); }
        void SetDomainTag(const std::string& tag)
        {
            domain_tag = tag;
        }

        bool IsEnabled() const       { return enabled; }
        void SetEnabled(bool e)      { enabled = e; }

    public:

        void OnAttach() override {}
        void OnUpdate(float deltaTime) override {}
        void OnDetach() override {}

        static const char* GetSerializationType();

        static bool SerializeToRecord(const std::shared_ptr<Component>& component,
                                      const hgl::UnorderedMap<EntityID, int32_t>& entity_index_map,
                                      ComponentRecord& out_record);

        static void DeserializeFromRecord(const ComponentRecord& record,
                                          Entity* entity,
                                          std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& deferred_parents);
    };

    using TextureBindingComponent = TextureBindingRequestComponent;

}//namespace hgl::ecs
