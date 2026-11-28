#include<hgl/ecs/components/ModelCollisionProxyComponent.h>
// `Entity::GetComponent<T>()` 的定义在 ECSCoreContext.h 中（Entity.h 只声明），
// 本 TU 必须自己看得见定义。
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/core/ECSCoreContext.h>

namespace hgl::ecs
{
    ModelCollisionProxyComponent *GetModelCollisionProxy(Entity *owner)
    {
        if (!owner)
            return nullptr;

        return owner->GetComponent<ModelCollisionProxyComponent>();
    }

    const ModelCollisionProxyComponent *GetModelCollisionProxy(const Entity *owner)
    {
        if (!owner)
            return nullptr;

        return owner->GetComponent<ModelCollisionProxyComponent>();
    }
}//namespace hgl::ecs
