#include<hgl/ecs/components/ShadowProxy.h>

namespace hgl::ecs
{
    ShadowProxy::ShadowProxy(const std::string &name)
        : Component(name)
        , cast_shadow(true)
        , max_cast_distance(0.0f)
        , receive_shadow(true)
        , bias_multiplier(1.0f)
    {
    }

    ShadowProxy *GetShadowProxy(Entity *owner)
    {
        if (!owner)
            return nullptr;

        return owner->GetComponent<ShadowProxy>().get();
    }

    const ShadowProxy *GetShadowProxy(const Entity *owner)
    {
        if (!owner)
            return nullptr;

        return owner->GetComponent<ShadowProxy>().get();
    }

    bool CanCastShadow(const Entity *owner)
    {
        const ShadowProxy *proxy = GetShadowProxy(owner);
        return proxy ? proxy->CanCastShadow() : true;
    }

    float GetShadowMaxDistance(const Entity *owner)
    {
        const ShadowProxy *proxy = GetShadowProxy(owner);
        return proxy ? proxy->GetMaxDistance() : 0.0f;
    }

    bool CanReceiveShadow(const Entity *owner)
    {
        const ShadowProxy *proxy = GetShadowProxy(owner);
        return proxy ? proxy->CanReceiveShadow() : true;
    }

    float GetShadowBiasMultiplier(const Entity *owner)
    {
        const ShadowProxy *proxy = GetShadowProxy(owner);
        return proxy ? proxy->GetBiasMultiplier() : 1.0f;
    }
}//namespace hgl::ecs
