#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/ecs/core/EntityHandle.h>
#include<hgl/common/TextureSamplerTypeDef.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/type/String.h>
#include<vector>

namespace hgl::ecs
{
    class ECSContext;

    struct TextureBindingTask
    {
        EntityID                        entity_id = EntityID::Invalid();
        graph::mtl::SamplerSlot         slot = graph::mtl::SamplerSlot::BaseColor;
        graph::mtl::TextureSourceMode   source_mode = graph::mtl::TextureSourceMode::Simple;
        graph::TextureChannelHint       channel_hint = graph::TextureChannelHint::RGBA;
        hgl::OSString                   texture_path;
        std::string                     domain_tag;

        bool HasDomainTag() const { return !domain_tag.empty(); }
    };

    class TextureMaterialBindingSystem : public System
    {
    private:

        ECSContext *world = nullptr;
        std::vector<TextureBindingTask> pending_tasks;

    public:

        TextureMaterialBindingSystem(const std::string& name = "TextureMaterialBindingSystem");
        ~TextureMaterialBindingSystem() override = default;

    public:

        void SetWorld(ECSContext *w) { world = w; }

        void SubmitTextureBindingTask(const TextureBindingTask &task);
        bool SubmitTextureBindingRequest(EntityID entity_id,
                                         const hgl::OSString &texture_path,
                                         const std::string &domain_tag = std::string(),
                                         graph::mtl::SamplerSlot slot = graph::mtl::SamplerSlot::BaseColor,
                                         graph::mtl::TextureSourceMode source_mode = graph::mtl::TextureSourceMode::Simple,
                                         graph::TextureChannelHint channel_hint = graph::TextureChannelHint::RGBA);

        void Update(float deltaTime) override;

    private:

        bool EnsurePrimitiveTextureBinding(class PrimitiveComponent *primitive,
                                           const TextureBindingTask &task);
    };
}//namespace hgl::ecs
