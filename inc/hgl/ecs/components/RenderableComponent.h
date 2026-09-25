#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/ecs/components/ShadowComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<glm/glm.hpp>
#include<memory>
#include <hgl/type/UnorderedMap.h>
#include<utility>
#include<vector>

namespace hgl::ecs
{
    class TransformComponent;

    /**
    * Base renderable component interface
    * Derived classes should implement specific rendering needs
    */
    class RenderableComponent : public Component
    {
    protected:

        bool visible;
        float boundingRadius; // Simple bounding sphere for frustum culling

        // 弱引用缓存：同 Entity 上的 ShadowComponent（若有）
        ShadowComponent* cached_shadow_component = nullptr;

    public:

        explicit RenderableComponent(const std::string& name = "Renderable")
            : Component(name)
            , visible(true)
            , boundingRadius(1.0f)
            , cached_shadow_component(nullptr)
        {
        }

        virtual ~RenderableComponent() = default;

        void OnAttach() override
        {
            Component::OnAttach();
            if (auto *e = GetOwner())
            {
                if (auto shadow = e->GetComponent<ShadowComponent>())
                {
                    cached_shadow_component = shadow.get();
                }
            }
        }

        void OnDetach() override
        {
            cached_shadow_component = nullptr;
            Component::OnDetach();
        }

        bool IsVisible() const { return visible; }
        void SetVisible(bool v) { visible = v; }

        float GetBoundingRadius() const { return boundingRadius; }
        void SetBoundingRadius(float radius) { boundingRadius = radius; }

        // ── 阴影关联与缺省约定 ──

        void SetCachedShadowComponent(ShadowComponent *sc) { cached_shadow_component = sc; }

        ShadowComponent *GetShadowComponent() const
        {
            if (!cached_shadow_component)
            {
                if (auto *e = GetOwner())
                {
                    if (auto shadow = e->GetComponent<ShadowComponent>())
                        const_cast<RenderableComponent*>(this)->cached_shadow_component = shadow.get();
                }
            }
            return cached_shadow_component;
        }

        /// 缺省约定：未挂载 ShadowComponent 默认投射阴影；挂载则遵循组件配置
        bool CanCastShadow() const
        {
            auto *sc = GetShadowComponent();
            return sc ? sc->CanCastShadow() : true;
        }

        /// 缺省约定：未挂载 ShadowComponent 默认不限制投射距离（0.0f）
        float GetShadowMaxDistance() const
        {
            auto *sc = GetShadowComponent();
            return sc ? sc->GetMaxDistance() : 0.0f;
        }

        /// 缺省约定：未挂载 ShadowComponent 默认接收阴影
        bool CanReceiveShadow() const
        {
            auto *sc = GetShadowComponent();
            return sc ? sc->CanReceiveShadow() : true;
        }

        float GetShadowBiasMultiplier() const
        {
            auto *sc = GetShadowComponent();
            return sc ? sc->GetBiasMultiplier() : 1.0f;
        }
    };
}//namespace hgl::ecs


