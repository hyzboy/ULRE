#include<hgl/ecs/components/ShadowProxy.h>

namespace hgl::ecs
{
    ShadowProxy *GetShadowProxy(Entity *owner)
    {
        if (!owner)
            return nullptr;

        return owner->GetComponent<ShadowProxy>();
    }

    const ShadowProxy *GetShadowProxy(const Entity *owner)
    {
        if (!owner)
            return nullptr;

        return owner->GetComponent<ShadowProxy>();
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
