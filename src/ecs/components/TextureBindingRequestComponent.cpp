#include<hgl/ecs/components/TextureBindingRequestComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/ComponentRecords.h>

namespace hgl::ecs
{
    namespace
    {
        struct TextureBindingRecord
        {
            uint32_t sampler_slot = static_cast<uint32_t>(graph::mtl::SamplerSlot::BaseColor);
            uint32_t source_mode = static_cast<uint32_t>(graph::mtl::TextureSourceMode::Simple);
            uint32_t channel_hint = static_cast<uint32_t>(graph::TextureChannelHint::RGBA);
            hgl::OSString texture_path;
            std::string domain_tag;
            bool enabled = true;
        };
    }

    const char* TextureBindingRequestComponent::GetSerializationType()
    {
        return "TextureBindingRequest";
    }

    bool TextureBindingRequestComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                                           const hgl::UnorderedMap<EntityID, int32_t>&,
                                                           ComponentRecord& out_record)
    {
        auto binding = std::dynamic_pointer_cast<TextureBindingRequestComponent>(component);
        if (!binding)
            return false;

        TextureBindingRecord data{};
        data.sampler_slot = static_cast<uint32_t>(binding->GetSamplerSlot());
        data.source_mode = static_cast<uint32_t>(binding->GetTextureSourceMode());
        data.channel_hint = static_cast<uint32_t>(binding->GetChannelHint());
        data.texture_path = binding->GetTexturePath();
        data.domain_tag = binding->GetDomainTag();
        data.enabled = binding->IsEnabled();

        out_record.type = GetSerializationType();
        out_record.payload = data;
        return true;
    }

    void TextureBindingRequestComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                               Entity* entity,
                                                               std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&)
    {
        const auto& data = std::any_cast<const TextureBindingRecord&>(record.payload);
        auto binding = std::make_shared<TextureBindingRequestComponent>();
        binding->SetSamplerSlot(static_cast<graph::mtl::SamplerSlot>(data.sampler_slot));
        binding->SetTextureSourceMode(static_cast<graph::mtl::TextureSourceMode>(data.source_mode));
        binding->SetChannelHint(static_cast<graph::TextureChannelHint>(data.channel_hint));
        binding->SetTexturePath(data.texture_path);
        binding->SetDomainTag(data.domain_tag);
        binding->SetEnabled(data.enabled);
        entity->AddComponentInstance(binding);
    }
}//namespace hgl::ecs
