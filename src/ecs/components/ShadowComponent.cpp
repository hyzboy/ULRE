#include<hgl/ecs/components/ShadowComponent.h>
#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/core/Entity.h>

namespace hgl::ecs
{
    ShadowComponent::ShadowComponent(const std::string &name)
        : Component(name)
        , cast_shadow(true)
        , max_cast_distance(0.0f)
        , receive_shadow(true)
        , bias_multiplier(1.0f)
    {
    }

    void ShadowComponent::OnAttach()
    {
        if (auto *e = GetOwner())
        {
            std::vector<std::shared_ptr<Component>> comps;
            e->GetAllComponents(comps);
            for (auto &c : comps)
            {
                if (auto renderable = std::dynamic_pointer_cast<RenderableComponent>(c))
                {
                    renderable->SetCachedShadowComponent(this);
                }
            }
        }
    }

    void ShadowComponent::OnDetach()
    {
        if (auto *e = GetOwner())
        {
            std::vector<std::shared_ptr<Component>> comps;
            e->GetAllComponents(comps);
            for (auto &c : comps)
            {
                if (auto renderable = std::dynamic_pointer_cast<RenderableComponent>(c))
                {
                    if (renderable->GetShadowComponent() == this)
                        renderable->SetCachedShadowComponent(nullptr);
                }
            }
        }
    }
}//namespace hgl::ecs
