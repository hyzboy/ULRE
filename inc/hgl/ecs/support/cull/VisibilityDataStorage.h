#pragma once

#include<hgl/ecs/core/entity/EntityHandle.h>
#include <ankerl/unordered_dense.h>
#include<hgl/sync/Mutex.h>

namespace hgl::ecs
{
    class ECSCoreContext;
    class TransformDataStorage;

    /**
     * VisibilityDataStorage - Fast lookup for invisible entities with hierarchical support
     *
     * Maintains a set of invisible entity IDs for O(1) query during rendering.
     * **唯一真值**：由世界通过 `WorldSpatialState::SetEntityVisible` 写入（A5a 起组件级可见性副本已删除）。
     *
     * **极性 = 负向**（集合成员 = 不可见；默认可见）；`IsInvisible` **含祖先链继承**，
     * `IsDirectlyInvisible` 只查自己 —— 后者**不得**冒充前者做可见判定（漏继承 ⇒ 后代被误判可见）。
     * 另两种表示：`EntityGPU` 行的 `Visible` 位 = **扁平派生位**（不继承）；策略事实
     * `StrategyFact::EntityVisible` = 每帧现算含继承。权威链 / 传播规则 / 已知失步见
     * **doc/visibility-authority-chain.md**。
     *
     * Supports hierarchical visibility: if an ancestor is invisible, all descendants are invisible.
     */
    class VisibilityDataStorage
    {
        friend class WorldSpatialState;
    private:
        ankerl::unordered_dense::set<EntityID> invisible_entities;
        mutable hgl::Mutex mutex;
        ECSCoreContext *core_context = nullptr;
        TransformDataStorage *transform_storage = nullptr;

    public:
        VisibilityDataStorage() = default;
        ~VisibilityDataStorage() = default;

        /// Bind core entity lookup and the spatial transform source for ancestor checks.
        void SetSpatialSource(ECSCoreContext *core, TransformDataStorage *transforms)
        {
            core_context = core;
            transform_storage = transforms;
        }

    private:
        /// 世界空间域是唯一写入方。
        void SetInvisible(EntityID entity_id);
        void SetVisible(EntityID entity_id);

    public:
        /// Check if entity is invisible (O(1) lookup)
        /// Also checks ancestor chain for hierarchical visibility
        bool IsInvisible(EntityID entity_id) const;

        /// Query only direct visibility (doesn't check ancestors)
        bool IsDirectlyInvisible(EntityID entity_id) const;

        /// Get count of invisible entities
        size_t GetInvisibleCount() const;

        /// Clear all data
        void Clear();
    };
}//namespace hgl::ecs

