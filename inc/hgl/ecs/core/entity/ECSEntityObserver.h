#pragma once

#include <hgl/ecs/core/entity/EntityHandle.h>
#include <hgl/ecs/support/storage/ComponentTypeTable.h>
#include <cstdint>

namespace hgl::ecs
{
    class Entity;

    /**
     * 同步接收实体变化事实；不拥有实体或组件，也不承担其销毁。
     * 行刷新回调中的实体仅在回调期间有效；销毁回调只携带值句柄。
     * 回调不得修改订阅或重入世界的实体/组件生命周期操作。
     * 观察者由调用方拥有，须在其销毁前解除绑定。
     */
    class IECSEntityObserver
    {
    public:
        virtual ~IECSEntityObserver() = default;

        virtual void OnEntityCreated(EntityID id) = 0;
        virtual void OnEntityRowsChanged(const Entity *entity) = 0;
        virtual void OnSceneStructureDirty() = 0;
        virtual void OnEntityDestroyed(EntityID id) = 0;
    };
}
