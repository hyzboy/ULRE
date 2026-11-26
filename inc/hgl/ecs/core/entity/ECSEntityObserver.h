#pragma once

#include <hgl/ecs/support/storage/ComponentTypeTable.h>
#include <cstdint>

namespace hgl::ecs
{
    class Entity;

    /**
     * IECSEntityObserver —— 实体与组件生命周期同步契约。
     *
     * 解耦 Entity 对具体上下文（特别是图形/材质/GPU表）的直接依赖。
     */
    class IECSEntityObserver
    {
    public:
        virtual ~IECSEntityObserver() = default;

        /// 实体行表反激活（实体已死亡）
        virtual void OnEntityDeactivated(EntityID id) = 0;

        /// 实体行表随挂载/卸载刷新
        virtual void OnEntityRowRefreshed(Entity *entity) = 0;

        /// 组件行销毁通知
        virtual void OnComponentRowDestroyed(ComponentType type, uint32_t row) = 0;

        /// 场景结构脏标记（增删实体/组件时触发）
        virtual void OnSceneStructureDirty() = 0;

        /// 实体即将销毁通知（清理材质旁表与反激活 GPU 实体行）
        virtual void OnEntityDestroyed(EntityID id) = 0;
    };
}
