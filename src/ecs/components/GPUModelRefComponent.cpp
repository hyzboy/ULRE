#include <hgl/ecs/components/GPUModelRefComponent.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    void GPUModelRefComponent::SetTemplateBinding(const uint32_t id, const bool automatic)
    {
        if (template_id == id && automatically_normalized == automatic)
            return;

        template_id = id;
        automatically_normalized = automatic;
        if (auto *context = owner ? owner->GetContextAs<ECSContext>() : nullptr)
        {
            context->RenderData().InvalidateEntityRuntimePipeline(owner->GetEntityID());
            context->MarkTopologyChanged(TopologyChange::ModelTemplate);
        }
    }

    void GPUModelRefComponent::OnDetach()
    {
    }
}
