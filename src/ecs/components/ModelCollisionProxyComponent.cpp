#include<hgl/ecs/components/ModelCollisionProxyComponent.h>
// 与 ShadowProxy.cpp 同规矩：`Entity::GetComponent<T>()` 的定义在 Context.h 尾部（Entity.h 只声明），
// 本 TU 必须自己看得见定义。
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/core/Context.h>

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
