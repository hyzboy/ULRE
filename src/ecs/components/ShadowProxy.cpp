#include<hgl/ecs/components/ShadowProxy.h>
// 2026-10-10（审计 §1.1）：本文件用 `Entity::GetComponent<T>()`，其**模板定义在 Context.h 尾部**
//（Entity.h 只声明）。此前该实例由 Context.cpp 的 `ECSContext::GetShadowProxy` 顺带实例化 —— 那四个
// 零调用者重载删除后，本 TU 必须自己看得见定义（与同目录 GeometryData.cpp / MaterialData.cpp 同规矩）。
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>

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
